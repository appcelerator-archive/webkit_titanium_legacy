/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "GraphicsLayerQt.h"

#include "CurrentTime.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "RefCounted.h"
#include "TranslateTransformOperation.h"
#include "UnitBezier.h"
#include <QtCore/qabstractanimation.h>
#include <QtCore/qdebug.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qset.h>
#include <QtCore/qtimer.h>
#include <QtGui/qbitmap.h>
#include <QtGui/qcolor.h>
#include <QtGui/qgraphicseffect.h>
#include <QtGui/qgraphicsitem.h>
#include <QtGui/qgraphicsscene.h>
#include <QtGui/qmatrix4x4.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpalette.h>
#include <QtGui/qpixmap.h>
#include <QtGui/qstyleoption.h>

namespace WebCore {

class MaskEffectQt : public QGraphicsEffect {
public:
    MaskEffectQt(QObject* parent, QGraphicsItem* maskLayer)
            : QGraphicsEffect(parent)
            , m_maskLayer(maskLayer)
    {
    }

    void draw(QPainter* painter)
    {
        // this is a modified clone of QGraphicsOpacityEffect.
        // It's more efficient to do it this way because
        // (a) we don't need the QBrush abstraction - we always end up using QGraphicsItem::paint from the mask layer
        // (b) QGraphicsOpacityEffect detaches the pixmap, which is inefficient on OpenGL.
        QPixmap maskPixmap(sourceBoundingRect().toAlignedRect().size());

        // we need to do this so the pixmap would have hasAlpha()
        maskPixmap.fill(Qt::transparent);
        QPainter maskPainter(&maskPixmap);
        QStyleOptionGraphicsItem option;
        option.exposedRect = option.rect = maskPixmap.rect();
        maskPainter.setRenderHints(painter->renderHints(), true);
        m_maskLayer->paint(&maskPainter, &option, 0);
        maskPainter.end();
        QPoint offset;
        QPixmap srcPixmap = sourcePixmap(Qt::LogicalCoordinates, &offset, QGraphicsEffect::NoPad);

        // we have to use another intermediate pixmap, to make sure the mask applies only to this item
        // and doesn't modify pixels already painted into this paint-device
        QPixmap pixmap(srcPixmap.size());
        pixmap.fill(Qt::transparent);

        if (pixmap.isNull())
            return;

        QPainter pixmapPainter(&pixmap);
        pixmapPainter.setRenderHints(painter->renderHints());
        pixmapPainter.setCompositionMode(QPainter::CompositionMode_Source);
        // We use drawPixmap rather than detaching, because it's more efficient on OpenGL
        pixmapPainter.drawPixmap(0, 0, srcPixmap);
        pixmapPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        pixmapPainter.drawPixmap(0, 0, maskPixmap);
        pixmapPainter.end();
        painter->drawPixmap(offset, pixmap);
    }

    QGraphicsItem* m_maskLayer;
};

class GraphicsLayerQtImpl : public QGraphicsObject {
    Q_OBJECT

public:
    // this set of flags help us defer which properties of the layer have been
    // modified by the compositor, so we can know what to look for in the next flush
    enum ChangeMask {
        NoChanges =                 0,

        ParentChange =              (1L << 0),
        ChildrenChange =            (1L << 1),
        MaskLayerChange =           (1L << 2),
        PositionChange =            (1L << 3),

        AnchorPointChange =         (1L << 4),
        SizeChange  =               (1L << 5),
        TransformChange =           (1L << 6),
        ContentChange =             (1L << 7),

        GeometryOrientationChange = (1L << 8),
        ContentsOrientationChange = (1L << 9),
        OpacityChange =             (1L << 10),
        ContentsRectChange =        (1L << 11),

        Preserves3DChange =         (1L << 12),
        MasksToBoundsChange =       (1L << 13),
        DrawsContentChange =        (1L << 14),
        ContentsOpaqueChange =      (1L << 15),

        BackfaceVisibilityChange =  (1L << 16),
        ChildrenTransformChange =   (1L << 17),
        DisplayChange =             (1L << 18),
        BackgroundColorChange =     (1L << 19),

        DistributesOpacityChange =  (1L << 20)
    };

    // the compositor lets us special-case images and colors, so we try to do so
    enum StaticContentType { HTMLContentType, PixmapContentType, ColorContentType, MediaContentType};

    GraphicsLayerQtImpl(GraphicsLayerQt* newLayer);
    virtual ~GraphicsLayerQtImpl();

    // reimps from QGraphicsItem
    virtual QPainterPath opaqueArea() const;
    virtual QRectF boundingRect() const;
    virtual void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*);

    // we manage transforms ourselves because transform-origin acts differently in webkit and in Qt
    void setBaseTransform(const TransformationMatrix&);
    QTransform computeTransform(const TransformationMatrix& baseTransform) const;
    void updateTransform();

    // let the compositor-API tell us which properties were changed
    void notifyChange(ChangeMask);

    // called when the compositor is ready for us to show the changes on screen
    // this is called indirectly from ChromeClientQt::setNeedsOneShotDrawingSynchronization
    // (meaning the sync would happen together with the next draw)
    // or ChromeClientQt::scheduleCompositingLayerSync (meaning the sync will happen ASAP)
    void flushChanges(bool recursive = true, bool forceTransformUpdate = false);

    // optimization: when we have an animation running on an element with no contents, that has child-elements with contents,
    // ALL of them have to have ItemCoordinateCache and not DeviceCoordinateCache
    void adjustCachingRecursively(bool animationIsRunning);

    // optimization: returns true if this or an ancestor has a transform animation running.
    // this enables us to use ItemCoordinatesCache while the animation is running, otherwise we have to recache for every frame
    bool isTransformAnimationRunning() const;

public slots:
    // we need to notify the client (aka the layer compositor) when the animation actually starts
    void notifyAnimationStarted();

    // we notify WebCore of a layer changed asynchronously; otherwise we end up calling flushChanges too often.
    void notifySyncRequired();

signals:
    // optimization: we don't want to use QTimer::singleShot
    void notifyAnimationStartedAsync();

public:
    GraphicsLayerQt* m_layer;

    TransformationMatrix m_baseTransform;
    bool m_transformAnimationRunning;
    bool m_opacityAnimationRunning;
    QWeakPointer<MaskEffectQt> m_maskEffect;

    struct ContentData {
        QPixmap pixmap;
        QRegion regionToUpdate;
        bool updateAll;
        QColor contentsBackgroundColor;
        QColor backgroundColor;
        QWeakPointer<QGraphicsObject> mediaLayer;
        StaticContentType contentType;
        float opacity;
        ContentData()
                : updateAll(false)
                , contentType(HTMLContentType)
                , opacity(1.f)
        {
        }

    };

    ContentData m_pendingContent;
    ContentData m_currentContent;

    int m_changeMask;

    QSizeF m_size;
#ifndef QT_NO_ANIMATION
    QList<QWeakPointer<QAbstractAnimation> > m_animations;
#endif
    QTimer m_suspendTimer;

    struct State {
        GraphicsLayer* maskLayer;
        FloatPoint pos;
        FloatPoint3D anchorPoint;
        FloatSize size;
        TransformationMatrix transform;
        TransformationMatrix childrenTransform;
        Color backgroundColor;
        Color currentColor;
        GraphicsLayer::CompositingCoordinatesOrientation geoOrientation;
        GraphicsLayer::CompositingCoordinatesOrientation contentsOrientation;
        float opacity;
        QRect contentsRect;

        bool preserves3D: 1;
        bool masksToBounds: 1;
        bool drawsContent: 1;
        bool contentsOpaque: 1;
        bool backfaceVisibility: 1;
        bool distributeOpacity: 1;
        bool align: 2;
        State(): maskLayer(0), opacity(1.f), preserves3D(false), masksToBounds(false),
                  drawsContent(false), contentsOpaque(false), backfaceVisibility(false),
                  distributeOpacity(false)
        {
        }
    } m_state;

#ifndef QT_NO_ANIMATION
    friend class AnimationQtBase;
#endif
};

GraphicsLayerQtImpl::GraphicsLayerQtImpl(GraphicsLayerQt* newLayer)
    : QGraphicsObject(0)
    , m_layer(newLayer)
    , m_transformAnimationRunning(false)
    , m_opacityAnimationRunning(false)
    , m_changeMask(NoChanges)
{
    // we use graphics-view for compositing, not for interactivity
    setAcceptedMouseButtons(Qt::NoButton);
    // we need to have the item enabled, or else wheel events are not
    // passed to the parent class implementation of wheelEvent, where
    // they are ignored and passed to the item below.
    setEnabled(true);

    // we'll set the cache when we know what's going on
    setCacheMode(NoCache);

    connect(this, SIGNAL(notifyAnimationStartedAsync()), this, SLOT(notifyAnimationStarted()), Qt::QueuedConnection);
}

GraphicsLayerQtImpl::~GraphicsLayerQtImpl()
{
    // the compositor manages item lifecycle - we don't want the graphics-view
    // system to automatically delete our items

    const QList<QGraphicsItem*> children = childItems();
    for (QList<QGraphicsItem*>::const_iterator it = children.begin(); it != children.end(); ++it) {
        if (QGraphicsItem* item = *it) {
            if (scene())
                scene()->removeItem(item);
            item->setParentItem(0);
        }
    }

#ifndef QT_NO_ANIMATION
    // we do, however, own the animations...
    for (QList<QWeakPointer<QAbstractAnimation> >::iterator it = m_animations.begin(); it != m_animations.end(); ++it)
        if (QAbstractAnimation* anim = it->data())
            delete anim;
#endif
}

void GraphicsLayerQtImpl::adjustCachingRecursively(bool animationIsRunning)
{
    // optimization: we make sure all our children have ItemCoordinateCache -
    // otherwise we end up re-rendering them during the animation
    const QList<QGraphicsItem*> children = childItems();

    for (QList<QGraphicsItem*>::const_iterator it = children.begin(); it != children.end(); ++it) {
        if (QGraphicsItem* item = *it)
            if (GraphicsLayerQtImpl* layer = qobject_cast<GraphicsLayerQtImpl*>(item->toGraphicsObject())) {
                if (layer->m_layer->drawsContent() && layer->m_currentContent.contentType == HTMLContentType)
                    layer->setCacheMode(animationIsRunning ? QGraphicsItem::ItemCoordinateCache : QGraphicsItem::DeviceCoordinateCache);
            }
    }    
}

void GraphicsLayerQtImpl::updateTransform()
{
    setBaseTransform(isTransformAnimationRunning() ? m_baseTransform : m_layer->transform());
}

void GraphicsLayerQtImpl::setBaseTransform(const TransformationMatrix& baseTransform)
{
    m_baseTransform = baseTransform;
    setTransform(computeTransform(baseTransform));
}

QTransform GraphicsLayerQtImpl::computeTransform(const TransformationMatrix& baseTransform) const
{
    if (!m_layer)
        return baseTransform;

    TransformationMatrix computedTransform;

    // The origin for childrenTransform is always the center of the ancestor which contains the childrenTransform.
    // this has to do with how WebCore implements -webkit-perspective and -webkit-perspective-origin, which are the CSS
    // attribute that call setChildrenTransform
    QPointF offset = -pos() - boundingRect().bottomRight() / 2;

    for (const GraphicsLayerQtImpl* ancestor = this; (ancestor = qobject_cast<GraphicsLayerQtImpl*>(ancestor->parentObject())); ) {
        if (!ancestor->m_state.childrenTransform.isIdentity()) {
            const QPointF offset = mapFromItem(ancestor, QPointF(ancestor->m_size.width() / 2, ancestor->m_size.height() / 2));
            computedTransform
                .translate(offset.x(), offset.y())
                .multLeft(ancestor->m_state.childrenTransform)
                .translate(-offset.x(), -offset.y());
            break;
        }
    }

    // webkit has relative-to-size originPoint, graphics-view has a pixel originPoint, here we convert
    // we have to manage this ourselves because QGraphicsView's transformOrigin is incompatible
    const qreal originX = m_state.anchorPoint.x() * m_size.width();
    const qreal originY = m_state.anchorPoint.y() * m_size.height();
    computedTransform
            .translate3d(originX, originY, m_state.anchorPoint.z())
            .multLeft(baseTransform)
            .translate3d(-originX, -originY, -m_state.anchorPoint.z());

    // now we project to 2D
    return QTransform(computedTransform);
}

bool GraphicsLayerQtImpl::isTransformAnimationRunning() const
{
    if (m_transformAnimationRunning)
        return true;
    if (GraphicsLayerQtImpl* parent = qobject_cast<GraphicsLayerQtImpl*>(parentObject()))
        return parent->isTransformAnimationRunning();
    return false;
}

QPainterPath GraphicsLayerQtImpl::opaqueArea() const
{
    QPainterPath painterPath;
    // we try out best to return the opaque area, maybe it will help graphics-view render less items
    if (m_currentContent.backgroundColor.isValid() && m_currentContent.backgroundColor.alpha() == 0xff)
        painterPath.addRect(boundingRect());
    else {
        if (m_state.contentsOpaque
            || (m_currentContent.contentType == ColorContentType && m_currentContent.contentsBackgroundColor.alpha() == 0xff)
            || (m_currentContent.contentType == MediaContentType)
            || (m_currentContent.contentType == PixmapContentType && !m_currentContent.pixmap.hasAlpha())) {

            painterPath.addRect(m_state.contentsRect);
        }
    }
    return painterPath;
}

QRectF GraphicsLayerQtImpl::boundingRect() const
{
    return QRectF(QPointF(0, 0), QSizeF(m_size));
}

void GraphicsLayerQtImpl::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    if (m_currentContent.backgroundColor.isValid())
        painter->fillRect(option->exposedRect, QColor(m_currentContent.backgroundColor));

    switch (m_currentContent.contentType) {
    case HTMLContentType:
        if (m_state.drawsContent) {
            // this is the expensive bit. we try to minimize calls to this area by proper caching
            GraphicsContext gc(painter);
            m_layer->paintGraphicsLayerContents(gc, option->exposedRect.toAlignedRect());
        }
        break;
    case PixmapContentType:
        painter->drawPixmap(m_state.contentsRect, m_currentContent.pixmap);
        break;
    case ColorContentType:
        painter->fillRect(m_state.contentsRect, m_currentContent.contentsBackgroundColor);
        break;
    case MediaContentType:
        // we don't need to paint anything: we have a QGraphicsItem from the media element
        break;
    }
}

void GraphicsLayerQtImpl::notifySyncRequired()
{
    if (m_layer->client())
        m_layer->client()->notifySyncRequired(m_layer);
}

void GraphicsLayerQtImpl::notifyChange(ChangeMask changeMask)
{
    m_changeMask |= changeMask;
    static QMetaMethod syncMethod = staticMetaObject.method(staticMetaObject.indexOfMethod("notifySyncRequired()"));
    syncMethod.invoke(this, Qt::QueuedConnection);
}

void GraphicsLayerQtImpl::flushChanges(bool recursive, bool forceUpdateTransform)
{
    // this is the bulk of the work. understanding what the compositor is trying to achieve,
    // what graphics-view can do, and trying to find a sane common-grounds
    if (!m_layer || m_changeMask == NoChanges)
        goto afterLayerChanges;

    if (m_currentContent.contentType == HTMLContentType && (m_changeMask & ParentChange)) {
        // the WebCore compositor manages item ownership. We have to make sure
        // graphics-view doesn't try to snatch that ownership...
        if (!m_layer->parent() && !parentItem())
            setParentItem(0);
        else if (m_layer && m_layer->parent() && m_layer->parent()->nativeLayer() != parentItem())
            setParentItem(m_layer->parent()->nativeLayer());
    }

    if (m_changeMask & ChildrenChange) {
        // we basically do an XOR operation on the list of current children
        // and the list of wanted children, and remove/add
        QSet<QGraphicsItem*> newChildren;
        const Vector<GraphicsLayer*> newChildrenVector = (m_layer->children());
        newChildren.reserve(newChildrenVector.size());

        for (size_t i = 0; i < newChildrenVector.size(); ++i)
            newChildren.insert(newChildrenVector[i]->platformLayer());

        const QSet<QGraphicsItem*> currentChildren = childItems().toSet();
        const QSet<QGraphicsItem*> childrenToAdd = newChildren - currentChildren;
        const QSet<QGraphicsItem*> childrenToRemove = currentChildren - newChildren;

        for (QSet<QGraphicsItem*>::const_iterator it = childrenToAdd.begin(); it != childrenToAdd.end(); ++it)
             if (QGraphicsItem* w = *it)
                w->setParentItem(this);

        for (QSet<QGraphicsItem*>::const_iterator it = childrenToRemove.begin(); it != childrenToRemove.end(); ++it)
             if (GraphicsLayerQtImpl* w = qobject_cast<GraphicsLayerQtImpl*>((*it)->toGraphicsObject()))
                w->setParentItem(0);

        // children are ordered by z-value, let graphics-view know.
        for (size_t i = 0; i < newChildrenVector.size(); ++i)
            if (newChildrenVector[i]->platformLayer())
                newChildrenVector[i]->platformLayer()->setZValue(i);
    }

    if (m_changeMask & MaskLayerChange) {
        // we can't paint here, because we don't know if the mask layer
        // itself is ready... we'll have to wait till this layer tries to paint
        setFlag(ItemClipsChildrenToShape, m_layer->maskLayer() || m_layer->masksToBounds());
        setGraphicsEffect(0);
        if (m_layer->maskLayer()) {
            if (GraphicsLayerQtImpl* mask = qobject_cast<GraphicsLayerQtImpl*>(m_layer->maskLayer()->platformLayer()->toGraphicsObject())) {
                mask->m_maskEffect = new MaskEffectQt(this, mask);
                mask->setCacheMode(NoCache);
                setGraphicsEffect(mask->m_maskEffect.data());
            }
        }
    }

    if ((m_changeMask & PositionChange) && (m_layer->position() != m_state.pos))
        setPos(m_layer->position().x(), m_layer->position().y());

    if (m_changeMask & SizeChange) {
        if (m_layer->size() != m_state.size) {
            prepareGeometryChange();
            m_size = QSizeF(m_layer->size().width(), m_layer->size().height());
        }
    }
    // FIXME: this is a hack, due to a probable QGraphicsScene bug when rapidly modifying the perspective
    // but without this line we get graphic artifacts
    if ((m_changeMask & ChildrenTransformChange) && m_state.childrenTransform != m_layer->childrenTransform())
        scene()->update();

    if (m_changeMask & (ChildrenTransformChange | Preserves3DChange | TransformChange | AnchorPointChange | SizeChange)) {
        // due to the differences between the way WebCore handles transforms and the way Qt handles transforms,
        // all these elements affect the transforms of all the descendants.
        forceUpdateTransform = true;
    }

    if (m_changeMask & (ContentChange | DrawsContentChange | MaskLayerChange)) {
        switch (m_pendingContent.contentType) {
        case PixmapContentType:
            update();
            setFlag(ItemHasNoContents, false);

            break;
        case MediaContentType:
            setFlag(ItemHasNoContents, true);
            m_pendingContent.mediaLayer.data()->setParentItem(this);
            break;

        case ColorContentType:
            // no point in caching a solid-color rectangle
            setCacheMode(m_layer->maskLayer() ? QGraphicsItem::DeviceCoordinateCache : QGraphicsItem::NoCache);
            if (m_pendingContent.contentType != m_currentContent.contentType || m_pendingContent.contentsBackgroundColor != m_currentContent.contentsBackgroundColor)
                update();
            m_state.drawsContent = false;
            setFlag(ItemHasNoContents, false);

            // we only use ItemUsesExtendedStyleOption for HTML content - colors don't gain much from that anyway
            setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, false);
            break;

        case HTMLContentType:
            if (m_pendingContent.contentType != m_currentContent.contentType)
                update();
            if (!m_state.drawsContent && m_layer->drawsContent())
                update();
                if (m_layer->drawsContent() && !m_maskEffect) {
                    setCacheMode(isTransformAnimationRunning() ? ItemCoordinateCache : DeviceCoordinateCache);

                    // HTML content: we want to use exposedRect so we don't use WebCore rendering if we don't have to
                    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
                }
            else
                setCacheMode(NoCache);

            setFlag(ItemHasNoContents, !m_layer->drawsContent());
            break;
        }
    }

    if ((m_changeMask & OpacityChange) && m_state.opacity != m_layer->opacity() && !m_opacityAnimationRunning)
        setOpacity(m_layer->opacity());

    if (m_changeMask & ContentsRectChange) {
        const QRect rect(m_layer->contentsRect());
        if (m_state.contentsRect != rect) {
            m_state.contentsRect = rect;
            update();
        }
    }

    if ((m_changeMask & MasksToBoundsChange)
        && m_state.masksToBounds != m_layer->masksToBounds()) {

        setFlag(QGraphicsItem::ItemClipsToShape, m_layer->masksToBounds());
        setFlag(QGraphicsItem::ItemClipsChildrenToShape, m_layer->masksToBounds());
    }

    if ((m_changeMask & ContentsOpaqueChange) && m_state.contentsOpaque != m_layer->contentsOpaque())
        prepareGeometryChange();

    if (m_maskEffect)
        m_maskEffect.data()->update();
    else if (m_changeMask & DisplayChange)
        update(m_pendingContent.regionToUpdate.boundingRect());

    if ((m_changeMask & BackgroundColorChange) && (m_pendingContent.backgroundColor != m_currentContent.backgroundColor))
        update();

    // FIXME: the following flags are currently not handled, as they don't have a clear test or are in low priority
    // GeometryOrientationChange, ContentsOrientationChange, BackfaceVisibilityChange, ChildrenTransformChange, Preserves3DChange

    m_state.maskLayer = m_layer->maskLayer();
    m_state.pos = m_layer->position();
    m_state.anchorPoint = m_layer->anchorPoint();
    m_state.size = m_layer->size();
    m_state.transform = m_layer->transform();
    m_state.geoOrientation = m_layer->geometryOrientation();
    m_state.contentsOrientation =m_layer->contentsOrientation();
    m_state.opacity = m_layer->opacity();
    m_state.contentsRect = m_layer->contentsRect();
    m_state.preserves3D = m_layer->preserves3D();
    m_state.masksToBounds = m_layer->masksToBounds();
    m_state.drawsContent = m_layer->drawsContent();
    m_state.contentsOpaque = m_layer->contentsOpaque();
    m_state.backfaceVisibility = m_layer->backfaceVisibility();
    m_state.childrenTransform = m_layer->childrenTransform();
    m_currentContent.pixmap = m_pendingContent.pixmap;
    m_currentContent.contentType = m_pendingContent.contentType;
    m_currentContent.mediaLayer = m_pendingContent.mediaLayer;
    m_currentContent.backgroundColor = m_pendingContent.backgroundColor;
    m_currentContent.regionToUpdate |= m_pendingContent.regionToUpdate;
    m_currentContent.contentsBackgroundColor = m_pendingContent.contentsBackgroundColor;
    m_pendingContent.regionToUpdate = QRegion();
    m_changeMask = NoChanges;


afterLayerChanges:
    if (forceUpdateTransform)
        updateTransform();

    if (!recursive)
        return;    

    QList<QGraphicsItem*> children = childItems();
    if (m_state.maskLayer)
        children.append(m_state.maskLayer->platformLayer());

    for (QList<QGraphicsItem*>::const_iterator it = children.begin(); it != children.end(); ++it) {
        if (QGraphicsItem* item = *it)
            if (GraphicsLayerQtImpl* layer = qobject_cast<GraphicsLayerQtImpl*>(item->toGraphicsObject()))
                layer->flushChanges(true, forceUpdateTransform);
    }
}

void GraphicsLayerQtImpl::notifyAnimationStarted()
{
    // WebCore notifies javascript when the animation starts
    // here we're letting it know
    m_layer->client()->notifyAnimationStarted(m_layer, WTF::currentTime());
}

GraphicsLayerQt::GraphicsLayerQt(GraphicsLayerClient* client)
    : GraphicsLayer(client)
    , m_impl(PassOwnPtr<GraphicsLayerQtImpl>(new GraphicsLayerQtImpl(this)))
{
}

GraphicsLayerQt::~GraphicsLayerQt()
{
}

// this is the hook for WebCore compositor to know that Qt implements compositing with GraphicsLayerQt
PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerClient* client)
{
    return new GraphicsLayerQt(client);
}

// reimp from GraphicsLayer.h: Qt is top-down
GraphicsLayer::CompositingCoordinatesOrientation GraphicsLayer::compositingCoordinatesOrientation()
{
    return CompositingCoordinatesTopDown;
}

// reimp from GraphicsLayer.h: we'll need to update the whole display, and we can't count on the current size because it might change
void GraphicsLayerQt::setNeedsDisplay()
{
    m_impl->m_pendingContent.regionToUpdate = QRegion(QRect(QPoint(0, 0), QSize(size().width(), size().height())));
    m_impl->notifyChange(GraphicsLayerQtImpl::DisplayChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setNeedsDisplayInRect(const FloatRect& rect)
{
    m_impl->m_pendingContent.regionToUpdate|= QRectF(rect).toAlignedRect();
    m_impl->notifyChange(GraphicsLayerQtImpl::DisplayChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setName(const String& name)
{
    m_impl->setObjectName(name);
    GraphicsLayer::setName(name);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setParent(GraphicsLayer* layer)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::ParentChange);
    GraphicsLayer::setParent(layer);
}

// reimp from GraphicsLayer.h
bool GraphicsLayerQt::setChildren(const Vector<GraphicsLayer*>& children)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::ChildrenChange);
    return GraphicsLayer::setChildren(children);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::addChild(GraphicsLayer* layer)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::ChildrenChange);
    GraphicsLayer::addChild(layer);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::addChildAtIndex(GraphicsLayer* layer, int index)
{
    GraphicsLayer::addChildAtIndex(layer, index);
    m_impl->notifyChange(GraphicsLayerQtImpl::ChildrenChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::addChildAbove(GraphicsLayer* layer, GraphicsLayer* sibling)
{
     GraphicsLayer::addChildAbove(layer, sibling);
     m_impl->notifyChange(GraphicsLayerQtImpl::ChildrenChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::addChildBelow(GraphicsLayer* layer, GraphicsLayer* sibling)
{

    GraphicsLayer::addChildBelow(layer, sibling);
    m_impl->notifyChange(GraphicsLayerQtImpl::ChildrenChange);
}

// reimp from GraphicsLayer.h
bool GraphicsLayerQt::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    if (GraphicsLayer::replaceChild(oldChild, newChild)) {
        m_impl->notifyChange(GraphicsLayerQtImpl::ChildrenChange);
        return true;
    }

    return false;
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::removeFromParent()
{
    if (parent())
        m_impl->notifyChange(GraphicsLayerQtImpl::ParentChange);
    GraphicsLayer::removeFromParent();
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setMaskLayer(GraphicsLayer* value)
{
    if (value == maskLayer())
        return;
    GraphicsLayer::setMaskLayer(value);
    m_impl->notifyChange(GraphicsLayerQtImpl::MaskLayerChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setPosition(const FloatPoint& value)
{
    if (value == position())
        return;
    GraphicsLayer::setPosition(value);
    m_impl->notifyChange(GraphicsLayerQtImpl::PositionChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setAnchorPoint(const FloatPoint3D& value)
{
    if (value == anchorPoint())
        return;
    GraphicsLayer::setAnchorPoint(value);
    m_impl->notifyChange(GraphicsLayerQtImpl::AnchorPointChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setSize(const FloatSize& value)
{
    if (value == size())
        return;
    GraphicsLayer::setSize(value);
    m_impl->notifyChange(GraphicsLayerQtImpl::SizeChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setTransform(const TransformationMatrix& value)
{
    if (value == transform())
        return;
    GraphicsLayer::setTransform(value);
    m_impl->notifyChange(GraphicsLayerQtImpl::TransformChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setChildrenTransform(const TransformationMatrix& value)
{
    if (value == childrenTransform())
        return;
    GraphicsLayer::setChildrenTransform(value);
    m_impl->notifyChange(GraphicsLayerQtImpl::ChildrenTransformChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setPreserves3D(bool value)
{
    if (value == preserves3D())
        return;
    GraphicsLayer::setPreserves3D(value);
    m_impl->notifyChange(GraphicsLayerQtImpl::Preserves3DChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setMasksToBounds(bool value)
{
    if (value == masksToBounds())
        return;
    GraphicsLayer::setMasksToBounds(value);
    m_impl->notifyChange(GraphicsLayerQtImpl::MasksToBoundsChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setDrawsContent(bool value)
{
    if (value == drawsContent())
        return;
    m_impl->notifyChange(GraphicsLayerQtImpl::DrawsContentChange);
    GraphicsLayer::setDrawsContent(value);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setBackgroundColor(const Color& value)
{
    if (value == m_impl->m_pendingContent.backgroundColor)
        return;
    m_impl->m_pendingContent.backgroundColor = value;
    GraphicsLayer::setBackgroundColor(value);
    m_impl->notifyChange(GraphicsLayerQtImpl::BackgroundColorChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::clearBackgroundColor()
{
    if (!m_impl->m_pendingContent.backgroundColor.isValid())
        return;
    m_impl->m_pendingContent.backgroundColor = QColor();
    GraphicsLayer::clearBackgroundColor();
    m_impl->notifyChange(GraphicsLayerQtImpl::BackgroundColorChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setContentsOpaque(bool value)
{
    if (value == contentsOpaque())
        return;
    m_impl->notifyChange(GraphicsLayerQtImpl::ContentsOpaqueChange);
    GraphicsLayer::setContentsOpaque(value);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setBackfaceVisibility(bool value)
{
    if (value == backfaceVisibility())
        return;
    GraphicsLayer::setBackfaceVisibility(value);
    m_impl->notifyChange(GraphicsLayerQtImpl::BackfaceVisibilityChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setOpacity(float value)
{
    if (value == opacity())
        return;
    GraphicsLayer::setOpacity(value);
    m_impl->notifyChange(GraphicsLayerQtImpl::OpacityChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setContentsRect(const IntRect& value)
{
    if (value == contentsRect())
        return;
    GraphicsLayer::setContentsRect(value);
    m_impl->notifyChange(GraphicsLayerQtImpl::ContentsRectChange);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setContentsToImage(Image* image)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::ContentChange);
    m_impl->m_pendingContent.contentType = GraphicsLayerQtImpl::HTMLContentType;
    GraphicsLayer::setContentsToImage(image);
    if (image) {
        QPixmap* pxm = image->nativeImageForCurrentFrame();
        if (pxm) {
            m_impl->m_pendingContent.pixmap = *pxm;
            m_impl->m_pendingContent.contentType = GraphicsLayerQtImpl::PixmapContentType;
            return;
        }        
    }
    m_impl->m_pendingContent.pixmap = QPixmap();
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setContentsBackgroundColor(const Color& color)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::ContentChange);
    m_impl->m_pendingContent.contentType = GraphicsLayerQtImpl::ColorContentType;
    m_impl->m_pendingContent.contentsBackgroundColor = QColor(color);
    GraphicsLayer::setContentsBackgroundColor(color);
}

void GraphicsLayerQt::setContentsToMedia(PlatformLayer* media)
{
    if (media) {
        m_impl->m_pendingContent.contentType = GraphicsLayerQtImpl::MediaContentType;
        m_impl->m_pendingContent.mediaLayer = media->toGraphicsObject();
    } else
        m_impl->m_pendingContent.contentType = GraphicsLayerQtImpl::HTMLContentType;

    m_impl->notifyChange(GraphicsLayerQtImpl::ContentChange);
    GraphicsLayer::setContentsToMedia(media);
}


// reimp from GraphicsLayer.h
void GraphicsLayerQt::setGeometryOrientation(CompositingCoordinatesOrientation orientation)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::GeometryOrientationChange);
    GraphicsLayer::setGeometryOrientation(orientation);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::setContentsOrientation(CompositingCoordinatesOrientation orientation)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::ContentsOrientationChange);
    GraphicsLayer::setContentsOrientation(orientation);
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::distributeOpacity(float o)
{
    m_impl->notifyChange(GraphicsLayerQtImpl::OpacityChange);
    m_impl->m_state.distributeOpacity = true;
}

// reimp from GraphicsLayer.h
float GraphicsLayerQt::accumulatedOpacity() const
{
    return m_impl->effectiveOpacity();
}

// reimp from GraphicsLayer.h
void GraphicsLayerQt::syncCompositingState()
{
    m_impl->flushChanges();
    GraphicsLayer::syncCompositingState();
}

// reimp from GraphicsLayer.h
NativeLayer GraphicsLayerQt::nativeLayer() const
{
    return m_impl.get();
}

// reimp from GraphicsLayer.h
PlatformLayer* GraphicsLayerQt::platformLayer() const
{
    return m_impl.get();
}

// now we start dealing with WebCore animations translated to Qt animations

template <typename T>
struct KeyframeValueQt {
    TimingFunction timingFunction;
    T value;
};

// we copy this from the AnimationBase.cpp
static inline double solveEpsilon(double duration)
{
    return 1.0 / (200.0 * duration);
}

static inline double solveCubicBezierFunction(qreal p1x, qreal p1y, qreal p2x, qreal p2y, double t, double duration)
{
    UnitBezier bezier(p1x, p1y, p2x, p2y);
    return bezier.solve(t, solveEpsilon(duration));
}

// we want the timing function to be as close as possible to what the web-developer intended, so we're using the same function used by WebCore when compositing is disabled
// Using easing-curves would probably work for some of the cases, but wouldn't really buy us anything as we'd have to convert the bezier function back to an easing curve
static inline qreal applyTimingFunction(const TimingFunction& timingFunction, qreal progress, double duration)
{
    if (timingFunction.type() == LinearTimingFunction)
        return progress;
    if (timingFunction.type() == CubicBezierTimingFunction) {
        return solveCubicBezierFunction(timingFunction.x1(),
                                        timingFunction.y1(),
                                        timingFunction.x2(),
                                        timingFunction.y2(),
                                        double(progress), double(duration) / 1000);
    }
    return progress;
}

// helper functions to safely get a value out of WebCore's AnimationValue*
static void webkitAnimationToQtAnimationValue(const AnimationValue* animationValue, TransformOperations& transformOperations)
{
    transformOperations = TransformOperations();
    if (!animationValue)
        return;

    if (const TransformOperations* ops = static_cast<const TransformAnimationValue*>(animationValue)->value())
        transformOperations = *ops;
}

static void webkitAnimationToQtAnimationValue(const AnimationValue* animationValue, qreal& realValue)
{
    realValue = animationValue ? static_cast<const FloatAnimationValue*>(animationValue)->value() : 0;
}

#ifndef QT_NO_ANIMATION
// we put a bit of the functionality in a base class to allow casting and to save some code size
class AnimationQtBase : public QAbstractAnimation {
public:
    AnimationQtBase(GraphicsLayerQtImpl* layer, const KeyframeValueList& values, const IntSize& boxSize, const Animation* anim, const QString & name)
        : QAbstractAnimation(0)
        , m_layer(layer)
        , m_boxSize(boxSize)
        , m_duration(anim->duration() * 1000)
        , m_isAlternate(anim->direction() == Animation::AnimationDirectionAlternate)
        , m_webkitPropertyID(values.property())
        , m_webkitAnimation(anim)
        , m_keyframesName(name)
    {
    }

    virtual void updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState)
    {
        QAbstractAnimation::updateState(newState, oldState);

        // for some reason I have do this asynchronously - or the animation won't work
        if (newState == Running && oldState == Stopped && m_layer.data())
            m_layer.data()->notifyAnimationStartedAsync();
    }

    virtual int duration() const { return m_duration; }

    QWeakPointer<GraphicsLayerQtImpl> m_layer;
    IntSize m_boxSize;
    int m_duration;
    bool m_isAlternate;
    AnimatedPropertyID m_webkitPropertyID;

    // we might need this in case the same animation is added again (i.e. resumed by WebCore)
    const Animation* m_webkitAnimation;
    QString m_keyframesName;
};

// we'd rather have a templatized QAbstractAnimation than QPropertyAnimation / QVariantAnimation;
// Since we know the types that we're dealing with, the QObject/QProperty/QVariant abstraction
// buys us very little in this case, for too much overhead
template <typename T>
class AnimationQt : public AnimationQtBase {

public:
    AnimationQt(GraphicsLayerQtImpl* layer, const KeyframeValueList& values, const IntSize& boxSize, const Animation* anim, const QString & name)
        :AnimationQtBase(layer, values, boxSize, anim, name)
    {
        // copying those WebCore structures is not trivial, we have to do it like this
        for (size_t i = 0; i < values.size(); ++i) {
            const AnimationValue* animationValue = values.at(i);
            KeyframeValueQt<T> keyframeValue;
            if (animationValue->timingFunction())
                keyframeValue.timingFunction = *animationValue->timingFunction();
            else
                keyframeValue.timingFunction = anim->timingFunction();
            webkitAnimationToQtAnimationValue(animationValue, keyframeValue.value);
            m_keyframeValues[animationValue->keyTime()] = keyframeValue;
        }
    }

protected:

    // this is the part that differs between animated properties
    virtual void applyFrame(const T& fromValue, const T& toValue, qreal progress) = 0;

    virtual void updateCurrentTime(int currentTime)
    {
        if (!m_layer)
            return;

        qreal progress = qreal(currentLoopTime()) / duration();

        if (m_isAlternate && currentLoop()%2)
            progress = 1-progress;

        if (m_keyframeValues.isEmpty())
            return;

        // we find the current from-to keyframes in our little map
        typename QMap<qreal, KeyframeValueQt<T> >::iterator it = m_keyframeValues.find(progress);

        // we didn't find an exact match, we try the closest match (lower bound)
        if (it == m_keyframeValues.end())
            it = m_keyframeValues.lowerBound(progress)-1;

        // we didn't find any match - we use the first keyframe
        if (it == m_keyframeValues.end())
            it = m_keyframeValues.begin();

        typename QMap<qreal, KeyframeValueQt<T> >::iterator it2 = it+1;
        if (it2 == m_keyframeValues.end())
            it2 = it;
        const KeyframeValueQt<T>& fromKeyframe = it.value();
        const KeyframeValueQt<T>& toKeyframe = it2.value();

        const TimingFunction& timingFunc = fromKeyframe.timingFunction;
        const T& fromValue = fromKeyframe.value;
        const T& toValue = toKeyframe.value;

        // now we have a source keyframe, origin keyframe and a timing function
        // we can now process the progress and apply the frame
        progress = (!progress || progress == 1 || it.key() == it2.key())
                                         ? progress
                                         : applyTimingFunction(timingFunc, (progress - it.key()) / (it2.key() - it.key()), duration());
        applyFrame(fromValue, toValue, progress);
    }

    QMap<qreal, KeyframeValueQt<T> > m_keyframeValues;
};

class TransformAnimationQt : public AnimationQt<TransformOperations> {
public:
    TransformAnimationQt(GraphicsLayerQtImpl* layer, const KeyframeValueList& values, const IntSize& boxSize, const Animation* anim, const QString & name)
        : AnimationQt<TransformOperations>(layer, values, boxSize, anim, name)
    {
    }

    ~TransformAnimationQt()
    {
        // this came up during the compositing/animation LayoutTests
        // when the animation dies, the transform has to go back to default
        if (m_layer)
            m_layer.data()->updateTransform();
    }

    // the idea is that we let WebCore manage the transform-operations
    // and Qt just manages the animation heartbeat and the bottom-line QTransform
    // we get the performance not by using QTransform instead of TransformationMatrix, but by proper caching of
    // items that are expensive for WebCore to render. We want the rest to be as close to WebCore's idea as possible.
    virtual void applyFrame(const TransformOperations& sourceOperations, const TransformOperations& targetOperations, qreal progress)
    {
        TransformationMatrix transformMatrix;

        // sometimes the animation values from WebCore are misleading and we have to use the actual matrix as source
        // The Mac implementation simply doesn't try to accelerate those (e.g. 360deg rotation), but we do.
        if (progress == 1 || !targetOperations.size() || sourceOperations == targetOperations) {
            TransformationMatrix sourceMatrix;
            sourceOperations.apply(m_boxSize, sourceMatrix);
            transformMatrix = m_sourceMatrix;
            transformMatrix.blend(sourceMatrix, 1 - progress);
        } else {
            bool validTransformLists = true;
            const int sourceOperationCount = sourceOperations.size();
            if (sourceOperationCount) {
                if (targetOperations.size() != sourceOperationCount)
                    validTransformLists = false;
                else
                    for (size_t j = 0; j < sourceOperationCount && validTransformLists; ++j)
                        if (!sourceOperations.operations()[j]->isSameType(*targetOperations.operations()[j]))
                            validTransformLists = false;
            }

            if (validTransformLists) {
                for (size_t i = 0; i < targetOperations.size(); ++i)
                    targetOperations.operations()[i]->blend(sourceOperations.at(i), progress)->apply(transformMatrix, m_boxSize);
            } else {
                targetOperations.apply(m_boxSize, transformMatrix);
                transformMatrix.blend(m_sourceMatrix, progress);
            }
        }
        m_layer.data()->setBaseTransform(transformMatrix);
    }

    virtual void updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState)
    {
        AnimationQtBase::updateState(newState, oldState);
        if (!m_layer)
            return;
        m_layer.data()->flushChanges(true);

        // to increase FPS, we use a less accurate caching mechanism while animation is going on
        // this is a UX choice that should probably be customizable
        if (newState == QAbstractAnimation::Running) {
            m_sourceMatrix = m_layer.data()->m_layer->transform();
            m_layer.data()->m_transformAnimationRunning = true;
            m_layer.data()->adjustCachingRecursively(true);
        } else if (newState == QAbstractAnimation::Stopped) {
            m_layer.data()->m_transformAnimationRunning = false;
            m_layer.data()->adjustCachingRecursively(false);
        }
    }

    TransformationMatrix m_sourceMatrix;
};

class OpacityAnimationQt : public AnimationQt<qreal> {
public:
    OpacityAnimationQt(GraphicsLayerQtImpl* layer, const KeyframeValueList& values, const IntSize& boxSize, const Animation* anim, const QString & name)
         : AnimationQt<qreal>(layer, values, boxSize, anim, name)
    {
    }

    virtual void applyFrame(const qreal& fromValue, const qreal& toValue, qreal progress)
    {
        qreal opacity = qBound(qreal(0), fromValue + (toValue-fromValue)*progress, qreal(1));

        // FIXME: this is a hack, due to a probable QGraphicsScene bug.
        // Without this the opacity change doesn't always have immediate effect.
        if (!m_layer.data()->opacity() && opacity)
            m_layer.data()->scene()->update();

        m_layer.data()->setOpacity(opacity);
    }

    virtual void updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState)
    {
        QAbstractAnimation::updateState(newState, oldState);

        if (m_layer)
            m_layer.data()->m_opacityAnimationRunning = (newState == QAbstractAnimation::Running);
    }
};

bool GraphicsLayerQt::addAnimation(const KeyframeValueList& values, const IntSize& boxSize, const Animation* anim, const String& keyframesName, double timeOffset)
{
    if (!anim->duration() || !anim->iterationCount())
        return false;

    QAbstractAnimation* newAnim = 0;

    // fixed: we might already have the Qt animation object associated with this WebCore::Animation object
    for (QList<QWeakPointer<QAbstractAnimation> >::iterator it = m_impl->m_animations.begin(); it != m_impl->m_animations.end(); ++it) {
        if (*it) {
            AnimationQtBase* curAnimation = static_cast<AnimationQtBase*>(it->data());
            if (curAnimation && curAnimation->m_webkitAnimation == anim)
                newAnim = curAnimation;
        }
    }

    if (!newAnim) {
        switch (values.property()) {
        case AnimatedPropertyOpacity:
            newAnim = new OpacityAnimationQt(m_impl.get(), values, boxSize, anim, keyframesName);
            break;
        case AnimatedPropertyWebkitTransform:
            newAnim = new TransformAnimationQt(m_impl.get(), values, boxSize, anim, keyframesName);
            break;
        default:
            return false;
        }

        // we make sure WebCore::Animation and QAnimation are on the same terms
        newAnim->setLoopCount(anim->iterationCount());
        m_impl->m_animations.append(QWeakPointer<QAbstractAnimation>(newAnim));
        QObject::connect(&m_impl->m_suspendTimer, SIGNAL(timeout()), newAnim, SLOT(resume()));
    }

    // flush now or flicker...
    m_impl->flushChanges(false);

    if (anim->delay())
        QTimer::singleShot(anim->delay() * 1000, newAnim, SLOT(start()));
    else
        newAnim->start();

    // we synchronize the animation's clock to WebCore's timeOffset
    newAnim->setCurrentTime(timeOffset * 1000);

    // we don't need to manage the animation object's lifecycle:
    // WebCore would call removeAnimations when it's time to delete.

    return true;
}

void GraphicsLayerQt::removeAnimationsForProperty(AnimatedPropertyID id)
{
    for (QList<QWeakPointer<QAbstractAnimation> >::iterator it = m_impl->m_animations.begin(); it != m_impl->m_animations.end(); ++it) {
        if (*it) {
            AnimationQtBase* anim = static_cast<AnimationQtBase*>(it->data());
            if (anim && anim->m_webkitPropertyID == id) {
                anim->deleteLater();
                it = m_impl->m_animations.erase(it);
                --it;
            }
        }
    }
}

void GraphicsLayerQt::removeAnimationsForKeyframes(const String& name)
{
    for (QList<QWeakPointer<QAbstractAnimation> >::iterator it = m_impl->m_animations.begin(); it != m_impl->m_animations.end(); ++it) {
        if (*it) {
            AnimationQtBase* anim = static_cast<AnimationQtBase*>((*it).data());
            if (anim && anim->m_keyframesName == QString(name)) {
                (*it).data()->deleteLater();
                it = m_impl->m_animations.erase(it);
                --it;
            }
        }
    }
}

void GraphicsLayerQt::pauseAnimation(const String& name, double timeOffset)
{
    for (QList<QWeakPointer<QAbstractAnimation> >::iterator it = m_impl->m_animations.begin(); it != m_impl->m_animations.end(); ++it) {
        if (!(*it))
            continue;

        AnimationQtBase* anim = static_cast<AnimationQtBase*>((*it).data());
        if (anim && anim->m_keyframesName == QString(name)) {
            // we synchronize the animation's clock to WebCore's timeOffset
            anim->setCurrentTime(timeOffset * 1000);
            anim->pause();
        }
    }
}

void GraphicsLayerQt::suspendAnimations(double time)
{
    if (m_impl->m_suspendTimer.isActive()) {
        m_impl->m_suspendTimer.stop();
        m_impl->m_suspendTimer.start(time * 1000);
    } else {
        for (QList<QWeakPointer<QAbstractAnimation> >::iterator it = m_impl->m_animations.begin(); it != m_impl->m_animations.end(); ++it) {
            QAbstractAnimation* anim = it->data();
            if (anim)
                anim->pause();
        }
    }
}

void GraphicsLayerQt::resumeAnimations()
{
    if (m_impl->m_suspendTimer.isActive()) {
        m_impl->m_suspendTimer.stop();
        for (QList<QWeakPointer<QAbstractAnimation> >::iterator it = m_impl->m_animations.begin(); it != m_impl->m_animations.end(); ++it) {
            QAbstractAnimation* anim = (*it).data();
            if (anim)
                anim->resume();
        }
    }
}

#endif // QT_NO_ANIMATION
}

#include <GraphicsLayerQt.moc>
