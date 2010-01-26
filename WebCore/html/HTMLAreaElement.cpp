/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "HTMLAreaElement.h"

#include "HTMLImageElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "MappedAttribute.h"
#include "Path.h"
#include "RenderObject.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

inline HTMLAreaElement::HTMLAreaElement(const QualifiedName& tagName, Document* document)
    : HTMLAnchorElement(tagName, document)
    , m_coordsLen(0)
    , m_lastSize(-1, -1)
    , m_shape(Unknown)
{
    ASSERT(hasTagName(areaTag));
}

PassRefPtr<HTMLAreaElement> HTMLAreaElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLAreaElement(tagName, document));
}

void HTMLAreaElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == shapeAttr) {
        if (equalIgnoringCase(attr->value(), "default"))
            m_shape = Default;
        else if (equalIgnoringCase(attr->value(), "circle"))
            m_shape = Circle;
        else if (equalIgnoringCase(attr->value(), "poly"))
            m_shape = Poly;
        else if (equalIgnoringCase(attr->value(), "rect"))
            m_shape = Rect;
    } else if (attr->name() == coordsAttr) {
        m_coords.set(newCoordsArray(attr->value().string(), m_coordsLen));
    } else if (attr->name() == altAttr || attr->name() == accesskeyAttr) {
        // Do nothing.
    } else
        HTMLAnchorElement::parseMappedAttribute(attr);
}

bool HTMLAreaElement::mapMouseEvent(int x, int y, const IntSize& size, HitTestResult& result)
{
    if (m_lastSize != size) {
        m_region.set(new Path(getRegion(size)));
        m_lastSize = size;
    }

    if (!m_region->contains(IntPoint(x, y)))
        return false;
    
    result.setInnerNode(this);
    result.setURLElement(this);
    return true;
}

Path HTMLAreaElement::getPath(RenderObject* obj) const
{
    if (!obj)
        return Path();
    
    // FIXME: This doesn't work correctly with transforms.
    FloatPoint absPos = obj->localToAbsolute();

    // Default should default to the size of the containing object.
    IntSize size = m_lastSize;
    if (m_shape == Default)
        size = obj->absoluteOutlineBounds().size();
    
    Path p = getRegion(size);
    p.translate(absPos - FloatPoint());
    return p;
}
    
IntRect HTMLAreaElement::getRect(RenderObject* obj) const
{
    return enclosingIntRect(getPath(obj).boundingRect());
}

Path HTMLAreaElement::getRegion(const IntSize& size) const
{
    if (!m_coords && m_shape != Default)
        return Path();

    int width = size.width();
    int height = size.height();

    // If element omits the shape attribute, select shape based on number of coordinates.
    Shape shape = m_shape;
    if (shape == Unknown) {
        if (m_coordsLen == 3)
            shape = Circle;
        else if (m_coordsLen == 4)
            shape = Rect;
        else if (m_coordsLen >= 6)
            shape = Poly;
    }

    Path path;
    switch (shape) {
        case Poly:
            if (m_coordsLen >= 6) {
                int numPoints = m_coordsLen / 2;
                path.moveTo(FloatPoint(m_coords[0].calcMinValue(width), m_coords[1].calcMinValue(height)));
                for (int i = 1; i < numPoints; ++i)
                    path.addLineTo(FloatPoint(m_coords[i * 2].calcMinValue(width), m_coords[i * 2 + 1].calcMinValue(height)));
                path.closeSubpath();
            }
            break;
        case Circle:
            if (m_coordsLen >= 3) {
                Length radius = m_coords[2];
                int r = min(radius.calcMinValue(width), radius.calcMinValue(height));
                path.addEllipse(FloatRect(m_coords[0].calcMinValue(width) - r, m_coords[1].calcMinValue(height) - r, 2 * r, 2 * r));
            }
            break;
        case Rect:
            if (m_coordsLen >= 4) {
                int x0 = m_coords[0].calcMinValue(width);
                int y0 = m_coords[1].calcMinValue(height);
                int x1 = m_coords[2].calcMinValue(width);
                int y1 = m_coords[3].calcMinValue(height);
                path.addRect(FloatRect(x0, y0, x1 - x0, y1 - y0));
            }
            break;
        case Default:
            path.addRect(FloatRect(0, 0, width, height));
            break;
        case Unknown:
            break;
    }

    return path;
}

KURL HTMLAreaElement::href() const
{
    return document()->completeURL(getAttribute(hrefAttr));
}

bool HTMLAreaElement::noHref() const
{
    return !getAttribute(nohrefAttr).isNull();
}

void HTMLAreaElement::setNoHref(bool noHref)
{
    setAttribute(nohrefAttr, noHref ? "" : 0);
}
    
HTMLImageElement* HTMLAreaElement::imageElement() const
{
    Node* mapElement = parent();
    if (!mapElement->hasTagName(mapTag))
        return 0;
    
    return static_cast<HTMLMapElement*>(mapElement)->imageElement();
}

bool HTMLAreaElement::isKeyboardFocusable(KeyboardEvent*) const
{
    return supportsFocus();
}

bool HTMLAreaElement::isFocusable() const
{
    return supportsFocus();
}
    
void HTMLAreaElement::dispatchBlurEvent()
{
    HTMLAnchorElement::dispatchBlurEvent();
    
    // On a blur, we might need to remove our focus rings by repainting.
    updateFocusAppearance(false);
}
    
void HTMLAreaElement::updateFocusAppearance(bool restorePreviousSelection)
{
    Node* parent = parentNode();
    if (!parent || !parent->hasTagName(mapTag))
        return;
    
    HTMLImageElement* imageElement = static_cast<HTMLMapElement*>(parent)->imageElement();
    if (!imageElement)
        return;
    
    // This will handle scrolling to the image if necessary.
    imageElement->updateFocusAppearance(restorePreviousSelection);
    
    RenderObject* imageRenderer = imageElement->renderer();
    if (imageRenderer)
        imageRenderer->setNeedsLayout(true);
}
    
bool HTMLAreaElement::supportsFocus() const
{
    // If the AREA element was a link, it should support focus.
    // The inherited method is not used because it assumes that a render object must exist 
    // for the element to support focus. AREA elements do not have render objects.
    return isLink();
}

String HTMLAreaElement::target() const
{
    return getAttribute(targetAttr);
}

}
