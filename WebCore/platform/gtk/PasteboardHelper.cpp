/*
 * Copyright (C) 2009 Martin Robinson
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

#include "config.h"
#include "PasteboardHelper.h"

#include <gtk/gtk.h>

namespace WebCore {

static RefPtr<PasteboardHelper> helperInstance = 0;
static bool shouldUsePrimaryClipboard = false;

/*static*/
void PasteboardHelper::setHelper(PassRefPtr<PasteboardHelper> newHelper)
{
    helperInstance = newHelper;
}

/*static*/
GtkClipboard* PasteboardHelper::clipboard()
{
    ASSERT(helperInstance);

    if (shouldUsePrimaryClipboard)
        return helperInstance->primaryClipboard();
    else
        return helperInstance->defaultClipboard();
}

/*static*/
GtkClipboard* PasteboardHelper::clipboardForFrame(Frame* frame)
{
    ASSERT(helperInstance);

    if (!frame)
        return clipboard();

    else if (shouldUsePrimaryClipboard)
        return helperInstance->primaryClipboardForFrame(frame);
    else
        return helperInstance->defaultClipboardForFrame(frame);
}

/*static*/
bool PasteboardHelper::usePrimaryClipboard()
{
    return shouldUsePrimaryClipboard;
}

/*static*/
void PasteboardHelper::setUsePrimaryClipboard(bool newUsePrimaryClipboard)
{
    shouldUsePrimaryClipboard = newUsePrimaryClipboard;
}

/*statc*/
PassRefPtr<PasteboardHelper> PasteboardHelper::helper()
{
    ASSERT(helperInstance);
    return helperInstance;
}

}
