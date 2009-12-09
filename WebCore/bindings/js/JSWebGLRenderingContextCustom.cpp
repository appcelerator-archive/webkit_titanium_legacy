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

#if ENABLE(3D_CANVAS)

#include "JSWebGLRenderingContext.h"

#include "WebGLRenderingContext.h"
#include "ExceptionCode.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "JSHTMLCanvasElement.h"
#include "JSHTMLImageElement.h"
#include "JSWebGLBuffer.h"
#include "JSWebGLFloatArray.h"
#include "JSWebGLFramebuffer.h"
#include "JSWebGLIntArray.h"
#include "JSWebGLProgram.h"
#include "JSWebGLRenderbuffer.h"
#include "JSWebGLShader.h"
#include "JSWebGLTexture.h"
#include "JSWebGLUniformLocation.h"
#include "JSWebGLUnsignedByteArray.h"
#include "JSWebKitCSSMatrix.h"
#include "NotImplemented.h"
#include "WebGLBuffer.h"
#include "WebGLGetInfo.h"
#include "WebGLFloatArray.h"
#include "WebGLFramebuffer.h"
#include "WebGLIntArray.h"
#include "WebGLProgram.h"
#include <runtime/Error.h>
#include <wtf/FastMalloc.h>
#include <wtf/OwnFastMallocPtr.h>

using namespace JSC;

namespace WebCore {

JSValue JSWebGLRenderingContext::bufferData(JSC::ExecState* exec, JSC::ArgList const& args)
{
    if (args.size() != 3)
        return throwError(exec, SyntaxError);

    unsigned target = args.at(0).toInt32(exec);
    unsigned usage = args.at(2).toInt32(exec);
    ExceptionCode ec = 0;

    // If argument 1 is a number, we are initializing this buffer to that size
    if (!args.at(1).isObject()) {
        unsigned int count = args.at(1).toInt32(exec);
        static_cast<WebGLRenderingContext*>(impl())->bufferData(target, count, usage, ec);
    } else {
        WebGLArray* array = toWebGLArray(args.at(1));
        static_cast<WebGLRenderingContext*>(impl())->bufferData(target, array, usage, ec);
    }

    if (ec != 0)
        setDOMException(exec, ec);
    return jsUndefined();
}

JSValue JSWebGLRenderingContext::bufferSubData(JSC::ExecState* exec, JSC::ArgList const& args)
{
    if (args.size() != 3)
        return throwError(exec, SyntaxError);

    unsigned target = args.at(0).toInt32(exec);
    unsigned offset = args.at(1).toInt32(exec);
    ExceptionCode ec = 0;
    
    WebGLArray* array = toWebGLArray(args.at(2));
    
    static_cast<WebGLRenderingContext*>(impl())->bufferSubData(target, offset, array, ec);

    if (ec != 0)
        setDOMException(exec, ec);
    return jsUndefined();
}

static JSValue toJS(ExecState* exec, JSDOMGlobalObject* globalObject, const WebGLGetInfo& info)
{
    switch (info.getType()) {
    case WebGLGetInfo::kTypeBool:
        return jsBoolean(info.getBool());
    case WebGLGetInfo::kTypeFloat:
        return jsNumber(exec, info.getFloat());
    case WebGLGetInfo::kTypeLong:
        return jsNumber(exec, info.getLong());
    case WebGLGetInfo::kTypeNull:
        return jsNull();
    case WebGLGetInfo::kTypeString:
        return jsString(exec, info.getString());
    case WebGLGetInfo::kTypeUnsignedLong:
        return jsNumber(exec, info.getUnsignedLong());
    case WebGLGetInfo::kTypeWebGLBuffer:
        return toJS(exec, globalObject, info.getWebGLBuffer());
    case WebGLGetInfo::kTypeWebGLFloatArray:
        return toJS(exec, globalObject, info.getWebGLFloatArray());
    case WebGLGetInfo::kTypeWebGLFramebuffer:
        return toJS(exec, globalObject, info.getWebGLFramebuffer());
    case WebGLGetInfo::kTypeWebGLIntArray:
        return toJS(exec, globalObject, info.getWebGLIntArray());
    // FIXME: implement WebGLObjectArray
    // case WebGLGetInfo::kTypeWebGLObjectArray:
    case WebGLGetInfo::kTypeWebGLProgram:
        return toJS(exec, globalObject, info.getWebGLProgram());
    case WebGLGetInfo::kTypeWebGLRenderbuffer:
        return toJS(exec, globalObject, info.getWebGLRenderbuffer());
    case WebGLGetInfo::kTypeWebGLTexture:
        return toJS(exec, globalObject, info.getWebGLTexture());
    case WebGLGetInfo::kTypeWebGLUnsignedByteArray:
        return toJS(exec, globalObject, info.getWebGLUnsignedByteArray());
    default:
        notImplemented();
        return jsUndefined();
    }
}

enum ObjectType {
    kBuffer, kRenderbuffer, kTexture, kVertexAttrib
};

static JSValue getObjectParameter(JSWebGLRenderingContext* obj, ExecState* exec, const ArgList& args, ObjectType objectType)
{
    if (args.size() != 2)
        return throwError(exec, SyntaxError);

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(obj->impl());
    unsigned target = args.at(0).toInt32(exec);
    if (exec->hadException())
        return jsUndefined();
    unsigned pname = args.at(1).toInt32(exec);
    if (exec->hadException())
        return jsUndefined();
    WebGLGetInfo info;
    switch (objectType) {
    case kBuffer:
        info = context->getBufferParameter(target, pname, ec);
        break;
    case kRenderbuffer:
        info = context->getRenderbufferParameter(target, pname, ec);
        break;
    case kTexture:
        info = context->getTexParameter(target, pname, ec);
        break;
    case kVertexAttrib:
        // target => index
        info = context->getVertexAttrib(target, pname, ec);
        break;
    default:
        notImplemented();
        break;
    }
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }
    return toJS(exec, obj->globalObject(), info);
}

enum WhichProgramCall {
    kProgramParameter, kUniform
};

JSValue JSWebGLRenderingContext::getBufferParameter(ExecState* exec, const ArgList& args)
{
    return getObjectParameter(this, exec, args, kBuffer);
}

JSValue JSWebGLRenderingContext::getFramebufferAttachmentParameter(ExecState* exec, const ArgList& args)
{
    if (args.size() != 3)
        return throwError(exec, SyntaxError);

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(impl());
    unsigned target = args.at(0).toInt32(exec);
    if (exec->hadException())
        return jsUndefined();
    unsigned attachment = args.at(1).toInt32(exec);
    if (exec->hadException())
        return jsUndefined();
    unsigned pname = args.at(2).toInt32(exec);
    if (exec->hadException())
        return jsUndefined();
    WebGLGetInfo info = context->getFramebufferAttachmentParameter(target, attachment, pname, ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }
    return toJS(exec, globalObject(), info);
}

JSValue JSWebGLRenderingContext::getParameter(ExecState* exec, const ArgList& args)
{
    if (args.size() != 1)
        return throwError(exec, SyntaxError);

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(impl());
    unsigned pname = args.at(0).toInt32(exec);
    if (exec->hadException())
        return jsUndefined();
    WebGLGetInfo info = context->getParameter(pname, ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }
    return toJS(exec, globalObject(), info);
}

JSValue JSWebGLRenderingContext::getProgramParameter(ExecState* exec, const ArgList& args)
{
    if (args.size() != 2)
        return throwError(exec, SyntaxError);

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(impl());
    WebGLProgram* program = toWebGLProgram(args.at(0));
    unsigned pname = args.at(1).toInt32(exec);
    if (exec->hadException())
        return jsUndefined();
    WebGLGetInfo info = context->getProgramParameter(program, pname, ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }
    return toJS(exec, globalObject(), info);
}

JSValue JSWebGLRenderingContext::getRenderbufferParameter(ExecState* exec, const ArgList& args)
{
    return getObjectParameter(this, exec, args, kRenderbuffer);
}

JSValue JSWebGLRenderingContext::getShaderParameter(ExecState* exec, const ArgList& args)
{
    if (args.size() != 2)
        return throwError(exec, SyntaxError);

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(impl());
    WebGLShader* shader = toWebGLShader(args.at(0));
    unsigned pname = args.at(1).toInt32(exec);
    if (exec->hadException())
        return jsUndefined();
    WebGLGetInfo info = context->getShaderParameter(shader, pname, ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }
    return toJS(exec, globalObject(), info);
}

JSValue JSWebGLRenderingContext::getTexParameter(ExecState* exec, const ArgList& args)
{
    return getObjectParameter(this, exec, args, kTexture);
}

JSValue JSWebGLRenderingContext::getUniform(ExecState* exec, const ArgList& args)
{
    if (args.size() != 2)
        return throwError(exec, SyntaxError);

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(impl());
    WebGLProgram* program = toWebGLProgram(args.at(0));
    WebGLUniformLocation* loc = toWebGLUniformLocation(args.at(1));
    if (exec->hadException())
        return jsUndefined();
    WebGLGetInfo info = context->getUniform(program, loc, ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }
    return toJS(exec, globalObject(), info);
}

JSValue JSWebGLRenderingContext::getVertexAttrib(ExecState* exec, const ArgList& args)
{
    return getObjectParameter(this, exec, args, kVertexAttrib);
}

// void texImage2DHTML(in unsigned long target, in unsigned long level, in HTMLImageElement image);
JSValue JSWebGLRenderingContext::texImage2D(ExecState* exec, const ArgList& args)
{ 
    if (args.size() < 3)
        return throwError(exec, SyntaxError);

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(impl());    
    unsigned target = args.at(0).toInt32(exec);
    if (exec->hadException())    
        return jsUndefined();
        
    unsigned level = args.at(1).toInt32(exec);
    if (exec->hadException())    
        return jsUndefined();
    
    if (args.size() > 5) {
        // This must be the bare array case.
        if (args.size() != 9)
            return throwError(exec, SyntaxError);
            
        unsigned internalformat = args.at(2).toInt32(exec);
        if (exec->hadException())    
            return jsUndefined();

        unsigned width = args.at(3).toInt32(exec);
        if (exec->hadException())    
            return jsUndefined();

        unsigned height = args.at(4).toInt32(exec);
        if (exec->hadException())    
            return jsUndefined();

        unsigned border = args.at(5).toInt32(exec);
        if (exec->hadException())    
            return jsUndefined();

        unsigned format = args.at(6).toInt32(exec);
        if (exec->hadException())    
            return jsUndefined();

        unsigned type = args.at(7).toInt32(exec);
        if (exec->hadException())    
            return jsUndefined();

        WebGLArray* array = toWebGLArray(args.at(8));
        if (exec->hadException())    
            return jsUndefined();
            
        if (!array)
            return throwError(exec, TypeError);
        
        // FIXME: Need to check to make sure WebGLArray is a WebGLByteArray or WebGLShortArray,
        // depending on the passed type parameter.
        
        context->texImage2D(target, level, internalformat, width, height, border, format, type, array, ec);
        return jsUndefined();
    }
    
    // The image parameter can be a <img> or <canvas> element.
    JSValue value = args.at(2);
    if (!value.isObject())
        return throwError(exec, TypeError);
    JSObject* o = asObject(value);
    
    bool flipY = (args.size() > 3) ? args.at(3).toBoolean(exec) : false;
    bool premultiplyAlpha = (args.size() > 4) ? args.at(3).toBoolean(exec) : false;
    
    if (o->inherits(&JSHTMLImageElement::s_info)) {
        HTMLImageElement* imgElt = static_cast<HTMLImageElement*>(static_cast<JSHTMLElement*>(o)->impl());
        context->texImage2D(target, level, imgElt, flipY, premultiplyAlpha, ec);
    } else if (o->inherits(&JSHTMLCanvasElement::s_info)) {
        HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(static_cast<JSHTMLElement*>(o)->impl());
        context->texImage2D(target, level, canvas, flipY, premultiplyAlpha, ec);
    } else {
        setDOMException(exec, TYPE_MISMATCH_ERR);
    }
    
    return jsUndefined();    
}

// void texSubImage2DHTML(in unsigned long target, in unsigned long level, in unsigned long xoff, in unsigned long yoff, in unsigned long width, in unsigned long height, in HTMLImageElement image);
JSValue JSWebGLRenderingContext::texSubImage2D(ExecState* exec, const ArgList& args)
{ 
    if (args.size() < 7 || args.size() > 9)
        return throwError(exec, SyntaxError);

    WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(impl());    
    unsigned target = args.at(0).toInt32(exec);
    unsigned level = args.at(1).toInt32(exec);
    unsigned xoff = args.at(2).toInt32(exec);
    unsigned yoff = args.at(3).toInt32(exec);
    unsigned width = args.at(4).toInt32(exec);
    unsigned height = args.at(5).toInt32(exec);
    
    // The image parameter can be a <img> or <canvas> element.
    JSValue value = args.at(6);
    if (!value.isObject())
        return throwError(exec, TypeError);
    JSObject* o = asObject(value);
    
    bool flipY = (args.size() > 3) ? args.at(3).toBoolean(exec) : false;
    bool premultiplyAlpha = (args.size() > 4) ? args.at(3).toBoolean(exec) : false;
    
    ExceptionCode ec = 0;
    if (o->inherits(&JSHTMLImageElement::s_info)) {
        HTMLImageElement* imgElt = static_cast<HTMLImageElement*>(static_cast<JSHTMLElement*>(o)->impl());
        context->texSubImage2D(target, level, xoff, yoff, width, height, imgElt, flipY, premultiplyAlpha, ec);
    } else if (o->inherits(&JSHTMLCanvasElement::s_info)) {
        HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(static_cast<JSHTMLElement*>(o)->impl());
        context->texSubImage2D(target, level, xoff, yoff, width, height, canvas, flipY, premultiplyAlpha, ec);
    } else
        ec = TYPE_MISMATCH_ERR;
    
    if (ec != 0)
        setDOMException(exec, ec);
    return jsUndefined();    
}

template<typename T>
void toArray(JSC::ExecState* exec, JSC::JSValue value, T*& array, int& size)
{
    array = 0;
    
    if (!value.isObject())
        return;
        
    JSC::JSObject* object = asObject(value);
    int length = object->get(exec, JSC::Identifier(exec, "length")).toInt32(exec);
    void* tempValues;
    if (!tryFastMalloc(length * sizeof(T)).getValue(tempValues))
        return;
    
    T* values = static_cast<T*>(tempValues);
    for (int i = 0; i < length; ++i) {
        JSC::JSValue v = object->get(exec, i);
        if (exec->hadException())
            return;
        values[i] = static_cast<T>(v.toNumber(exec));
    }

    array = values;
    size = length;
}

enum DataFunctionToCall {
    f_uniform1v, f_uniform2v, f_uniform3v, f_uniform4v,
    f_vertexAttrib1v, f_vertexAttrib2v, f_vertexAttrib3v, f_vertexAttrib4v
};

enum DataFunctionMatrixToCall {
    f_uniformMatrix2fv, f_uniformMatrix3fv, f_uniformMatrix4fv
};

static bool functionForUniform(DataFunctionToCall f)
{
    switch (f) {
    case f_uniform1v:
    case f_uniform2v:
    case f_uniform3v:
    case f_uniform4v:
        return true;
        break;
    default: break;
    }
    return false;
}

static JSC::JSValue dataFunctionf(DataFunctionToCall f, JSC::ExecState* exec, const JSC::ArgList& args, WebGLRenderingContext* context)
{
    if (args.size() != 2)
        return throwError(exec, SyntaxError);
    
    WebGLUniformLocation* location = 0;
    long index = -1;
    
    if (functionForUniform(f))
        location = toWebGLUniformLocation(args.at(0));
    else
        index = args.at(0).toInt32(exec);

    if (exec->hadException())
        return jsUndefined();
        
    RefPtr<WebGLFloatArray> webGLArray = toWebGLFloatArray(args.at(1));
    if (exec->hadException())    
        return jsUndefined();
        
    ExceptionCode ec = 0;
    if (webGLArray) {
        switch(f) {
            case f_uniform1v: context->uniform1fv(location, webGLArray.get(), ec); break;
            case f_uniform2v: context->uniform2fv(location, webGLArray.get(), ec); break;
            case f_uniform3v: context->uniform3fv(location, webGLArray.get(), ec); break;
            case f_uniform4v: context->uniform4fv(location, webGLArray.get(), ec); break;
            case f_vertexAttrib1v: context->vertexAttrib1fv(index, webGLArray.get()); break;
            case f_vertexAttrib2v: context->vertexAttrib2fv(index, webGLArray.get()); break;
            case f_vertexAttrib3v: context->vertexAttrib3fv(index, webGLArray.get()); break;
            case f_vertexAttrib4v: context->vertexAttrib4fv(index, webGLArray.get()); break;
        }
        if (ec != 0)
            setDOMException(exec, ec);
        return jsUndefined();
    }
    
    float* array;
    int size;
    toArray<float>(exec, args.at(1), array, size);
    
    if (!array)
        return throwError(exec, TypeError);

    switch(f) {
        case f_uniform1v: context->uniform1fv(location, array, size, ec); break;
        case f_uniform2v: context->uniform2fv(location, array, size, ec); break;
        case f_uniform3v: context->uniform3fv(location, array, size, ec); break;
        case f_uniform4v: context->uniform4fv(location, array, size, ec); break;
        case f_vertexAttrib1v: context->vertexAttrib1fv(index, array, size); break;
        case f_vertexAttrib2v: context->vertexAttrib2fv(index, array, size); break;
        case f_vertexAttrib3v: context->vertexAttrib3fv(index, array, size); break;
        case f_vertexAttrib4v: context->vertexAttrib4fv(index, array, size); break;
    }
    if (ec != 0)
        setDOMException(exec, ec);
    return jsUndefined();
}

static JSC::JSValue dataFunctioni(DataFunctionToCall f, JSC::ExecState* exec, const JSC::ArgList& args, WebGLRenderingContext* context)
{
    if (args.size() != 2)
        return throwError(exec, SyntaxError);

    WebGLUniformLocation* location = toWebGLUniformLocation(args.at(0));
  
    if (exec->hadException())
        return jsUndefined();
        
    RefPtr<WebGLIntArray> webGLArray = toWebGLIntArray(args.at(1));
    if (exec->hadException())    
        return jsUndefined();
        
    ExceptionCode ec = 0;
    if (webGLArray) {
        switch(f) {
            case f_uniform1v: context->uniform1iv(location, webGLArray.get(), ec); break;
            case f_uniform2v: context->uniform2iv(location, webGLArray.get(), ec); break;
            case f_uniform3v: context->uniform3iv(location, webGLArray.get(), ec); break;
            case f_uniform4v: context->uniform4iv(location, webGLArray.get(), ec); break;
            default: break;
        }
        if (ec != 0)
            setDOMException(exec, ec);
        return jsUndefined();
    }
    
    int* array;
    int size;
    toArray<int>(exec, args.at(1), array, size);
    
    if (!array)
        return throwError(exec, TypeError);

    switch(f) {
        case f_uniform1v: context->uniform1iv(location, array, size, ec); break;
        case f_uniform2v: context->uniform2iv(location, array, size, ec); break;
        case f_uniform3v: context->uniform3iv(location, array, size, ec); break;
        case f_uniform4v: context->uniform4iv(location, array, size, ec); break;
        default: break;
    }
    if (ec != 0)
        setDOMException(exec, ec);
    return jsUndefined();
}

static JSC::JSValue dataFunctionMatrix(DataFunctionMatrixToCall f, JSC::ExecState* exec, const JSC::ArgList& args, WebGLRenderingContext* context)
{
    if (args.size() != 3)
        return throwError(exec, SyntaxError);

    WebGLUniformLocation* location = toWebGLUniformLocation(args.at(0));

    if (exec->hadException())    
        return jsUndefined();
        
    bool transpose = args.at(1).toBoolean(exec);
    if (exec->hadException())    
        return jsUndefined();
        
    RefPtr<WebGLFloatArray> webGLArray = toWebGLFloatArray(args.at(2));
    if (exec->hadException())    
        return jsUndefined();
        
    ExceptionCode ec = 0;
    if (webGLArray) {
        switch(f) {
            case f_uniformMatrix2fv: context->uniformMatrix2fv(location, transpose, webGLArray.get(), ec); break;
            case f_uniformMatrix3fv: context->uniformMatrix3fv(location, transpose, webGLArray.get(), ec); break;
            case f_uniformMatrix4fv: context->uniformMatrix4fv(location, transpose, webGLArray.get(), ec); break;
        }
        if (ec != 0)
            setDOMException(exec, ec);
        return jsUndefined();
    }
    
    float* array;
    int size;
    toArray<float>(exec, args.at(2), array, size);
    
    if (!array)
        return throwError(exec, TypeError);

    switch(f) {
        case f_uniformMatrix2fv: context->uniformMatrix2fv(location, transpose, array, size, ec); break;
        case f_uniformMatrix3fv: context->uniformMatrix3fv(location, transpose, array, size, ec); break;
        case f_uniformMatrix4fv: context->uniformMatrix4fv(location, transpose, array, size, ec); break;
    }
    if (ec != 0)
        setDOMException(exec, ec);
    return jsUndefined();
}

JSC::JSValue JSWebGLRenderingContext::uniform1fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_uniform1v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniform1iv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctioni(f_uniform1v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniform2fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_uniform2v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniform2iv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctioni(f_uniform2v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniform3fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_uniform3v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniform3iv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctioni(f_uniform3v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniform4fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_uniform4v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniform4iv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctioni(f_uniform4v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniformMatrix2fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionMatrix(f_uniformMatrix2fv, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniformMatrix3fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionMatrix(f_uniformMatrix3fv, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniformMatrix4fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionMatrix(f_uniformMatrix4fv, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::vertexAttrib1fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_vertexAttrib1v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::vertexAttrib2fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_vertexAttrib2v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::vertexAttrib3fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_vertexAttrib3v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::vertexAttrib4fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_vertexAttrib4v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
