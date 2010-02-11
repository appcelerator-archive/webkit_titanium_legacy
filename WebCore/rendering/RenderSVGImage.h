/*
    Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
    Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
    Copyright (C) 2007 Rob Buis <buis@kde.org>
    Copyright (C) 2009 Google, Inc.

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

#ifndef RenderSVGImage_h
#define RenderSVGImage_h

#if ENABLE(SVG)
#include "AffineTransform.h"
#include "FloatRect.h"
#include "RenderImage.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGRenderSupport.h"

namespace WebCore {

    class SVGImageElement;

    class RenderSVGImage : public RenderImage, protected SVGRenderBase {
    public:
        RenderSVGImage(SVGImageElement*);

    private:
        virtual const SVGRenderBase* toSVGRenderBase() const { return this; }
        virtual const char* renderName() const { return "RenderSVGImage"; }
        virtual bool isSVGImage() const { return true; }

        virtual const AffineTransform& localToParentTransform() const { return m_localTransform; }

        virtual FloatRect objectBoundingBox() const;
        virtual FloatRect strokeBoundingBox() const { return m_localBounds; }
        virtual FloatRect repaintRectInLocalCoordinates() const;

        virtual IntRect clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer);
        virtual void computeRectForRepaint(RenderBoxModelObject* repaintContainer, IntRect&, bool fixed = false);

        virtual void mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool useTransforms, bool fixed, TransformState&) const;

        virtual void absoluteRects(Vector<IntRect>&, int tx, int ty);
        virtual void absoluteQuads(Vector<FloatQuad>&);
        virtual void addFocusRingRects(Vector<IntRect>&, int tx, int ty);

        virtual void imageChanged(WrappedImagePtr, const IntRect* = 0);
        
        virtual void layout();
        virtual void paint(PaintInfo&, int parentX, int parentY);

        virtual bool requiresLayer() const { return false; }

        virtual bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction);
        virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

        virtual AffineTransform localTransform() const { return m_localTransform; }

        AffineTransform m_localTransform;
        FloatRect m_localBounds;
        mutable FloatRect m_cachedLocalRepaintRect;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // RenderSVGImage_h

// vim:ts=4:noet
