/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>
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
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(FILTERS)
#include "FEComponentTransfer.h"

#include "CanvasPixelArray.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageData.h"
#include <math.h>

namespace WebCore {

typedef void (*TransferType)(unsigned char*, const ComponentTransferFunction&);

FEComponentTransfer::FEComponentTransfer(FilterEffect* in, const ComponentTransferFunction& redFunc, 
    const ComponentTransferFunction& greenFunc, const ComponentTransferFunction& blueFunc, const ComponentTransferFunction& alphaFunc)
    : FilterEffect()
    , m_in(in)
    , m_redFunc(redFunc)
    , m_greenFunc(greenFunc)
    , m_blueFunc(blueFunc)
    , m_alphaFunc(alphaFunc)
{
}

PassRefPtr<FEComponentTransfer> FEComponentTransfer::create(FilterEffect* in, const ComponentTransferFunction& redFunc, 
    const ComponentTransferFunction& greenFunc, const ComponentTransferFunction& blueFunc, const ComponentTransferFunction& alphaFunc)
{
    return adoptRef(new FEComponentTransfer(in, redFunc, greenFunc, blueFunc, alphaFunc));
}

ComponentTransferFunction FEComponentTransfer::redFunction() const
{
    return m_redFunc;
}

void FEComponentTransfer::setRedFunction(const ComponentTransferFunction& func)
{
    m_redFunc = func;
}

ComponentTransferFunction FEComponentTransfer::greenFunction() const
{
    return m_greenFunc;
}

void FEComponentTransfer::setGreenFunction(const ComponentTransferFunction& func)
{
    m_greenFunc = func;
}

ComponentTransferFunction FEComponentTransfer::blueFunction() const
{
    return m_blueFunc;
}

void FEComponentTransfer::setBlueFunction(const ComponentTransferFunction& func)
{
    m_blueFunc = func;
}

ComponentTransferFunction FEComponentTransfer::alphaFunction() const
{
    return m_alphaFunc;
}

void FEComponentTransfer::setAlphaFunction(const ComponentTransferFunction& func)
{
    m_alphaFunc = func;
}

static void identity(unsigned char*, const ComponentTransferFunction&)
{
}

static void table(unsigned char* values, const ComponentTransferFunction& transferFunction)
{
    const Vector<float>& tableValues = transferFunction.tableValues;
    unsigned n = tableValues.size();
    if (n < 1)
        return;            
    for (unsigned i = 0; i < 256; ++i) {
        double c = i / 255.0;                
        unsigned k = static_cast<unsigned>(c * (n - 1));
        double v1 = tableValues[k];
        double v2 = tableValues[std::min((k + 1), (n - 1))];
        double val = 255.0 * (v1 + (c * (n - 1) - k) * (v2 - v1));
        val = std::max(0.0, std::min(255.0, val));
        values[i] = static_cast<unsigned char>(val);
    }
}

static void discrete(unsigned char* values, const ComponentTransferFunction& transferFunction)
{
    const Vector<float>& tableValues = transferFunction.tableValues;
    unsigned n = tableValues.size();
    if (n < 1)
        return;
    for (unsigned i = 0; i < 256; ++i) {
        unsigned k = static_cast<unsigned>((i * n) / 255.0);
        k = std::min(k, n - 1);
        double val = 255 * tableValues[k];
        val = std::max(0.0, std::min(255.0, val));
        values[i] = static_cast<unsigned char>(val);
    }
}

static void linear(unsigned char* values, const ComponentTransferFunction& transferFunction)
{
    for (unsigned i = 0; i < 256; ++i) {
        double val = transferFunction.slope * i + 255 * transferFunction.intercept;
        val = std::max(0.0, std::min(255.0, val));
        values[i] = static_cast<unsigned char>(val);
    }
}

static void gamma(unsigned char* values, const ComponentTransferFunction& transferFunction)
{
    for (unsigned i = 0; i < 256; ++i) {
        double val = 255.0 * (transferFunction.amplitude * pow((i / 255.0), transferFunction.exponent) + transferFunction.offset);
        val = std::max(0.0, std::min(255.0, val));
        values[i] = static_cast<unsigned char>(val);
    }
}

void FEComponentTransfer::apply(Filter* filter)
{
    m_in->apply(filter);
    if (!m_in->resultImage())
        return;

    if (!getEffectContext())
        return;

    unsigned char rValues[256], gValues[256], bValues[256], aValues[256];
    for (unsigned i = 0; i < 256; ++i)
        rValues[i] = gValues[i] = bValues[i] = aValues[i] = i;
    unsigned char* tables[] = { rValues, gValues, bValues, aValues };
    ComponentTransferFunction transferFunction[] = {m_redFunc, m_greenFunc, m_blueFunc, m_alphaFunc};
    TransferType callEffect[] = {identity, identity, table, discrete, linear, gamma};

    for (unsigned channel = 0; channel < 4; channel++)
        (*callEffect[transferFunction[channel].type])(tables[channel], transferFunction[channel]);

    IntRect drawingRect = calculateDrawingIntRect(m_in->subRegion());
    RefPtr<ImageData> imageData(m_in->resultImage()->getUnmultipliedImageData(drawingRect));
    CanvasPixelArray* srcPixelArray(imageData->data());

    for (unsigned pixelOffset = 0; pixelOffset < srcPixelArray->length(); pixelOffset += 4) {
        for (unsigned channel = 0; channel < 4; ++channel) {
            unsigned char c = srcPixelArray->get(pixelOffset + channel);
            imageData->data()->set(pixelOffset + channel, tables[channel][c]);
        }
    }

    resultImage()->putUnmultipliedImageData(imageData.get(), IntRect(IntPoint(), resultImage()->size()), IntPoint());
}

void FEComponentTransfer::dump()
{
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
