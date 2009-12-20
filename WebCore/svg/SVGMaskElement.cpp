/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
                  2005 Alexander Kellett <lypanov@kde.org>
                  2009 Dirk Schulze <krit@webkit.org>

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

#if ENABLE(SVG)
#include "SVGMaskElement.h"

#include "CSSStyleSelector.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "MappedAttribute.h"
#include "RenderSVGContainer.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGRenderSupport.h"
#include "SVGUnitTypes.h"
#include <math.h>
#include <wtf/MathExtras.h>
#include <wtf/OwnPtr.h>

using namespace std;

namespace WebCore {

SVGMaskElement::SVGMaskElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledLocatableElement(tagName, doc)
    , SVGURIReference()
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_maskUnits(this, SVGNames::maskUnitsAttr, SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
    , m_maskContentUnits(this, SVGNames::maskContentUnitsAttr, SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
    , m_x(this, SVGNames::xAttr, LengthModeWidth, "-10%")
    , m_y(this, SVGNames::yAttr, LengthModeHeight, "-10%")
    , m_width(this, SVGNames::widthAttr, LengthModeWidth, "120%")
    , m_height(this, SVGNames::heightAttr, LengthModeHeight, "120%")
    , m_href(this, XLinkNames::hrefAttr)
    , m_externalResourcesRequired(this, SVGNames::externalResourcesRequiredAttr, false)
{
    // Spec: If the x/y attribute is not specified, the effect is as if a value of "-10%" were specified.
    // Spec: If the width/height attribute is not specified, the effect is as if a value of "120%" were specified.
}

SVGMaskElement::~SVGMaskElement()
{
}

void SVGMaskElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::maskUnitsAttr) {
        if (attr->value() == "userSpaceOnUse")
            setMaskUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (attr->value() == "objectBoundingBox")
            setMaskUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::maskContentUnitsAttr) {
        if (attr->value() == "userSpaceOnUse")
            setMaskContentUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE);
        else if (attr->value() == "objectBoundingBox")
            setMaskContentUnitsBaseValue(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    } else if (attr->name() == SVGNames::xAttr)
        setXBaseValue(SVGLength(LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(SVGLength(LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::widthAttr)
        setWidthBaseValue(SVGLength(LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::heightAttr)
        setHeightBaseValue(SVGLength(LengthModeHeight, attr->value()));
    else {
        if (SVGURIReference::parseMappedAttribute(attr))
            return;
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGStyledElement::parseMappedAttribute(attr);
    }
}

void SVGMaskElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledElement::svgAttributeChanged(attrName);

    if (!m_masker)
        return;

    if (attrName == SVGNames::maskUnitsAttr || attrName == SVGNames::maskContentUnitsAttr ||
        attrName == SVGNames::xAttr || attrName == SVGNames::yAttr ||
        attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr ||
        SVGURIReference::isKnownAttribute(attrName) ||
        SVGTests::isKnownAttribute(attrName) ||
        SVGLangSpace::isKnownAttribute(attrName) ||
        SVGExternalResourcesRequired::isKnownAttribute(attrName) ||
        SVGStyledElement::isKnownAttribute(attrName))
        m_masker->invalidate();
}

void SVGMaskElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGStyledElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (!m_masker)
        return;

    m_masker->invalidate();
}

PassOwnPtr<ImageBuffer> SVGMaskElement::drawMaskerContent(const FloatRect& targetRect, FloatRect& maskDestRect) const
{    
    // Determine specified mask size
    if (maskUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        maskDestRect = FloatRect(x().valueAsPercentage() * targetRect.width(),
                                 y().valueAsPercentage() * targetRect.height(),
                                 width().valueAsPercentage() * targetRect.width(),
                                 height().valueAsPercentage() * targetRect.height());
    else
        maskDestRect = FloatRect(x().value(this),
                                 y().value(this),
                                 width().value(this),
                                 height().value(this));

    IntSize imageSize(lroundf(maskDestRect.width()), lroundf(maskDestRect.height()));
    clampImageBufferSizeToViewport(document()->view(), imageSize);

    if (imageSize.width() < static_cast<int>(maskDestRect.width()))
        maskDestRect.setWidth(imageSize.width());

    if (imageSize.height() < static_cast<int>(maskDestRect.height()))
        maskDestRect.setHeight(imageSize.height());

    // FIXME: This changes color space to linearRGB, the default color space
    // for masking operations in SVG. We need a switch for the other color-space
    // attribute values sRGB, inherit and auto.
    OwnPtr<ImageBuffer> maskImage = ImageBuffer::create(imageSize, LinearRGB);
    if (!maskImage)
        return 0;

    FloatPoint maskContextLocation = maskDestRect.location();
    if (maskUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        maskDestRect.move(targetRect.x(), targetRect.y());
        if (maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
            maskContextLocation.move(targetRect.x(), targetRect.y());
    } else {
        if (maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
            maskContextLocation.move(-targetRect.x(), -targetRect.y());
    }

    GraphicsContext* maskImageContext = maskImage->context();
    ASSERT(maskImageContext);

    maskImageContext->save();
    maskImageContext->translate(-maskContextLocation.x(), -maskContextLocation.y());

    if (maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        maskImageContext->save();
        maskImageContext->scale(FloatSize(targetRect.width(), targetRect.height()));
    }

    // Render subtree into ImageBuffer
    for (Node* n = firstChild(); n; n = n->nextSibling()) {
        SVGElement* elem = 0;
        if (n->isSVGElement())
            elem = static_cast<SVGElement*>(n);
        if (!elem || !elem->isStyled())
            continue;

        SVGStyledElement* e = static_cast<SVGStyledElement*>(elem);
        RenderObject* item = e->renderer();
        if (!item)
            continue;

        renderSubtreeToImage(maskImage.get(), item);
    }

    if (maskContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        maskImageContext->restore();

    maskImageContext->restore();
    return maskImage.release();
}
 
RenderObject* SVGMaskElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    RenderSVGContainer* maskContainer = new (arena) RenderSVGContainer(this);
    maskContainer->setDrawsContents(false);
    return maskContainer;
}

SVGResource* SVGMaskElement::canvasResource()
{
    if (!m_masker)
        m_masker = SVGResourceMasker::create(this);
    return m_masker.get();
}

}

#endif // ENABLE(SVG)
