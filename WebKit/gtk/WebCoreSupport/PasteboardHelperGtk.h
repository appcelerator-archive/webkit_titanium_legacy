/*
 * Copyright (C) 2007 Luca Bruno <lethalman88@gmail.com>
 * Copyright (C) 2009 Holger Hans Peter Freyther
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

#ifndef PasteboardHelperGtk_h
#define PasteboardHelperGtk_h

#include "Frame.h"
#include "PasteboardHelper.h"

#include <gtk/gtk.h>

using namespace WebCore;

namespace WebKit {

class PasteboardHelperGtk : public PasteboardHelper {
public:
    PasteboardHelperGtk() { }
    virtual GtkClipboard* defaultClipboard();
    virtual GtkClipboard* defaultClipboardForFrame(Frame*);
    virtual GtkClipboard* primaryClipboard();
    virtual GtkClipboard* primaryClipboardForFrame(Frame*);
    virtual void getClipboardContents(GtkClipboard*);
    virtual void writeClipboardContents(GtkClipboard*);
    virtual void fillSelectionData(GtkSelectionData*, guint, DataObjectGtk*);
    virtual void fillDataObject(GtkSelectionData*, guint, DataObjectGtk*);
    virtual GtkTargetList* fullTargetList();
    virtual GtkTargetList* targetListForDataObject(DataObjectGtk* dataObject);
    virtual GtkTargetList* targetListForDragContext(GdkDragContext* context);
};

}

#endif // PasteboardHelperGtk_h
