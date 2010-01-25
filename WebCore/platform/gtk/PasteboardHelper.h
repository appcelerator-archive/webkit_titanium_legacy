/*
 * Copyright (C) 2007 Luca Bruno <lethalman88@gmail.com>
 * Copyright (C) 2009 Holger Hans Peter Freyther
 * Copyright (C) 2009 Appcelerator, Inc.
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef PasteboardHelper_h
#define PasteboardHelper_h

#include "Frame.h"
#include "DataObjectGtk.h"

#include <wtf/RefCounted.h>

typedef struct _GtkClipboard GtkClipboard;
typedef struct _GtkSelectionData GtkSelectionData;
typedef struct _GtkTargetList GtkTargetList;

namespace WebCore {

// This singleton is necessary because WebKit knows about per-widget clipboards
// and the info identifier for clipboard and drag-and-drop selection data. WebCore
// doesn't have the information, so it must ask WebKit. Can this be in WebCore?
class PasteboardHelper : public RefCounted<PasteboardHelper> {
public:
    virtual ~PasteboardHelper() {};

    virtual GtkClipboard* getCurrentTarget(Frame*) const = 0;
    virtual GtkClipboard* getClipboard(Frame*) const = 0;
    virtual GtkClipboard* getPrimary(Frame*) const = 0;
    virtual GtkTargetList* targetList() const = 0;
    virtual gint getWebViewTargetInfoHtml() const = 0;
    virtual void getClipboardContents(GtkClipboard*) = 0;
    virtual void writeClipboardContents(GtkClipboard*, gpointer data=0) = 0;
    virtual void fillDataObject(GtkSelectionData*, guint, DataObjectGtk*) = 0;
    virtual GtkTargetList* targetListForDragContext(GdkDragContext* context) = 0;
};

}

#endif // PasteboardHelper_h
