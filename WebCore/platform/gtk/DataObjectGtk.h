/*
 * Copyright (C) 2009, Martin Robinson
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

#ifndef DataObjectGtk_h
#define DataObjectGtk_h

#include "CString.h"
#include "StringHash.h"
#include "FileList.h"
#include "KURL.h"

#include <wtf/RefCounted.h>
#include <gtk/gtk.h>

namespace WebCore {

class DataObjectGtk : public RefCounted<DataObjectGtk> {
public:
    static PassRefPtr<DataObjectGtk> create()
    {
        return adoptRef(new DataObjectGtk());
    }

    ~DataObjectGtk();

    String text() { return m_text; }
    String markup() { return m_markup; }
    Vector<KURL> uriList() { return m_uriList; }
    GdkPixbuf* image() { return m_image; }
    void setText(const String& newText);
    void setMarkup(const String& newMarkup) { m_markup = newMarkup; }
    void setURIList(const Vector<KURL>& newURIList) {  m_uriList = newURIList; }
    bool hasText() { return !m_text.isEmpty(); }
    bool hasMarkup() { return !m_markup.isEmpty(); }
    bool hasURIList() { return !m_uriList.isEmpty(); }
    bool hasImage() { return m_image; }
    void clearText() { m_text = ""; }
    void clearMarkup() { m_markup = ""; }
    void clearURIList() { m_uriList.clear(); }
    GdkDragContext* dragContext() { return m_dragContext; }

    void setImage(GdkPixbuf*);
    void setDragContext(GdkDragContext*);
    Vector<String> files();
    bool hasURL();
    String url();
    String urlLabel();

    void clearImage();
    void clear();

    static DataObjectGtk* forClipboard(GtkClipboard*);

private:
    DataObjectGtk();

    String m_text;
    String m_markup;
    Vector<KURL> m_uriList;
    GdkPixbuf* m_image;
    GdkDragContext* m_dragContext;
};

}

#endif // DataObjectGtk_h
