/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef WKCACFLayer_h
#define WKCACFLayer_h

#if USE(ACCELERATED_COMPOSITING)

#include "StringHash.h"

#include <wtf/RefCounted.h>

#include <QuartzCore/CACFLayer.h>
#include <QuartzCore/CACFVector.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

#include "GraphicsContext.h"
#include "GraphicsLayerCACF.h"
#include "PlatformString.h"
#include "TransformationMatrix.h"

namespace WebCore {

class WKCACFAnimation;
class WKCACFTimingFunction;

class WKCACFLayer : public RefCounted<WKCACFLayer> {
public:
    enum LayerType { Layer, TransformLayer };
    enum FilterType { Linear, Nearest, Trilinear, Lanczos };
    enum ContentsGravityType { Center, Top, Bottom, Left, Right, TopLeft, TopRight, 
                               BottomLeft, BottomRight, Resize, ResizeAspect, ResizeAspectFill };

    static PassRefPtr<WKCACFLayer> create(LayerType, GraphicsLayerCACF* owner = 0);
    static WKCACFLayer* layer(CACFLayerRef layer) { return static_cast<WKCACFLayer*>(CACFLayerGetUserData(layer)); }

    ~WKCACFLayer();

    // Makes this layer the root when the passed context is rendered
    void becomeRootLayerForContext(CACFContextRef);

    static RetainPtr<CFTypeRef> cfValue(float value) { return RetainPtr<CFTypeRef>(AdoptCF, CFNumberCreate(0, kCFNumberFloat32Type, &value)); }
    static RetainPtr<CFTypeRef> cfValue(const TransformationMatrix& value)
    {
        CATransform3D t;
        t.m11 = value.m11();
        t.m12 = value.m12();
        t.m13 = value.m13();
        t.m14 = value.m14(); 
        t.m21 = value.m21();
        t.m22 = value.m22();
        t.m23 = value.m23();
        t.m24 = value.m24(); 
        t.m31 = value.m31();
        t.m32 = value.m32();
        t.m33 = value.m33();
        t.m34 = value.m34(); 
        t.m41 = value.m41();
        t.m42 = value.m42();
        t.m43 = value.m43();
        t.m44 = value.m44(); 
        return RetainPtr<CFTypeRef>(AdoptCF, CACFVectorCreateTransform(t));
    }
    static RetainPtr<CFTypeRef> cfValue(const FloatPoint& value)
    {
        CGPoint p;
        p.x = value.x(); p.y = value.y();
        return RetainPtr<CFTypeRef>(AdoptCF, CACFVectorCreatePoint(p));
    }
    static RetainPtr<CFTypeRef> cfValue(const FloatRect& rect)
    {
        CGRect r;
        r.origin.x = rect.x();
        r.origin.y = rect.y();
        r.size.width = rect.width();
        r.size.height = rect.height();
        CGFloat v[4] = { CGRectGetMinX(r), CGRectGetMinY(r), CGRectGetMaxX(r), CGRectGetMaxY(r) };
        return RetainPtr<CFTypeRef>(AdoptCF, CACFVectorCreate(4, v));
    }
    static RetainPtr<CFTypeRef> cfValue(const Color& color)
    {
        return RetainPtr<CFTypeRef>(AdoptCF, CGColorCreateGenericRGB(color.red(), color.green(), color.blue(), color.alpha()));
    }

    void display(PlatformGraphicsContext*);

    bool isTransformLayer() const;

    void addSublayer(PassRefPtr<WKCACFLayer> sublayer);
    void insertSublayer(PassRefPtr<WKCACFLayer>, size_t index);
    void insertSublayerAboveLayer(PassRefPtr<WKCACFLayer>, const WKCACFLayer* reference);
    void insertSublayerBelowLayer(PassRefPtr<WKCACFLayer>, const WKCACFLayer* reference);
    void replaceSublayer(WKCACFLayer* reference, PassRefPtr<WKCACFLayer>);
    void removeFromSuperlayer();
    static void moveSublayers(WKCACFLayer* fromLayer, WKCACFLayer* toLayer);

    WKCACFLayer* ancestorOrSelfWithSuperlayer(WKCACFLayer*) const;
    
    void setAnchorPoint(const CGPoint& p) { CACFLayerSetAnchorPoint(layer(), p); setNeedsCommit(); }
    CGPoint anchorPoint() const { return CACFLayerGetAnchorPoint(layer()); }

    void setAnchorPointZ(CGFloat z) { CACFLayerSetAnchorPointZ(layer(), z); setNeedsCommit(); }
    CGFloat anchorPointZ() const { return CACFLayerGetAnchorPointZ(layer()); }

    void setBackgroundColor(CGColorRef color) { CACFLayerSetBackgroundColor(layer(), color); setNeedsCommit(); }
    CGColorRef backgroundColor() const { return CACFLayerGetBackgroundColor(layer()); }

    void setBorderColor(CGColorRef color) { CACFLayerSetBorderColor(layer(), color); setNeedsCommit(); }
    CGColorRef borderColor() const { return CACFLayerGetBorderColor(layer()); }

    void setBorderWidth(CGFloat width) { CACFLayerSetBorderWidth(layer(), width); setNeedsCommit(); }
    CGFloat borderWidth() const { return CACFLayerGetBorderWidth(layer()); }

    void setBounds(const CGRect&);
    CGRect bounds() const { return CACFLayerGetBounds(layer()); }

    void setClearsContext(bool clears) { CACFLayerSetClearsContext(layer(), clears); setNeedsCommit(); }
    bool clearsContext() const { return CACFLayerGetClearsContext(layer()); }

    void setContents(CGImageRef contents) { CACFLayerSetContents(layer(), contents); setNeedsCommit(); }
    CGImageRef contents() const { return static_cast<CGImageRef>(const_cast<void*>(CACFLayerGetContents(layer()))); }

    void setContentsRect(const CGRect& contentsRect) { CACFLayerSetContentsRect(layer(), contentsRect); setNeedsCommit(); }
    CGRect contentsRect() const { return CACFLayerGetContentsRect(layer()); }

    void setContentsGravity(ContentsGravityType);
    ContentsGravityType contentsGravity() const;
        
    void setDoubleSided(bool b) { CACFLayerSetDoubleSided(layer(), b); setNeedsCommit(); }
    bool doubleSided() const { return CACFLayerIsDoubleSided(layer()); }

    void setEdgeAntialiasingMask(uint32_t mask) { CACFLayerSetEdgeAntialiasingMask(layer(), mask); setNeedsCommit(); }
    uint32_t edgeAntialiasingMask() const { return CACFLayerGetEdgeAntialiasingMask(layer()); }

    void setFilters(CFArrayRef filters) { CACFLayerSetFilters(layer(), filters); setNeedsCommit(); }
    CFArrayRef filters() const { return CACFLayerGetFilters(layer()); }

    void setFrame(const CGRect&);
    CGRect frame() const { return CACFLayerGetFrame(layer()); }

    void setHidden(bool hidden) { CACFLayerSetHidden(layer(), hidden); setNeedsCommit(); }
    bool isHidden() const { return CACFLayerIsHidden(layer()); }

    void setMasksToBounds(bool b) { CACFLayerSetMasksToBounds(layer(), b); }
    bool masksToBounds() const { return CACFLayerGetMasksToBounds(layer()); }

    void setMagnificationFilter(FilterType);
    FilterType magnificationFilter() const;

    void setMinificationFilter(FilterType);
    FilterType minificationFilter() const;

    void setMinificationFilterBias(float bias) { CACFLayerSetMinificationFilterBias(layer(), bias); }
    float minificationFilterBias() const { return CACFLayerGetMinificationFilterBias(layer()); }

    void setName(const String& name) { CACFLayerSetName(layer(), RetainPtr<CFStringRef>(AdoptCF, name.createCFString()).get()); }
    String name() const { return CACFLayerGetName(layer()); }

    void setNeedsDisplay(const CGRect& dirtyRect);
    void setNeedsDisplay();
    
    void setNeedsDisplayOnBoundsChange(bool needsDisplay) { m_needsDisplayOnBoundsChange = needsDisplay; }

    void setOpacity(float opacity) { CACFLayerSetOpacity(layer(), opacity); setNeedsCommit(); }
    float opacity() const { return CACFLayerGetOpacity(layer()); }

    void setOpaque(bool b) { CACFLayerSetOpaque(layer(), b); setNeedsCommit(); }
    bool opaque() const { return CACFLayerIsOpaque(layer()); }

    void setPosition(const CGPoint& position) { CACFLayerSetPosition(layer(), position); setNeedsCommit(); }
    CGPoint position() const { return CACFLayerGetPosition(layer()); }

    void setZPosition(CGFloat position) { CACFLayerSetZPosition(layer(), position); setNeedsCommit(); }
    CGFloat zPosition() const { return CACFLayerGetZPosition(layer()); }

    void setSpeed(float speed) { CACFLayerSetSpeed(layer(), speed); }
    CFTimeInterval speed() const { CACFLayerGetSpeed(layer()); }

    void setTimeOffset(CFTimeInterval t) { CACFLayerSetTimeOffset(layer(), t); }
    CFTimeInterval timeOffset() const { CACFLayerGetTimeOffset(layer()); }

    WKCACFLayer* rootLayer() const;

    void setSortsSublayers(bool sorts) { CACFLayerSetSortsSublayers(layer(), sorts); setNeedsCommit(); }
    bool sortsSublayers() const { return CACFLayerGetSortsSublayers(layer()); }

    void removeAllSublayers();
    
    void setSublayers(const Vector<RefPtr<WKCACFLayer> >&);

    void setSublayerTransform(const CATransform3D& transform) { CACFLayerSetSublayerTransform(layer(), transform); setNeedsCommit(); }
    CATransform3D sublayerTransform() const { return CACFLayerGetSublayerTransform(layer()); }

    WKCACFLayer* superlayer() const;

    void setTransform(const CATransform3D& transform) { CACFLayerSetTransform(layer(), transform); setNeedsCommit(); }
    CATransform3D transform() const { return CACFLayerGetTransform(layer()); }

    void setGeometryFlipped(bool flipped) { CACFLayerSetGeometryFlipped(layer(), flipped); setNeedsCommit(); }
    bool geometryFlipped() const { return CACFLayerIsGeometryFlipped(layer()); }

#ifndef NDEBUG
    // Print the tree from the root. Also does consistency checks
    void printTree() const;
#endif

private:
    WKCACFLayer(LayerType, GraphicsLayerCACF* owner);

    void setNeedsCommit();
    CACFLayerRef layer() const { return m_layer.get(); }
    size_t numSublayers() const
    {
        CFArrayRef sublayers = CACFLayerGetSublayers(layer());
        return sublayers ? CFArrayGetCount(sublayers) : 0;
    }

    const WKCACFLayer* sublayerAtIndex(int) const;
    
    // Returns the index of the passed layer in this layer's sublayers list
    // or -1 if not found
    int indexOfSublayer(const WKCACFLayer*);

    // This should only be called from removeFromSuperlayer.
    void removeSublayer(const WKCACFLayer*);

#ifndef NDEBUG
    // Print this layer and its children to the console
    void printLayer(int indent) const;
#endif

    RetainPtr<CACFLayerRef> m_layer;
    bool m_needsDisplayOnBoundsChange;
    GraphicsLayerCACF* m_owner;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // WKCACFLayer_h
