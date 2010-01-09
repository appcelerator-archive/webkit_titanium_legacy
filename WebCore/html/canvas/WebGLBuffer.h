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

#ifndef WebGLBuffer_h
#define WebGLBuffer_h

#include "CanvasObject.h"
#include "WebGLArrayBuffer.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {
    
    class WebGLBuffer : public CanvasObject {
    public:
        virtual ~WebGLBuffer() { deleteObject(); }
        
        static PassRefPtr<WebGLBuffer> create(WebGLRenderingContext*);
        
        // For querying previously created objects via e.g. getFramebufferAttachmentParameter
        // FIXME: should consider canonicalizing these objects
        static PassRefPtr<WebGLBuffer> create(WebGLRenderingContext*, Platform3DObject);

        bool associateBufferData(unsigned long target, int size);
        bool associateBufferData(unsigned long target, WebGLArray* array);
        bool associateBufferSubData(unsigned long target, long offset, WebGLArray* array);
        
        unsigned byteLength(unsigned long target) const;
        const WebGLArrayBuffer* elementArrayBuffer() const { return m_elementArrayBuffer.get(); }
                        
        // Gets the cached max index for the given type. Returns -1 if
        // none has been set.
        long getCachedMaxIndex(unsigned long type);
        // Sets the cached max index for the given type.
        void setCachedMaxIndex(unsigned long type, long value);

    protected:
        WebGLBuffer(WebGLRenderingContext*);
        WebGLBuffer(WebGLRenderingContext*, Platform3DObject obj);
        
        virtual void _deleteObject(Platform3DObject o);
    
    private:
        RefPtr<WebGLArrayBuffer> m_elementArrayBuffer;
        unsigned m_elementArrayBufferByteLength;
        unsigned m_arrayBufferByteLength;
        bool m_elementArrayBufferCloned;

        // Optimization for index validation. For each type of index
        // (i.e., UNSIGNED_SHORT), cache the maximum index in the
        // entire buffer.
        // 
        // This is sufficient to eliminate a lot of work upon each
        // draw call as long as all bound array buffers are at least
        // that size.
        struct MaxIndexCacheEntry {
            unsigned long type;
            long maxIndex;
        };
        // OpenGL ES 2.0 only has two valid index types (UNSIGNED_BYTE
        // and UNSIGNED_SHORT), but might as well leave open the
        // possibility of adding others.
        MaxIndexCacheEntry m_maxIndexCache[4];
        unsigned m_nextAvailableCacheEntry;

        // Clears all of the cached max indices.
        void clearCachedMaxIndices();
    };
    
} // namespace WebCore

#endif // WebGLBuffer_h
