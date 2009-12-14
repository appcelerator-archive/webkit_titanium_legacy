/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008, 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Brent Fulgham <bfulgham@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#include "config.h"
#include "GraphicsContext.h"

#if PLATFORM(CAIRO)

#include "CairoPath.h"
#include "FEGaussianBlur.h"
#include "FloatRect.h"
#include "Font.h"
#include "ImageBuffer.h"
#include "ImageBufferFilter.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "Path.h"
#include "Pattern.h"
#include "SimpleFontData.h"
#include "SourceGraphic.h"
#include "TransformationMatrix.h"

#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <wtf/MathExtras.h>

#if PLATFORM(GTK)
#include <gdk/gdk.h>
#include <pango/pango.h>
#elif PLATFORM(WIN)
#include <cairo-win32.h>
#endif
#include "GraphicsContextPlatformPrivateCairo.h"
#include "GraphicsContextPrivate.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace WebCore {

static inline void setColor(cairo_t* cr, const Color& col)
{
    float red, green, blue, alpha;
    col.getRGBA(red, green, blue, alpha);
    cairo_set_source_rgba(cr, red, green, blue, alpha);
}

static inline void setPlatformFill(GraphicsContext* context, cairo_t* cr, GraphicsContextPrivate* gcp)
{
    cairo_save(cr);
    if (gcp->state.fillPattern) {
        TransformationMatrix affine;
        cairo_set_source(cr, gcp->state.fillPattern->createPlatformPattern(affine));
    } else if (gcp->state.fillGradient)
        cairo_set_source(cr, gcp->state.fillGradient->platformGradient());
    else
        setColor(cr, context->fillColor());
    cairo_clip_preserve(cr);
    cairo_paint_with_alpha(cr, gcp->state.globalAlpha);
    cairo_restore(cr);
}

static inline void setPlatformStroke(GraphicsContext* context, cairo_t* cr, GraphicsContextPrivate* gcp)
{
    cairo_save(cr);
    if (gcp->state.strokePattern) {
        TransformationMatrix affine;
        cairo_set_source(cr, gcp->state.strokePattern->createPlatformPattern(affine));
    } else if (gcp->state.strokeGradient)
        cairo_set_source(cr, gcp->state.strokeGradient->platformGradient());
    else  {
        Color strokeColor = colorWithOverrideAlpha(context->strokeColor().rgb(), context->strokeColor().alpha() / 255.f * gcp->state.globalAlpha);
        setColor(cr, strokeColor);
    }
    if (gcp->state.globalAlpha < 1.0f && (gcp->state.strokePattern || gcp->state.strokeGradient)) {
        cairo_push_group(cr);
        cairo_paint_with_alpha(cr, gcp->state.globalAlpha);
        cairo_pop_group_to_source(cr);
    }
    cairo_stroke_preserve(cr);
    cairo_restore(cr);
}

// A fillRect helper
static inline void fillRectSourceOver(cairo_t* cr, const FloatRect& rect, const Color& col)
{
    setColor(cr, col);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_fill(cr);
}

static inline void copyContextProperties(cairo_t* srcCr, cairo_t* dstCr)
{
    cairo_set_antialias(dstCr, cairo_get_antialias(srcCr));

    size_t dashCount = cairo_get_dash_count(srcCr);
    Vector<double> dashes(dashCount);

    double offset;
    cairo_get_dash(srcCr, dashes.data(), &offset);
    cairo_set_dash(dstCr, dashes.data(), dashCount, offset);
    cairo_set_line_cap(dstCr, cairo_get_line_cap(srcCr));
    cairo_set_line_join(dstCr, cairo_get_line_join(srcCr));
    cairo_set_line_width(dstCr, cairo_get_line_width(srcCr));
    cairo_set_miter_limit(dstCr, cairo_get_miter_limit(srcCr));
    cairo_set_fill_rule(dstCr, cairo_get_fill_rule(srcCr));
}

void GraphicsContext::calculateShadowBufferDimensions(IntSize& shadowBufferSize, FloatRect& shadowRect, float& kernelSize, const FloatRect& sourceRect, const IntSize& shadowSize, int shadowBlur)
{
#if ENABLE(FILTERS)
    // calculate the kernel size according to the HTML5 canvas shadow specification
    kernelSize = (shadowBlur < 8 ? shadowBlur / 2.f : sqrt(shadowBlur * 2.f));
    int blurRadius = ceil(kernelSize);

    shadowBufferSize = IntSize(sourceRect.width() + blurRadius * 2, sourceRect.height() + blurRadius * 2);

    // determine dimensions of shadow rect
    shadowRect = FloatRect(sourceRect.location(), shadowBufferSize);
    shadowRect.move(shadowSize.width() - kernelSize, shadowSize.height() - kernelSize);
#endif
}

static inline void drawPathShadow(GraphicsContext* context, GraphicsContextPrivate* gcp, bool fillShadow, bool strokeShadow)
{
#if ENABLE(FILTERS)
    IntSize shadowSize;
    int shadowBlur;
    Color shadowColor;
    if (!context->getShadow(shadowSize, shadowBlur, shadowColor))
        return;
    
    // Calculate filter values to create appropriate shadow.
    cairo_t* cr = context->platformContext();
    cairo_path_t* path = cairo_copy_path(cr);
    double x0, x1, y0, y1;
    if (strokeShadow)
        cairo_stroke_extents(cr, &x0, &y0, &x1, &y1);
    else
        cairo_fill_extents(cr, &x0, &y0, &x1, &y1);
    FloatRect rect(x0, y0, x1 - x0, y1 - y0);

    IntSize shadowBufferSize;
    FloatRect shadowRect;
    float kernelSize = 0;
    GraphicsContext::calculateShadowBufferDimensions(shadowBufferSize, shadowRect, kernelSize, rect, shadowSize, shadowBlur);

    // Create suitably-sized ImageBuffer to hold the shadow.
    OwnPtr<ImageBuffer> shadowBuffer = ImageBuffer::create(shadowBufferSize);

    // Draw shadow into a new ImageBuffer.
    cairo_t* shadowContext = shadowBuffer->context()->platformContext();
    copyContextProperties(cr, shadowContext);
    cairo_translate(shadowContext, -rect.x() + kernelSize, -rect.y() + kernelSize);
    cairo_new_path(shadowContext);
    cairo_append_path(shadowContext, path);

    if (fillShadow)
        setPlatformFill(context, shadowContext, gcp);
    if (strokeShadow)
        setPlatformStroke(context, shadowContext, gcp);

    context->createPlatformShadow(shadowBuffer.release(), shadowColor, shadowRect, kernelSize);
#endif
}

GraphicsContext::GraphicsContext(PlatformGraphicsContext* cr)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate)
{
    m_data->cr = cairo_reference(cr);
    m_data->syncContext(cr);
    setPaintingDisabled(!cr);
}

GraphicsContext::~GraphicsContext()
{
    destroyGraphicsContextPrivate(m_common);
    delete m_data;
}

TransformationMatrix GraphicsContext::getCTM() const
{
    cairo_t* cr = platformContext();
    cairo_matrix_t m;
    cairo_get_matrix(cr, &m);
    return TransformationMatrix(m.xx, m.yx, m.xy, m.yy, m.x0, m.y0);
}

cairo_t* GraphicsContext::platformContext() const
{
    return m_data->cr;
}

void GraphicsContext::savePlatformState()
{
    cairo_save(m_data->cr);
    m_data->save();
}

void GraphicsContext::restorePlatformState()
{
    cairo_restore(m_data->cr);
    m_data->restore();
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_save(cr);

    if (fillColor().alpha())
        fillRectSourceOver(cr, rect, fillColor());

    if (strokeStyle() != NoStroke) {
        setColor(cr, strokeColor());
        FloatRect r(rect);
        r.inflate(-.5f);
        cairo_rectangle(cr, r.x(), r.y(), r.width(), r.height());
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    }

    cairo_restore(cr);
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    StrokeStyle style = strokeStyle();
    if (style == NoStroke)
        return;

    cairo_t* cr = m_data->cr;
    cairo_save(cr);

    float width = strokeThickness();
    if (width < 1)
        width = 1;

    FloatPoint p1 = point1;
    FloatPoint p2 = point2;
    bool isVerticalLine = (p1.x() == p2.x());

    adjustLineToPixelBoundaries(p1, p2, width, style);
    cairo_set_line_width(cr, width);

    int patWidth = 0;
    switch (style) {
    case NoStroke:
    case SolidStroke:
        break;
    case DottedStroke:
        patWidth = static_cast<int>(width);
        break;
    case DashedStroke:
        patWidth = 3*static_cast<int>(width);
        break;
    }

    setColor(cr, strokeColor());

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

    if (patWidth) {
        // Do a rect fill of our endpoints.  This ensures we always have the
        // appearance of being a border.  We then draw the actual dotted/dashed line.
        if (isVerticalLine) {
            fillRectSourceOver(cr, FloatRect(p1.x() - width/2, p1.y() - width, width, width), strokeColor());
            fillRectSourceOver(cr, FloatRect(p2.x() - width/2, p2.y(), width, width), strokeColor());
        } else {
            fillRectSourceOver(cr, FloatRect(p1.x() - width, p1.y() - width/2, width, width), strokeColor());
            fillRectSourceOver(cr, FloatRect(p2.x(), p2.y() - width/2, width, width), strokeColor());
        }

        // Example: 80 pixels with a width of 30 pixels.
        // Remainder is 20.  The maximum pixels of line we could paint
        // will be 50 pixels.
        int distance = (isVerticalLine ? (point2.y() - point1.y()) : (point2.x() - point1.x())) - 2*static_cast<int>(width);
        int remainder = distance%patWidth;
        int coverage = distance-remainder;
        int numSegments = coverage/patWidth;

        float patternOffset = 0;
        // Special case 1px dotted borders for speed.
        if (patWidth == 1)
            patternOffset = 1.0;
        else {
            bool evenNumberOfSegments = !(numSegments % 2);
            if (remainder)
                evenNumberOfSegments = !evenNumberOfSegments;
            if (evenNumberOfSegments) {
                if (remainder) {
                    patternOffset += patWidth - remainder;
                    patternOffset += remainder / 2;
                } else
                    patternOffset = patWidth / 2;
            } else if (!evenNumberOfSegments) {
                if (remainder)
                    patternOffset = (patWidth - remainder) / 2;
            }
        }

        double dash = patWidth;
        cairo_set_dash(cr, &dash, 1, patternOffset);
    }

    cairo_move_to(cr, p1.x(), p1.y());
    cairo_line_to(cr, p2.x(), p2.y());

    cairo_stroke(cr);
    cairo_restore(cr);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_save(cr);
    float yRadius = .5 * rect.height();
    float xRadius = .5 * rect.width();
    cairo_translate(cr, rect.x() + xRadius, rect.y() + yRadius);
    cairo_scale(cr, xRadius, yRadius);
    cairo_arc(cr, 0., 0., 1., 0., 2 * M_PI);
    cairo_restore(cr);

    if (fillColor().alpha()) {
        setColor(cr, fillColor());
        cairo_fill_preserve(cr);
    }

    if (strokeStyle() != NoStroke) {
        setColor(cr, strokeColor());
        cairo_set_line_width(cr, strokeThickness());
        cairo_stroke(cr);
    }

    cairo_new_path(cr);
}

void GraphicsContext::strokeArc(const IntRect& rect, int startAngle, int angleSpan)
{
    if (paintingDisabled() || strokeStyle() == NoStroke)
        return;

    int x = rect.x();
    int y = rect.y();
    float w = rect.width();
    float h = rect.height();
    float scaleFactor = h / w;
    float reverseScaleFactor = w / h;

    float hRadius = w / 2;
    float vRadius = h / 2;
    float fa = startAngle;
    float falen =  fa + angleSpan;

    cairo_t* cr = m_data->cr;
    cairo_save(cr);

    if (w != h)
        cairo_scale(cr, 1., scaleFactor);

    cairo_arc_negative(cr, x + hRadius, (y + vRadius) * reverseScaleFactor, hRadius, -fa * M_PI/180, -falen * M_PI/180);

    if (w != h)
        cairo_scale(cr, 1., reverseScaleFactor);

    float width = strokeThickness();
    int patWidth = 0;

    switch (strokeStyle()) {
    case DottedStroke:
        patWidth = static_cast<int>(width / 2);
        break;
    case DashedStroke:
        patWidth = 3 * static_cast<int>(width / 2);
        break;
    default:
        break;
    }

    setColor(cr, strokeColor());

    if (patWidth) {
        // Example: 80 pixels with a width of 30 pixels.
        // Remainder is 20.  The maximum pixels of line we could paint
        // will be 50 pixels.
        int distance;
        if (hRadius == vRadius)
            distance = static_cast<int>((M_PI * hRadius) / 2.0);
        else // We are elliptical and will have to estimate the distance
            distance = static_cast<int>((M_PI * sqrtf((hRadius * hRadius + vRadius * vRadius) / 2.0)) / 2.0);

        int remainder = distance % patWidth;
        int coverage = distance - remainder;
        int numSegments = coverage / patWidth;

        float patternOffset = 0.0;
        // Special case 1px dotted borders for speed.
        if (patWidth == 1)
            patternOffset = 1.0;
        else {
            bool evenNumberOfSegments = !(numSegments % 2);
            if (remainder)
                evenNumberOfSegments = !evenNumberOfSegments;
            if (evenNumberOfSegments) {
                if (remainder) {
                    patternOffset += patWidth - remainder;
                    patternOffset += remainder / 2.0;
                } else
                    patternOffset = patWidth / 2.0;
            } else {
                if (remainder)
                    patternOffset = (patWidth - remainder) / 2.0;
            }
        }

        double dash = patWidth;
        cairo_set_dash(cr, &dash, 1, patternOffset);
    }

    cairo_stroke(cr);
    cairo_restore(cr);
}

void GraphicsContext::drawConvexPolygon(size_t npoints, const FloatPoint* points, bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    if (npoints <= 1)
        return;

    cairo_t* cr = m_data->cr;

    cairo_save(cr);
    cairo_set_antialias(cr, shouldAntialias ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE);
    cairo_move_to(cr, points[0].x(), points[0].y());
    for (size_t i = 1; i < npoints; i++)
        cairo_line_to(cr, points[i].x(), points[i].y());
    cairo_close_path(cr);

    if (fillColor().alpha()) {
        setColor(cr, fillColor());
        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
        cairo_fill_preserve(cr);
    }

    if (strokeStyle() != NoStroke) {
        setColor(cr, strokeColor());
        cairo_set_line_width(cr, strokeThickness());
        cairo_stroke(cr);
    }

    cairo_new_path(cr);
    cairo_restore(cr);
}

void GraphicsContext::fillPath()
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;

    cairo_set_fill_rule(cr, fillRule() == RULE_EVENODD ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
    drawPathShadow(this, m_common, true, false);

    setPlatformFill(this, cr, m_common);
    cairo_new_path(cr);
}

void GraphicsContext::strokePath()
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    drawPathShadow(this, m_common, false, true);

    setPlatformStroke(this, cr, m_common);
    cairo_new_path(cr);

}

void GraphicsContext::drawPath()
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;

    cairo_set_fill_rule(cr, fillRule() == RULE_EVENODD ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
    drawPathShadow(this, m_common, true, true);

    setPlatformFill(this, cr, m_common);
    setPlatformStroke(this, cr, m_common);
    cairo_new_path(cr);
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    fillPath();
}

static void drawBorderlessRectShadow(GraphicsContext* context, const FloatRect& rect, const Color& rectColor)
{
#if ENABLE(FILTERS)
    IntSize shadowSize;
    int shadowBlur;
    Color shadowColor;

    if (!context->getShadow(shadowSize, shadowBlur, shadowColor))
        return;

    IntSize shadowBufferSize;
    FloatRect shadowRect;
    float kernelSize = 0;
    GraphicsContext::calculateShadowBufferDimensions(shadowBufferSize, shadowRect, kernelSize, rect, shadowSize, shadowBlur);

    // Draw shadow into a new ImageBuffer
    OwnPtr<ImageBuffer> shadowBuffer = ImageBuffer::create(shadowBufferSize);
    GraphicsContext* shadowContext = shadowBuffer->context();
    shadowContext->fillRect(FloatRect(FloatPoint(kernelSize, kernelSize), rect.size()), rectColor, DeviceColorSpace);

    context->createPlatformShadow(shadowBuffer.release(), shadowColor, shadowRect, kernelSize);
#endif
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    drawBorderlessRectShadow(this, rect, color);
    if (color.alpha())
        fillRectSourceOver(m_data->cr, rect, color);
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    cairo_clip(cr);
    cairo_set_fill_rule(cr, savedFillRule);
    m_data->clip(rect);
}

void GraphicsContext::clipPath(WindRule clipRule)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_set_fill_rule(cr, clipRule == RULE_EVENODD ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
    cairo_clip(cr);
}

void GraphicsContext::drawFocusRing(const Color& color)
{
    if (paintingDisabled())
        return;

    const Vector<IntRect>& rects = focusRingRects();
    unsigned rectCount = rects.size();

    cairo_t* cr = m_data->cr;
    cairo_save(cr);
    cairo_push_group(cr);
    cairo_new_path(cr);

#if PLATFORM(GTK)
    GdkRegion* reg = gdk_region_new();
    for (unsigned i = 0; i < rectCount; i++) {
        GdkRectangle rect = rects[i];
        gdk_region_union_with_rect(reg, &rect);
    }
    gdk_cairo_region(cr, reg);
    gdk_region_destroy(reg);

    setColor(cr, color);
    cairo_set_line_width(cr, 2.0f);
    setPlatformStrokeStyle(DottedStroke);
#else
    int radius = (focusRingWidth() - 1) / 2;
    for (unsigned i = 0; i < rectCount; i++)
        addPath(Path::createRoundedRectangle(rects[i], FloatSize(radius, radius)));

    // Force the alpha to 50%.  This matches what the Mac does with outline rings.
    Color ringColor(color.red(), color.green(), color.blue(), 127);
    setColor(cr, ringColor);
    cairo_set_line_width(cr, focusRingWidth());
    setPlatformStrokeStyle(SolidStroke);
#endif

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_stroke_preserve(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    cairo_fill(cr);

    cairo_pop_group_to_source(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_paint(cr);
    cairo_restore(cr);
}

void GraphicsContext::drawLineForText(const IntPoint& origin, int width, bool printing)
{
    if (paintingDisabled())
        return;

    // This is a workaround for http://bugs.webkit.org/show_bug.cgi?id=15659
    StrokeStyle savedStrokeStyle = strokeStyle();
    setStrokeStyle(SolidStroke);

    IntPoint endPoint = origin + IntSize(width, 0);
    drawLine(origin, endPoint);

    setStrokeStyle(savedStrokeStyle);
}

void GraphicsContext::drawLineForMisspellingOrBadGrammar(const IntPoint& origin, int width, bool grammar)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_save(cr);

    // Convention is green for grammar, red for spelling
    // These need to become configurable
    if (grammar)
        cairo_set_source_rgb(cr, 0, 1, 0);
    else
        cairo_set_source_rgb(cr, 1, 0, 0);

#if PLATFORM(GTK)
    // We ignore most of the provided constants in favour of the platform style
    pango_cairo_show_error_underline(cr, origin.x(), origin.y(), width, cMisspellingLineThickness);
#else
    notImplemented();
#endif

    cairo_restore(cr);
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& frect)
{
    FloatRect result;
    double x = frect.x();
    double y = frect.y();
    cairo_t* cr = m_data->cr;
    cairo_user_to_device(cr, &x, &y);
    x = round(x);
    y = round(y);
    cairo_device_to_user(cr, &x, &y);
    result.setX(static_cast<float>(x));
    result.setY(static_cast<float>(y));
    x = frect.width();
    y = frect.height();
    cairo_user_to_device_distance(cr, &x, &y);
    x = round(x);
    y = round(y);
    cairo_device_to_user_distance(cr, &x, &y);
    result.setWidth(static_cast<float>(x));
    result.setHeight(static_cast<float>(y));
    return result;
}

void GraphicsContext::translate(float x, float y)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_translate(cr, x, y);
    m_data->translate(x, y);
}

IntPoint GraphicsContext::origin()
{
    cairo_matrix_t matrix;
    cairo_t* cr = m_data->cr;
    cairo_get_matrix(cr, &matrix);
    return IntPoint(static_cast<int>(matrix.x0), static_cast<int>(matrix.y0));
}

void GraphicsContext::setPlatformFillColor(const Color& col, ColorSpace colorSpace)
{
    // Cairo contexts can't hold separate fill and stroke colors
    // so we set them just before we actually fill or stroke
}

void GraphicsContext::setPlatformStrokeColor(const Color& col, ColorSpace colorSpace)
{
    // Cairo contexts can't hold separate fill and stroke colors
    // so we set them just before we actually fill or stroke
}

void GraphicsContext::setPlatformStrokeThickness(float strokeThickness)
{
    if (paintingDisabled())
        return;

    cairo_set_line_width(m_data->cr, strokeThickness);
}

void GraphicsContext::setPlatformStrokeStyle(const StrokeStyle& strokeStyle)
{
    static double dashPattern[] = {5.0, 5.0};
    static double dotPattern[] = {1.0, 1.0};

    if (paintingDisabled())
        return;

    switch (strokeStyle) {
    case NoStroke:
        // FIXME: is it the right way to emulate NoStroke?
        cairo_set_line_width(m_data->cr, 0);
        break;
    case SolidStroke:
        cairo_set_dash(m_data->cr, 0, 0, 0);
        break;
    case DottedStroke:
        cairo_set_dash(m_data->cr, dotPattern, 2, 0);
        break;
    case DashedStroke:
        cairo_set_dash(m_data->cr, dashPattern, 2, 0);
        break;
    }
}

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
    notImplemented();
}

void GraphicsContext::concatCTM(const TransformationMatrix& transform)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    const cairo_matrix_t matrix = cairo_matrix_t(transform);
    cairo_transform(cr, &matrix);
    m_data->concatCTM(transform);
}

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness)
{
    if (paintingDisabled())
        return;

    clip(rect);

    Path p;
    FloatRect r(rect);
    // Add outer ellipse
    p.addEllipse(r);
    // Add inner ellipse
    r.inflate(-thickness);
    p.addEllipse(r);
    addPath(p);

    cairo_t* cr = m_data->cr;
    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_clip(cr);
    cairo_set_fill_rule(cr, savedFillRule);
}

void GraphicsContext::clipToImageBuffer(const FloatRect& rect, const ImageBuffer* imageBuffer)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::setPlatformShadow(IntSize const& size, int, Color const&, ColorSpace)
{
    // Cairo doesn't support shadows natively, they are drawn manually in the draw*
    // functions

    if (m_common->state.shadowsIgnoreTransforms) {
        // Meaning that this graphics context is associated with a CanvasRenderingContext
        // We flip the height since CG and HTML5 Canvas have opposite Y axis
        m_common->state.shadowSize = IntSize(size.width(), -size.height());
    }
}

void GraphicsContext::createPlatformShadow(PassOwnPtr<ImageBuffer> buffer, const Color& shadowColor, const FloatRect& shadowRect, float kernelSize)
{
#if ENABLE(FILTERS)
    cairo_t* cr = m_data->cr;

    // draw the shadow without blurring, if kernelSize is zero
    if (!kernelSize) {
        setColor(cr, shadowColor);
        cairo_mask_surface(cr, buffer->image()->nativeImageForCurrentFrame(), shadowRect.x(), shadowRect.y());
        return;
    }

    // limit kernel size to 1000, this is what CG is doing.
    kernelSize = std::min(1000.f, kernelSize);

    // create filter
    RefPtr<Filter> filter = ImageBufferFilter::create();
    filter->setSourceImage(buffer.release());
    RefPtr<FilterEffect> source = SourceGraphic::create();
    source->setScaledSubRegion(FloatRect(FloatPoint(), shadowRect.size()));
    source->setIsAlphaImage(true);
    RefPtr<FilterEffect> blur = FEGaussianBlur::create(source.get(), kernelSize, kernelSize);
    blur->setScaledSubRegion(FloatRect(FloatPoint(), shadowRect.size()));
    blur->apply(filter.get());

    // Mask the filter with the shadow color and draw it to the context.
    // Masking makes it possible to just blur the alpha channel.
    setColor(cr, shadowColor);
    cairo_mask_surface(cr, blur->resultImage()->image()->nativeImageForCurrentFrame(), shadowRect.x(), shadowRect.y());
#endif
}

void GraphicsContext::clearPlatformShadow()
{
    notImplemented();
}

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_push_group(cr);
    m_data->layers.append(opacity);
    m_data->beginTransparencyLayer();
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;

    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, m_data->layers.last());
    m_data->layers.removeLast();
    m_data->endTransparencyLayer();
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;

    cairo_save(cr);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_fill(cr);
    cairo_restore(cr);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float width)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_save(cr);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_set_line_width(cr, width);
    strokePath();
    cairo_restore(cr);
}

void GraphicsContext::setLineCap(LineCap lineCap)
{
    if (paintingDisabled())
        return;

    cairo_line_cap_t cairoCap = CAIRO_LINE_CAP_BUTT;
    switch (lineCap) {
    case ButtCap:
        // no-op
        break;
    case RoundCap:
        cairoCap = CAIRO_LINE_CAP_ROUND;
        break;
    case SquareCap:
        cairoCap = CAIRO_LINE_CAP_SQUARE;
        break;
    }
    cairo_set_line_cap(m_data->cr, cairoCap);
}

void GraphicsContext::setLineDash(const DashArray& dashes, float dashOffset)
{
    cairo_set_dash(m_data->cr, dashes.data(), dashes.size(), dashOffset);
}

void GraphicsContext::setLineJoin(LineJoin lineJoin)
{
    if (paintingDisabled())
        return;

    cairo_line_join_t cairoJoin = CAIRO_LINE_JOIN_MITER;
    switch (lineJoin) {
    case MiterJoin:
        // no-op
        break;
    case RoundJoin:
        cairoJoin = CAIRO_LINE_JOIN_ROUND;
        break;
    case BevelJoin:
        cairoJoin = CAIRO_LINE_JOIN_BEVEL;
        break;
    }
    cairo_set_line_join(m_data->cr, cairoJoin);
}

void GraphicsContext::setMiterLimit(float miter)
{
    if (paintingDisabled())
        return;

    cairo_set_miter_limit(m_data->cr, miter);
}

void GraphicsContext::setAlpha(float alpha)
{
    m_common->state.globalAlpha = alpha;
}

float GraphicsContext::getAlpha()
{
    return m_common->state.globalAlpha;
}

static inline cairo_operator_t toCairoOperator(CompositeOperator op)
{
    switch (op) {
    case CompositeClear:
        return CAIRO_OPERATOR_CLEAR;
    case CompositeCopy:
        return CAIRO_OPERATOR_SOURCE;
    case CompositeSourceOver:
        return CAIRO_OPERATOR_OVER;
    case CompositeSourceIn:
        return CAIRO_OPERATOR_IN;
    case CompositeSourceOut:
        return CAIRO_OPERATOR_OUT;
    case CompositeSourceAtop:
        return CAIRO_OPERATOR_ATOP;
    case CompositeDestinationOver:
        return CAIRO_OPERATOR_DEST_OVER;
    case CompositeDestinationIn:
        return CAIRO_OPERATOR_DEST_IN;
    case CompositeDestinationOut:
        return CAIRO_OPERATOR_DEST_OUT;
    case CompositeDestinationAtop:
        return CAIRO_OPERATOR_DEST_ATOP;
    case CompositeXOR:
        return CAIRO_OPERATOR_XOR;
    case CompositePlusDarker:
        return CAIRO_OPERATOR_SATURATE;
    case CompositeHighlight:
        // There is no Cairo equivalent for CompositeHighlight.
        return CAIRO_OPERATOR_OVER;
    case CompositePlusLighter:
        return CAIRO_OPERATOR_ADD;
    default:
        return CAIRO_OPERATOR_SOURCE;
    }
}

void GraphicsContext::setCompositeOperation(CompositeOperator op)
{
    if (paintingDisabled())
        return;

    cairo_set_operator(m_data->cr, toCairoOperator(op));
}

void GraphicsContext::beginPath()
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_new_path(cr);
}

void GraphicsContext::addPath(const Path& path)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_path_t* p = cairo_copy_path(path.platformPath()->m_cr);
    cairo_append_path(cr, p);
    cairo_path_destroy(p);
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_path_t* p = cairo_copy_path(path.platformPath()->m_cr);
    cairo_append_path(cr, p);
    cairo_path_destroy(p);
    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    cairo_clip(cr);
    cairo_set_fill_rule(cr, savedFillRule);
    m_data->clip(path);
}

void GraphicsContext::canvasClip(const Path& path)
{
    clip(path);
}

void GraphicsContext::clipOut(const Path& path)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    double x1, y1, x2, y2;
    cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
    cairo_rectangle(cr, x1, y1, x2 - x1, y2 - y1);
    addPath(path);

    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_clip(cr);
    cairo_set_fill_rule(cr, savedFillRule);
}

void GraphicsContext::rotate(float radians)
{
    if (paintingDisabled())
        return;

    cairo_rotate(m_data->cr, radians);
    m_data->rotate(radians);
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;

    cairo_scale(m_data->cr, size.width(), size.height());
    m_data->scale(size);
}

void GraphicsContext::clipOut(const IntRect& r)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    double x1, y1, x2, y2;
    cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
    cairo_rectangle(cr, x1, y1, x2 - x1, y2 - y1);
    cairo_rectangle(cr, r.x(), r.y(), r.width(), r.height());
    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_clip(cr);
    cairo_set_fill_rule(cr, savedFillRule);
}

void GraphicsContext::clipOutEllipseInRect(const IntRect& r)
{
    if (paintingDisabled())
        return;

    Path p;
    p.addEllipse(r);
    clipOut(p);
}

void GraphicsContext::fillRoundedRect(const IntRect& r, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    cairo_t* cr = m_data->cr;
    cairo_save(cr);
    beginPath();
    addPath(Path::createRoundedRectangle(r, topLeft, topRight, bottomLeft, bottomRight));
    setColor(cr, color);
    drawPathShadow(this, m_common, true, false);
    cairo_fill(cr);
    cairo_restore(cr);
}

#if PLATFORM(GTK)
void GraphicsContext::setGdkExposeEvent(GdkEventExpose* expose)
{
    m_data->expose = expose;
}

GdkEventExpose* GraphicsContext::gdkExposeEvent() const
{
    return m_data->expose;
}

GdkDrawable* GraphicsContext::gdkDrawable() const
{
    if (!m_data->expose)
        return 0;

    return GDK_DRAWABLE(m_data->expose->window);
}
#endif

void GraphicsContext::setPlatformShouldAntialias(bool enable)
{
    if (paintingDisabled())
        return;

    // When true, use the default Cairo backend antialias mode (usually this
    // enables standard 'grayscale' antialiasing); false to explicitly disable
    // antialiasing. This is the same strategy as used in drawConvexPolygon().
    cairo_set_antialias(m_data->cr, enable ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE);
}

void GraphicsContext::setImageInterpolationQuality(InterpolationQuality)
{
}

InterpolationQuality GraphicsContext::imageInterpolationQuality() const
{
    return InterpolationDefault;
}

} // namespace WebCore

#endif // PLATFORM(CAIRO)
