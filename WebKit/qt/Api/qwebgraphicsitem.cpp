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
#include "qwebgraphicsitem.h"

#include "qwebframe.h"
#include "qwebpage.h"
#include "qwebpage_p.h"
#include "QWebPageClient.h"
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsView>
#include <QtGui/qapplication.h>
#include <QtGui/qgraphicssceneevent.h>
#include <QtGui/qstyleoption.h>
#if defined(Q_WS_X11)
#include <QX11Info>
#endif

class QWebGraphicsItemPrivate : public QWebPageClient {
public:
    QWebGraphicsItemPrivate(QWebGraphicsItem* parent)
        : q(parent)
        , page(0)
        , interactive(true)
        , progress(1.0)
    {}

    virtual void scroll(int dx, int dy, const QRect&);
    virtual void update(const QRect& dirtyRect);

    virtual QCursor cursor() const;
    virtual void updateCursor(const QCursor& cursor);

    virtual int screenNumber() const;
    virtual WId winId() const;

    void _q_doLoadProgress(int progress);
    void _q_doLoadFinished(bool success);
    void _q_setStatusBarMessage(const QString& message);

    QWebGraphicsItem* q;
    QWebPage* page;

    QString statusBarMessage;
    bool interactive;
    qreal progress;
};

void QWebGraphicsItemPrivate::_q_doLoadProgress(int progress)
{
    if (qFuzzyCompare(this->progress, qreal(progress / 100.)))
        return;

    this->progress = progress / 100.;

    emit q->progressChanged(this->progress);
}

void QWebGraphicsItemPrivate::_q_doLoadFinished(bool success)
{
    // If the page had no title, still make sure it gets the signal
    if (q->title().isEmpty())
        emit q->urlChanged(q->url());

    if (success)
        emit q->loadFinished();
    else
        emit q->loadFailed();
}

void QWebGraphicsItemPrivate::scroll(int dx, int dy, const QRect& rectToScroll)
{
    q->scroll(qreal(dx), qreal(dy), QRectF(rectToScroll));
}

void QWebGraphicsItemPrivate::update(const QRect & dirtyRect)
{
    q->update(QRectF(dirtyRect));
}

QCursor QWebGraphicsItemPrivate::cursor() const
{
    return q->cursor();
}

void QWebGraphicsItemPrivate::updateCursor(const QCursor& cursor)
{
    q->setCursor(cursor);
}

int QWebGraphicsItemPrivate::screenNumber() const
{
#if defined(Q_WS_X11)
    const QList<QGraphicsView*> views = q->scene()->views();

    if (!views.isEmpty())
        return views.at(0)->x11Info().screen();
#endif

    return 0;
}

WId QWebGraphicsItemPrivate::winId() const
{
    const QList<QGraphicsView*> views = q->scene()->views();

    if (!views.isEmpty())
        return views.at(0)->winId();

    return 0;
}

void QWebGraphicsItemPrivate::_q_setStatusBarMessage(const QString& s)
{
    statusBarMessage = s;
    emit q->statusChanged();
}

/*!
    \class QWebGraphicsItem
    \brief The QWebGraphicsItem class allows web content to be added to a GraphicsView.
    \since 4.6

    A WebGraphicsItem renders web content based on a URL or set data.

    If the width and height of the item is not set, they will
    dynamically adjust to a size appropriate for the content.
    This width may be large (eg. 980) for typical online web pages.
*/

/*!
    Constructs an empty QWebGraphicsItem with parent \a parent.

    \sa load()
*/
QWebGraphicsItem::QWebGraphicsItem(QGraphicsItem* parent)
    : QGraphicsWidget(parent)
    , d(new QWebGraphicsItemPrivate(this))
{
#if QT_VERSION >= 0x040600
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
#endif
    setAcceptHoverEvents(true);
    setFocusPolicy(Qt::StrongFocus);
}

/*!
    Destroys the web graphicsitem.
*/
QWebGraphicsItem::~QWebGraphicsItem()
{
    if (d->page)
        d->page->d->view = 0;

    if (d->page && d->page->parent() == this)
        delete d->page;

    delete d;
}

/*!
    Returns a pointer to the underlying web page.

    \sa setPage()
*/
QWebPage* QWebGraphicsItem::page() const
{
    if (!d->page) {
        QWebGraphicsItem* that = const_cast<QWebGraphicsItem*>(this);
        QWebPage* page = new QWebPage(that);

        // Default to not having a background, in the case
        // the page doesn't provide one.
        QPalette palette = QApplication::palette();
        palette.setBrush(QPalette::Base, QColor::fromRgbF(0, 0, 0, 0));
        page->setPalette(palette);

        that->setPage(page);
    }

    return d->page;
}

/*! \reimp
*/
void QWebGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    page()->mainFrame()->render(painter, option->exposedRect.toRect());
}

/*! \reimp
*/
bool QWebGraphicsItem::sceneEvent(QEvent* event)
{
    // Re-implemented in order to allows fixing event-related bugs in patch releases.
    return QGraphicsWidget::sceneEvent(event);
}

/*! \reimp
*/
bool QWebGraphicsItem::event(QEvent* event)
{
    // Re-implemented in order to allows fixing event-related bugs in patch releases.

    if (d->page) {
#ifndef QT_NO_CURSOR
#if QT_VERSION >= 0x040400
        } else if (event->type() == QEvent::CursorChange) {
            // An unsetCursor will set the cursor to Qt::ArrowCursor.
            // Thus this cursor change might be a QWidget::unsetCursor()
            // If this is not the case and it came from WebCore, the
            // QWebPageClient already has set its cursor internally
            // to Qt::ArrowCursor, so updating the cursor is always
            // right, as it falls back to the last cursor set by
            // WebCore.
            // FIXME: Add a QEvent::CursorUnset or similar to Qt.
            if (cursor().shape() == Qt::ArrowCursor)
                d->resetCursor();
#endif
#endif
    }
    return QGraphicsWidget::event(event);
}

/*!
    Makes \a page the new web page of the web graphicsitem.

    The parent QObject of the provided page remains the owner
    of the object. If the current document is a child of the web
    view, it will be deleted.

    \sa page()
*/
void QWebGraphicsItem::setPage(QWebPage* page)
{
    if (d->page == page)
        return;

    if (d->page) {
        d->page->d->client = 0; // unset the page client
        if (d->page->parent() == this)
            delete d->page;
        else
            d->page->disconnect(this);
    }

    d->page = page;
    if (!d->page)
        return;
    d->page->d->client = d; // set the page client

    QSize size = geometry().size().toSize();
    page->setViewportSize(size);

    QWebFrame* mainFrame = d->page->mainFrame();

    connect(mainFrame, SIGNAL(titleChanged(const QString&)),
            this, SIGNAL(titleChanged(const QString&)));
    connect(mainFrame, SIGNAL(iconChanged()),
            this, SIGNAL(iconChanged()));
    connect(mainFrame, SIGNAL(urlChanged(const QUrl&)),
            this, SIGNAL(urlChanged(const QUrl&)));
    connect(d->page, SIGNAL(loadStarted()),
            this, SIGNAL(loadStarted()));
    connect(d->page, SIGNAL(loadProgress(int)),
            this, SLOT(_q_doLoadProgress(int)));
    connect(d->page, SIGNAL(loadFinished(bool)),
            this, SLOT(_q_doLoadFinished(bool)));
    connect(d->page, SIGNAL(statusBarMessage(const QString&)),
            this, SLOT(_q_setStatusBarMessage(const QString&)));
}

/*!
    \property QWebGraphicsItem::url
    \brief the url of the web page currently viewed

    Setting this property clears the view and loads the URL.

    By default, this property contains an empty, invalid URL.

    \sa load(), urlChanged()
*/

void QWebGraphicsItem::setUrl(const QUrl &url)
{
    page()->mainFrame()->setUrl(url);
}

QUrl QWebGraphicsItem::url() const
{
    if (d->page)
        return d->page->mainFrame()->url();

    return QUrl();
}

/*!
    \property QWebGraphicsItem::title
    \brief the title of the web page currently viewed

    By default, this property contains an empty string.

    \sa titleChanged()
*/
QString QWebGraphicsItem::title() const
{
    if (d->page)
        return d->page->mainFrame()->title();

    return QString();
}

/*!
    \property QWebGraphicsItem::icon
    \brief the icon associated with the web page currently viewed

    By default, this property contains a null icon.

    \sa iconChanged(), QWebSettings::iconForUrl()
*/
QIcon QWebGraphicsItem::icon() const
{
    if (d->page)
        return d->page->mainFrame()->icon();

    return QIcon();
}

/*!
    \property QWebGraphicsItem::zoomFactor
    \since 4.5
    \brief the zoom factor for the view
*/

void QWebGraphicsItem::setZoomFactor(qreal factor)
{
    if (factor == page()->mainFrame()->zoomFactor())
        return;

    page()->mainFrame()->setZoomFactor(factor);
    emit zoomFactorChanged();
}

qreal QWebGraphicsItem::zoomFactor() const
{
    return page()->mainFrame()->zoomFactor();
}

/*! \reimp
*/
void QWebGraphicsItem::updateGeometry()
{
    QGraphicsWidget::updateGeometry();

    if (!d->page)
        return;

    QSize size = geometry().size().toSize();
    d->page->setViewportSize(size);
}

/*! \reimp
*/
void QWebGraphicsItem::setGeometry(const QRectF& rect)
{
    QGraphicsWidget::setGeometry(rect);

    if (!d->page)
        return;

    // NOTE: call geometry() as setGeometry ensures that
    // the geometry is within legal bounds (minimumSize, maximumSize)
    QSize size = geometry().size().toSize();
    d->page->setViewportSize(size);
}

/*!
    \brief The load status message associated to the web graphicsitem

    Provides the latest status message set during the load of a URL.
    Commonly shown by Status Bar widgets.

    \sa statusChanged()
*/

QString QWebGraphicsItem::status() const
{
    return d->statusBarMessage;
}

/*!
    Convenience slot that stops loading the document.

    \sa reload(), loadFinished()
*/
void QWebGraphicsItem::stop()
{
    if (d->page)
        d->page->triggerAction(QWebPage::Stop);
}

/*!
    Convenience slot that loads the previous document in the list of documents
    built by navigating links. Does nothing if there is no previous document.

    \sa forward()
*/
void QWebGraphicsItem::back()
{
    if (d->page)
        d->page->triggerAction(QWebPage::Back);
}

/*!
    Convenience slot that loads the next document in the list of documents
    built by navigating links. Does nothing if there is no next document.

    \sa back()
*/
void QWebGraphicsItem::forward()
{
    if (d->page)
        d->page->triggerAction(QWebPage::Forward);
}

/*!
    Reloads the current document.

    \sa stop(), loadStarted()
*/
void QWebGraphicsItem::reload()
{
    if (d->page)
        d->page->triggerAction(QWebPage::Reload);
}

/*!
    \property QWebGraphicsItem::progress
    \brief the progress of loading the current URL, from 0 to 1.
*/
qreal QWebGraphicsItem::progress() const
{
    return d->progress;
}

/*!
    Loads the specified \a url and displays it.

    \note The view remains the same until enough data has arrived to display the new \a url.

    \sa setUrl(), url(), urlChanged()
*/
void QWebGraphicsItem::load(const QUrl& url)
{
    page()->mainFrame()->load(url);
}

/*!
    \fn void QWebGraphicsItem::load(const QNetworkRequest &request, QNetworkAccessManager::Operation operation, const QByteArray &body)

    Loads a network request, \a request, using the method specified in \a operation.

    \a body is optional and is only used for POST operations.

    \note The view remains the same until enough data has arrived to display the new url.

    \sa url(), urlChanged()
*/

void QWebGraphicsItem::load(const QNetworkRequest& request,
                    QNetworkAccessManager::Operation operation,
                    const QByteArray& body)
{
    page()->mainFrame()->load(request, operation, body);
}

/*!
    Sets the content of the web graphicsitem to the specified \a html.

    External objects such as stylesheets or images referenced in the HTML
    document are located relative to \a baseUrl.

    The \a html is loaded immediately; external objects are loaded asynchronously.

    When using this method, WebKit assumes that external resources such as
    JavaScript programs or style sheets are encoded in UTF-8 unless otherwise
    specified. For example, the encoding of an external script can be specified
    through the charset attribute of the HTML script tag. Alternatively, the
    encoding can also be specified by the web server.

    \sa load(), setContent(), QWebFrame::toHtml()
*/
void QWebGraphicsItem::setHtml(const QString& html, const QUrl& baseUrl)
{
    page()->mainFrame()->setHtml(html, baseUrl);
}

QString QWebGraphicsItem::toHtml() const
{
    return page()->mainFrame()->toHtml();
}

/*!
    Sets the content of the web graphicsitem to the specified content \a data. If the \a mimeType argument
    is empty it is currently assumed that the content is HTML but in future versions we may introduce
    auto-detection.

    External objects referenced in the content are located relative to \a baseUrl.

    The \a data is loaded immediately; external objects are loaded asynchronously.

    \sa load(), setHtml(), QWebFrame::toHtml()
*/
void QWebGraphicsItem::setContent(const QByteArray& data, const QString& mimeType, const QUrl& baseUrl)
{
    page()->mainFrame()->setContent(data, mimeType, baseUrl);
}

/*!
    Returns a pointer to the view's history of navigated web pages.

    It is equivalent to

    \snippet webkitsnippets/qtwebkit_qwebview_snippet.cpp 0
*/
QWebHistory* QWebGraphicsItem::history() const
{
    return page()->history();
}

/*!
  \property QWebGraphicsItem::interactive
  \brief controls whether the item responds to mouse and key events.
*/

bool QWebGraphicsItem::isInteractive() const
{
    return d->interactive;
}

void QWebGraphicsItem::setInteractive(bool allowed)
{
    if (d->interactive == allowed)
        return;

    d->interactive = allowed;
    emit interactivityChanged();
}

/*!
    Returns a pointer to the view/page specific settings object.

    It is equivalent to

    \snippet webkitsnippets/qtwebkit_qwebview_snippet.cpp 1

    \sa QWebSettings::globalSettings()
*/
QWebSettings* QWebGraphicsItem::settings() const
{
    return page()->settings();
}

/*! \reimp
*/
void QWebGraphicsItem::hoverMoveEvent(QGraphicsSceneHoverEvent* ev)
{
    if (d->interactive && d->page) {
        const bool accepted = ev->isAccepted();
        QMouseEvent me = QMouseEvent(QEvent::MouseMove,
                ev->pos().toPoint(), Qt::NoButton,
                Qt::NoButton, Qt::NoModifier);
        d->page->setView(ev->widget());
        d->page->event(&me);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsItem::hoverMoveEvent(ev);
}

/*! \reimp
*/
void QWebGraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* ev)
{
    Q_UNUSED(ev);
}

/*! \reimp
*/
void QWebGraphicsItem::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
    if (d->interactive && d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsItem::mouseMoveEvent(ev);
}

/*! \reimp
*/
void QWebGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
    if (d->interactive && d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsItem::mousePressEvent(ev);
}

/*! \reimp
*/
void QWebGraphicsItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
    if (d->interactive && d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsItem::mouseReleaseEvent(ev);
}

/*! \reimp
*/
void QWebGraphicsItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* ev)
{
    if (d->interactive && d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsItem::mouseDoubleClickEvent(ev);
}

/*! \reimp
*/
void QWebGraphicsItem::keyPressEvent(QKeyEvent* ev)
{
    if (d->interactive && d->page)
        d->page->event(ev);

    if (!ev->isAccepted())
        QGraphicsItem::keyPressEvent(ev);
}

/*! \reimp
*/
void QWebGraphicsItem::keyReleaseEvent(QKeyEvent* ev)
{
    if (d->interactive && d->page)
        d->page->event(ev);

    if (!ev->isAccepted())
        QGraphicsItem::keyReleaseEvent(ev);
}

/*! \reimp
*/
void QWebGraphicsItem::focusInEvent(QFocusEvent* ev)
{
    if (d->page)
        d->page->event(ev);
    else
        QGraphicsItem::focusInEvent(ev);
}

/*! \reimp
*/
void QWebGraphicsItem::focusOutEvent(QFocusEvent* ev)
{
    if (d->page)
        d->page->event(ev);
    else
        QGraphicsItem::focusOutEvent(ev);
}

/*! \reimp
*/
bool QWebGraphicsItem::focusNextPrevChild(bool next)
{
    if (d->page)
        return d->page->focusNextPrevChild(next);

    return QGraphicsWidget::focusNextPrevChild(next);
}

/*! \reimp
*/
void QWebGraphicsItem::dragEnterEvent(QGraphicsSceneDragDropEvent* ev)
{
#ifndef QT_NO_DRAGANDDROP
    //if (d->page)
    //    d->page->event(ev);
    //Just remove this line below when the code above is working
    Q_UNUSED(ev);
#else
    Q_UNUSED(ev);
#endif
}

/*! \reimp
*/
void QWebGraphicsItem::dragLeaveEvent(QGraphicsSceneDragDropEvent* ev)
{
#ifndef QT_NO_DRAGANDDROP
    if (d->interactive && d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsWidget::dragLeaveEvent(ev);
#else
    Q_UNUSED(ev);
#endif
}

/*! \reimp
*/
void QWebGraphicsItem::dragMoveEvent(QGraphicsSceneDragDropEvent* ev)
{
#ifndef QT_NO_DRAGANDDROP
    if (d->interactive && d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsWidget::dragMoveEvent(ev);
#else
    Q_UNUSED(ev);
#endif
}

/*! \reimp
*/
void QWebGraphicsItem::dropEvent(QGraphicsSceneDragDropEvent* ev)
{
#ifndef QT_NO_DRAGANDDROP
    if (d->interactive && d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsWidget::dropEvent(ev);
#else
    Q_UNUSED(ev);
#endif
}

#ifndef QT_NO_CONTEXTMENU
/*! \reimp
*/
void QWebGraphicsItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
    if (d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }
}
#endif // QT_NO_CONTEXTMENU

#ifndef QT_NO_WHEELEVENT
/*! \reimp
*/
void QWebGraphicsItem::wheelEvent(QGraphicsSceneWheelEvent* ev)
{
    if (d->interactive && d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsItem::wheelEvent(ev);
}
#endif // QT_NO_WHEELEVENT

/*! \reimp
*/
void QWebGraphicsItem::inputMethodEvent(QInputMethodEvent* ev)
{
    if (d->interactive && d->page)
        d->page->event(ev);

    if (!ev->isAccepted())
        QGraphicsItem::inputMethodEvent(ev);
}

#include "moc_qwebgraphicsitem.cpp"
