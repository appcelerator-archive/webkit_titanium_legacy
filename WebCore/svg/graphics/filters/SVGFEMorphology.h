/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGFEMorphology_h
#define SVGFEMorphology_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "FilterEffect.h"
#include "Filter.h"

namespace WebCore {

    enum MorphologyOperatorType {
        FEMORPHOLOGY_OPERATOR_UNKNOWN = 0,
        FEMORPHOLOGY_OPERATOR_ERODE   = 1,
        FEMORPHOLOGY_OPERATOR_DILATE  = 2
    };

    class FEMorphology : public FilterEffect {
    public:
        static PassRefPtr<FEMorphology> create(FilterEffect*, MorphologyOperatorType, float radiusX, float radiusY);  
        MorphologyOperatorType morphologyOperator() const;
        void setMorphologyOperator(MorphologyOperatorType);

        float radiusX() const;
        void setRadiusX(float);

        float radiusY() const;
        void setRadiusY(float);

        virtual FloatRect uniteChildEffectSubregions(Filter* filter) { return calculateUnionOfChildEffectSubregions(filter, m_in.get()); }
        void apply(Filter*);
        void dump();
        TextStream& externalRepresentation(TextStream& ts) const;

    private:
        FEMorphology(FilterEffect*, MorphologyOperatorType, float radiusX, float radiusY);
        
        RefPtr<FilterEffect> m_in;
        MorphologyOperatorType m_type;
        float m_radiusX;
        float m_radiusY;
    };

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)

#endif // SVGFEMorphology_h
