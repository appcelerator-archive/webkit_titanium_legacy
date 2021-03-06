/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(FILE_READER) || ENABLE(FILE_WRITER)

#include "FileStream.h"

#include "Blob.h"
#include "PlatformString.h"

namespace WebCore {

FileStream::FileStream(FileStreamClient* client)
    : m_client(client)
    , m_handle(invalidPlatformFileHandle)
{
}

FileStream::~FileStream()
{
    ASSERT(!isHandleValid(m_handle));
}

void FileStream::start()
{
    ASSERT(!isMainThread());
    m_client->didStart();
}

void FileStream::openForRead(Blob*)
{
    ASSERT(!isMainThread());
    // FIXME: to be implemented.
}

void FileStream::openForWrite(const String&)
{
    ASSERT(!isMainThread());
    // FIXME: to be implemented.
}

void FileStream::close()
{
    ASSERT(!isMainThread());
    if (isHandleValid(m_handle))
        closeFile(m_handle);
}

void FileStream::read(char*, int)
{
    ASSERT(!isMainThread());
    // FIXME: to be implemented.
}

void FileStream::write(Blob*, long long, int)
{
    ASSERT(!isMainThread());
    // FIXME: to be implemented.
}

void FileStream::truncate(long long)
{
    ASSERT(!isMainThread());
    // FIXME: to be implemented.
}

} // namespace WebCore

#endif // ENABLE(FILE_WRITER) || ENABLE_FILE_READER)
