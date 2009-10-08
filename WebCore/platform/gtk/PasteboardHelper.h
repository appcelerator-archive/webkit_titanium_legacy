/*
 * Copyright (C) 2007 Luca Bruno <lethalman88@gmail.com>
 * Copyright (C) 2009 Holger Hans Peter Freyther
 * Copyright (C) 2009 Martin Robinson
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
#include <gtk/gtk.h>

namespace WebCore {

// This singleton is necessary because WebKit knows about per-widget clipboards
// and the info identifier for clipboard and drag-and-drop selection data. WebCore
// doesn't have the information, so it must ask WebKit. Can this be in WebCore?
class PasteboardHelper {
public:
    virtual ~PasteboardHelper() {};

    virtual GtkClipboard* defaultClipboard() = 0;
    virtual GtkClipboard* defaultClipboardForFrame(Frame*) = 0;
    virtual GtkClipboard* primaryClipboard() = 0;
    virtual GtkClipboard* primaryClipboardForFrame(Frame*) = 0;
    virtual void getClipboardContents(GtkClipboard*) = 0;
    virtual void writeClipboardContents(GtkClipboard*) = 0;
    virtual void fillSelectionData(GtkSelectionData*, guint, DataObjectGtk*) = 0;
    virtual void fillDataObject(GtkSelectionData*, guint, DataObjectGtk*) = 0;
    virtual GtkTargetList* fullTargetList() = 0;
    virtual GtkTargetList* targetListForDataObject(DataObjectGtk* dataObject) = 0;
    virtual GtkTargetList* targetListForDragContext(GdkDragContext* context) = 0;
    static bool usePrimaryClipboard();
    static void setUsePrimaryClipboard(bool);
    static void setHelper(PasteboardHelper*);
    static GtkClipboard* clipboard();
    static GtkClipboard* clipboardForFrame(Frame*);
    static PasteboardHelper* helper();
};

}

#endif // PasteboardHelper_h
