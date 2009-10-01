/*
 * Copyright (C) 2009, Martin Robinson
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "DataObjectGtk.h"

#include <gtk/gtk.h>

namespace WebCore {


DataObjectGtk::DataObjectGtk()
    : m_image(0)
    , m_dragContext(0)
{

}

DataObjectGtk::~DataObjectGtk()
{
    if (m_image)
        g_object_unref(m_image);
    if (m_dragContext)
        g_object_unref(m_dragContext);
}

Vector<String> DataObjectGtk::files()
{
    Vector<KURL> uris(uriList());
    Vector<String> files;

    for (size_t i = 0; i < uris.size(); i++) {
        if (uris[0].isLocalFile())
            files.append(uris[0].string());
    }

    return files;
}

String DataObjectGtk::url()
{
    Vector<KURL> uris(uriList());
    for (size_t i = 0; i < uris.size(); i++) {
        if (uris[0].isValid())
            return uris[0].string();
    }

    return String();
}

String DataObjectGtk::urlLabel()
{
    if (hasText())
        return text();
    else if (hasURL())
        return url();
    else
        return String();
}

bool DataObjectGtk::hasURL()
{
    return !url().isEmpty();
}

void DataObjectGtk::setText(const String& newText)
{
    // For plain-text data we want to replace the non-breaking space
    // with a regular space, otherwise a lot of text from web pages
    // will come through with strange characters.
    static const UChar nonBreakingSpaceCharacter = 0xA0;
    static const UChar spaceCharacter = ' ';
    m_text = newText;
    m_text.replace(nonBreakingSpaceCharacter, spaceCharacter);
}

void DataObjectGtk::setImage(GdkPixbuf* newImage)
{
    if (m_image)
        g_object_unref(m_image);

    if (newImage)
        g_object_ref(newImage);

    m_image = newImage;
}
void DataObjectGtk::setDragContext(GdkDragContext* newDragContext)
{
    if (m_dragContext)
        g_object_unref(m_dragContext);

    if (newDragContext)
        g_object_ref(newDragContext);

    m_dragContext = newDragContext;
}

void DataObjectGtk::clearImage()
{
    if (m_image)
        g_object_unref(m_image);

    m_image = 0;
}

void DataObjectGtk::clear()
{
    clearText();
    clearMarkup();
    clearURIList();
    clearImage();
}

/*static*/
DataObjectGtk* DataObjectGtk::forClipboard(GtkClipboard* clipboard)
{
    static HashMap<GtkClipboard*, RefPtr<DataObjectGtk> > objectMap;

    if (!objectMap.contains(clipboard)) {
        RefPtr<DataObjectGtk> dataObject = DataObjectGtk::create();
        objectMap.set(clipboard, dataObject);
        return dataObject.get();
    }

    HashMap<GtkClipboard*, RefPtr<DataObjectGtk> >::iterator it = objectMap.find(clipboard);
    return it->second.get();
}

}
