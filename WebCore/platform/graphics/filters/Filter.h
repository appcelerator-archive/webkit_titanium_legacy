/*
 *  Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 *
 *   This library is free software; you can redistribute it and/or
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
 *  aint with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef Filter_h
#define Filter_h

#if ENABLE(FILTERS)
#include "FloatRect.h"
#include "FloatSize.h"
#include "ImageBuffer.h"
#include "StringHash.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    class FilterEffect;

    class Filter : public RefCounted<Filter> {
    public:
        virtual ~Filter() { }

        void setSourceImage(PassOwnPtr<ImageBuffer> sourceImage) { m_sourceImage = sourceImage; }
        ImageBuffer* sourceImage() { return m_sourceImage.get(); }

        FloatSize filterResolution() const { return m_filterResolution; }
        void setFilterResolution(const FloatSize& filterResolution) { m_filterResolution = filterResolution; }

        virtual FloatRect sourceImageRect() const = 0;
        virtual FloatRect filterRegion() const = 0;

        // SVG specific
        virtual void calculateEffectSubRegion(FilterEffect*) { }

        virtual FloatSize maxImageSize() const = 0;
        virtual bool effectBoundingBoxMode() const = 0;

    private:
        OwnPtr<ImageBuffer> m_sourceImage;
        FloatSize m_filterResolution;
    };

} // namespace WebCore

#endif // ENABLE(FILTERS)

#endif // Filter_h
