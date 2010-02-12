/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved. 
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
 *
 */

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FOREIGN_OBJECT)
#include "RenderForeignObject.h"

#include "GraphicsContext.h"
#include "RenderView.h"
#include "SVGForeignObjectElement.h"
#include "SVGRenderSupport.h"
#include "SVGSVGElement.h"
#include "TransformState.h"

namespace WebCore {

RenderForeignObject::RenderForeignObject(SVGForeignObjectElement* node) 
    : RenderSVGBlock(node)
{
}

void RenderForeignObject::paint(PaintInfo& paintInfo, int, int)
{
    if (paintInfo.context->paintingDisabled())
        return;

    PaintInfo childPaintInfo(paintInfo);
    childPaintInfo.context->save();

    applyTransformToPaintInfo(childPaintInfo, localTransform());

    if (SVGRenderBase::isOverflowHidden(this))
        childPaintInfo.context->clip(m_viewport);

    float opacity = style()->opacity();
    if (opacity < 1.0f)
        childPaintInfo.context->beginTransparencyLayer(opacity);

    RenderBlock::paint(childPaintInfo, 0, 0);

    if (opacity < 1.0f)
        childPaintInfo.context->endTransparencyLayer();

    childPaintInfo.context->restore();
}

IntRect RenderForeignObject::clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer)
{
    return SVGRenderBase::clippedOverflowRectForRepaint(this, repaintContainer);
}

void RenderForeignObject::computeRectForRepaint(RenderBoxModelObject* repaintContainer, IntRect& repaintRect, bool fixed)
{
    SVGRenderBase::computeRectForRepaint(this, repaintContainer, repaintRect, fixed);
}

const AffineTransform& RenderForeignObject::localToParentTransform() const
{
    m_localToParentTransform = localTransform();
    m_localToParentTransform.translate(m_viewport.x(), m_viewport.y());
    return m_localToParentTransform;
}

void RenderForeignObject::calcWidth()
{
    // FIXME: Investigate in size rounding issues
    setWidth(static_cast<int>(roundf(m_viewport.width())));
}

void RenderForeignObject::calcHeight()
{
    // FIXME: Investigate in size rounding issues
    setHeight(static_cast<int>(roundf(m_viewport.height())));
}

void RenderForeignObject::layout()
{
    ASSERT(needsLayout());
    ASSERT(!view()->layoutStateEnabled()); // RenderSVGRoot disables layoutState for the SVG rendering tree.

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    SVGForeignObjectElement* foreign = static_cast<SVGForeignObjectElement*>(node());
    m_localTransform = foreign->animatedLocalTransform();

    // Cache viewport boundaries
    FloatPoint viewportLocation(foreign->x().value(foreign), foreign->y().value(foreign));
    m_viewport = FloatRect(viewportLocation, FloatSize(foreign->width().value(foreign), foreign->height().value(foreign)));

    // Set box origin to the foreignObject x/y translation, so positioned objects in XHTML content get correct
    // positions. A regular RenderBoxModelObject would pull this information from RenderStyle - in SVG those
    // properties are ignored for non <svg> elements, so we mimic what happens when specifying them through CSS.

    // FIXME: Investigate in location rounding issues - only affects RenderForeignObject & RenderSVGText
    setLocation(roundedIntPoint(viewportLocation));
    RenderBlock::layout();

    repainter.repaintAfterLayout();
    setNeedsLayout(false);
}

bool RenderForeignObject::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    FloatPoint localPoint = localTransform().inverse().mapPoint(pointInParent);

    // Early exit if local point is not contained in clipped viewport area
    if (SVGRenderBase::isOverflowHidden(this) && !m_viewport.contains(localPoint))
        return false;

    IntPoint roundedLocalPoint = roundedIntPoint(localPoint);
    return RenderBlock::nodeAtPoint(request, result, roundedLocalPoint.x(), roundedLocalPoint.y(), 0, 0, hitTestAction);
}

bool RenderForeignObject::nodeAtPoint(const HitTestRequest&, HitTestResult&, int, int, int, int, HitTestAction)
{
    ASSERT_NOT_REACHED();
    return false;
}

void RenderForeignObject::mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool fixed, bool useTransforms, TransformState& transformState) const
{
    // When crawling up the hierachy starting from foreignObject child content, useTransforms may not be set to true.
    if (!useTransforms)
        useTransforms = true;
    SVGRenderBase::mapLocalToContainer(this, repaintContainer, fixed, useTransforms, transformState);
}

}

#endif
