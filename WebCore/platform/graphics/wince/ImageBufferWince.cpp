/*
 * Copyright (C) 2009 Torch Mobile, Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ImageBuffer.h"

#include "Base64.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "ImageData.h"
#include "JPEGEncoder.h"
#include "PNGEncoder.h"
#include "SharedBitmap.h"

namespace WebCore {

class BufferedImage: public Image {

public:
    BufferedImage(const ImageBufferData* data)
        : m_data(data)
    {
    }

    virtual IntSize size() const { return IntSize(m_data->m_bitmap->width(), m_data->m_bitmap->height()); }
    virtual void destroyDecodedData(bool destroyAll = true) {}
    virtual unsigned decodedSize() const { return 0; }
    virtual void draw(GraphicsContext*, const FloatRect& dstRect, const FloatRect& srcRect, CompositeOperator);
    virtual void drawPattern(GraphicsContext*, const FloatRect& srcRect, const TransformationMatrix& patternTransform,
                             const FloatPoint& phase, CompositeOperator, const FloatRect& destRect);

    const ImageBufferData* m_data;
};

void BufferedImage::draw(GraphicsContext* ctxt, const FloatRect& dstRect, const FloatRect& srcRect, CompositeOperator compositeOp)
{
    IntRect intDstRect = enclosingIntRect(dstRect);
    IntRect intSrcRect(srcRect);
    m_data->m_bitmap->draw(ctxt, intDstRect, intSrcRect, compositeOp);
}

void BufferedImage::drawPattern(GraphicsContext* ctxt, const FloatRect& tileRectIn, const TransformationMatrix& patternTransform,
                             const FloatPoint& phase, CompositeOperator op, const FloatRect& destRect)
{
    m_data->m_bitmap->drawPattern(ctxt, tileRectIn, patternTransform, phase, op, destRect, size());
}

ImageBufferData::ImageBufferData(const IntSize& size)
: m_bitmap(SharedBitmap::createInstance(false, size.width(), size.height(), false))
{
    // http://www.w3.org/TR/2009/WD-html5-20090212/the-canvas-element.html#canvaspixelarray
    // "When the canvas is initialized it must be set to fully transparent black."
    m_bitmap->resetPixels(true);
    m_bitmap->setHasAlpha(true);
}

ImageBuffer::ImageBuffer(const IntSize& size, ImageColorSpace colorSpace, bool& success)
    : m_data(size)
    , m_size(size)
{
    // FIXME: colorSpace is not used
    UNUSED_PARAM(colorSpace);

    m_context.set(new GraphicsContext(0));
    m_context->setBitmap(m_data.m_bitmap);
    success = true;
}

ImageBuffer::~ImageBuffer()
{
}

GraphicsContext* ImageBuffer::context() const
{
    return m_context.get();
}

Image* ImageBuffer::image() const
{
    if (!m_image)
        m_image = adoptRef(new BufferedImage(&m_data));

    return m_image.get();
}

template <bool premultiplied> PassRefPtr<ImageData>
static getImageData(const IntRect& rect, const SharedBitmap* bitmap)
{
    PassRefPtr<ImageData> imageData = ImageData::create(rect.width(), rect.height());

    const unsigned char* src = static_cast<const unsigned char*>(bitmap->bytes());
    if (!src)
        return imageData;

    IntRect sourceRect(0, 0, bitmap->width(), bitmap->height());
    sourceRect.intersect(rect);
    if (sourceRect.isEmpty())
        return imageData;

    unsigned char* dst = imageData->data()->data()->data();
    memset(dst, 0, imageData->data()->data()->length());
    src += (sourceRect.y() * bitmap->width() + sourceRect.x()) * 4;
    dst += ((sourceRect.y() - rect.y()) * imageData->width() + sourceRect.x() - rect.x()) * 4;
    int bytesToCopy = sourceRect.width() * 4;
    int srcSkip = (bitmap->width() - sourceRect.width()) * 4;
    int dstSkip = (imageData->width() - sourceRect.width()) * 4;
    const unsigned char* dstEnd = dst + sourceRect.height() * imageData->width() * 4;
    while (dst < dstEnd) {
        const unsigned char* dstRowEnd = dst + bytesToCopy;
        while (dst < dstRowEnd) {
            // Convert ARGB little endian to RGBA big endian
            int blue = *src++;
            int green = *src++;
            int red = *src++;
            int alpha = *src++;
            if (premultiplied) {
                *dst++ = static_cast<unsigned char>((red * alpha + 254) / 255);
                *dst++ = static_cast<unsigned char>((green * alpha + 254) / 255);
                *dst++ = static_cast<unsigned char>((blue * alpha + 254) / 255);
                *dst++ = static_cast<unsigned char>(alpha);
            } else {
                *dst++ = static_cast<unsigned char>(red);
                *dst++ = static_cast<unsigned char>(green);
                *dst++ = static_cast<unsigned char>(blue);
                *dst++ = static_cast<unsigned char>(alpha);
                ++src;
            }
        }
        src += srcSkip;
        dst += dstSkip;
    }

    return imageData;
}

PassRefPtr<ImageData> ImageBuffer::getUnmultipliedImageData(const IntRect& rect) const
{
    return getImageData<false>(rect, m_data.m_bitmap.get());
}

PassRefPtr<ImageData> ImageBuffer::getPremultipliedImageData(const IntRect& rect) const
{
    return getImageData<true>(rect, m_data.m_bitmap.get());
}

template <bool premultiplied>
static void putImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint, SharedBitmap* bitmap)
{
    unsigned char* dst = (unsigned char*)bitmap->bytes();
    if (!dst)
        return;

    IntRect destRect(destPoint, sourceRect.size());
    destRect.intersect(IntRect(0, 0, bitmap->width(), bitmap->height()));

    if (destRect.isEmpty())
        return;

    const unsigned char* src = source->data()->data()->data();
    dst += (destRect.y() * bitmap->width() + destRect.x()) * 4;
    src += (sourceRect.y() * source->width() + sourceRect.x()) * 4;
    int bytesToCopy = destRect.width() * 4;
    int dstSkip = (bitmap->width() - destRect.width()) * 4;
    int srcSkip = (source->width() - destRect.width()) * 4;
    const unsigned char* dstEnd = dst + destRect.height() * bitmap->width() * 4;
    while (dst < dstEnd) {
        const unsigned char* dstRowEnd = dst + bytesToCopy;
        while (dst < dstRowEnd) {
            // Convert RGBA big endian to ARGB little endian
            int red = *src++;
            int green = *src++;
            int blue = *src++;
            int alpha = *src++;
            if (premultiplied) {
                *dst++ = static_cast<unsigned char>(blue * 255 / alpha);
                *dst++ = static_cast<unsigned char>(green * 255 / alpha);
                *dst++ = static_cast<unsigned char>(red * 255 / alpha);
                *dst++ = static_cast<unsigned char>(alpha);
            } else {
                *dst++ = static_cast<unsigned char>(blue);
                *dst++ = static_cast<unsigned char>(green);
                *dst++ = static_cast<unsigned char>(red);
                *dst++ = static_cast<unsigned char>(alpha);
            }
        }
        src += srcSkip;
        dst += dstSkip;
    }
}

void ImageBuffer::putUnmultipliedImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint)
{
    putImageData<false>(source, sourceRect, destPoint, m_data.m_bitmap.get());
}

void ImageBuffer::putPremultipliedImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint)
{
    putImageData<true>(source, sourceRect, destPoint, m_data.m_bitmap.get());
}

String ImageBuffer::toDataURL(const String& mimeType) const
{
    if (!m_data.m_bitmap->bytes())
        return "data:,";

    Vector<char> output;
    const char* header;
    if (mimeType.lower() == "image/png") {
        if (!compressBitmapToPng(m_data.m_bitmap.get(), output))
            return "data:,";
        header = "data:image/png;base64,";
    } else {
        if (!compressBitmapToJpeg(m_data.m_bitmap.get(), output))
            return "data:,";
        header = "data:image/jpeg;base64,";
    }

    Vector<char> base64;
    base64Encode(output, base64);

    output.clear();

    Vector<char> url;
    url.append(header, strlen(header));
    url.append(base64);

    return String(url.data(), url.size());
}

}
