/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Jan Michael Alonzo
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "AccessibilityUIElement.h"

#include <JavaScriptCore/JSStringRef.h>
#include <wtf/Assertions.h>

#include <atk/atk.h>
#include <gtk/gtk.h>


AccessibilityUIElement::AccessibilityUIElement(PlatformUIElement element)
    : m_element(element)
{
}

AccessibilityUIElement::AccessibilityUIElement(const AccessibilityUIElement& other)
    : m_element(other.m_element)
{
}

AccessibilityUIElement::~AccessibilityUIElement()
{
}

void AccessibilityUIElement::getLinkedUIElements(Vector<AccessibilityUIElement>& elements)
{
    // FIXME: implement
}

void AccessibilityUIElement::getDocumentLinks(Vector<AccessibilityUIElement>&)
{
    // FIXME: implement
}

void AccessibilityUIElement::getChildren(Vector<AccessibilityUIElement>& children)
{
    int count = childrenCount();
    for (int i = 0; i < count; i++) {
        AtkObject* child = atk_object_ref_accessible_child(ATK_OBJECT(m_element), i);
        children.append(AccessibilityUIElement(child));
    }
}

void AccessibilityUIElement::getChildrenWithRange(Vector<AccessibilityUIElement>& elementVector, unsigned start, unsigned end)
{
    for (unsigned i = start; i < end; i++) {
        AtkObject* child = atk_object_ref_accessible_child(ATK_OBJECT(m_element), i);
        elementVector.append(AccessibilityUIElement(child));
    }
}

int AccessibilityUIElement::childrenCount()
{
    if (!m_element)
        return 0;

    ASSERT(ATK_IS_OBJECT(m_element));

    return atk_object_get_n_accessible_children(ATK_OBJECT(m_element));
}

AccessibilityUIElement AccessibilityUIElement::elementAtPoint(int x, int y)
{
    // FIXME: implement
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::getChildAtIndex(unsigned index)
{
    Vector<AccessibilityUIElement> children;
    getChildrenWithRange(children, index, index + 1);

    if (children.size() == 1)
        return children.at(0);

    return 0;
}

JSStringRef AccessibilityUIElement::allAttributes()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfLinkedUIElements()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfDocumentLinks()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

AccessibilityUIElement AccessibilityUIElement::titleUIElement()
{
    // FIXME: implement
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::parentElement()
{
    ASSERT(m_element);
    AtkObject* parent =  atk_object_get_parent(ATK_OBJECT(m_element));

    return parent ? AccessibilityUIElement(parent) : 0;
}

JSStringRef AccessibilityUIElement::attributesOfChildren()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::parameterizedAttributeNames()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::role()
{
    AtkRole role = atk_object_get_role(ATK_OBJECT(m_element));

    if (!role)
        return JSStringCreateWithCharacters(0, 0);

    return JSStringCreateWithUTF8CString(atk_role_get_name(role));
}

JSStringRef AccessibilityUIElement::subrole()
{
    return 0;
}

JSStringRef AccessibilityUIElement::roleDescription()
{
    return 0;
}

JSStringRef AccessibilityUIElement::title()
{
    const gchar* name = atk_object_get_name(ATK_OBJECT(m_element));

    if (!name)
        return JSStringCreateWithCharacters(0, 0);

    return JSStringCreateWithUTF8CString(name);
}

JSStringRef AccessibilityUIElement::description()
{
    const gchar* description = atk_object_get_description(ATK_OBJECT(m_element));

    if (!description)
        return JSStringCreateWithCharacters(0, 0);

    return JSStringCreateWithUTF8CString(description);
}

JSStringRef AccessibilityUIElement::stringValue()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::language()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

double AccessibilityUIElement::x()
{
    int x, y;

    atk_component_get_position(ATK_COMPONENT(m_element), &x, &y, ATK_XY_SCREEN);

    return x;
}

double AccessibilityUIElement::y()
{
    int x, y;

    atk_component_get_position(ATK_COMPONENT(m_element), &x, &y, ATK_XY_SCREEN);

    return y;
}

double AccessibilityUIElement::width()
{
    int width, height;

    atk_component_get_size(ATK_COMPONENT(m_element), &width, &height);

    return width;
}

double AccessibilityUIElement::height()
{
    int width, height;

    atk_component_get_size(ATK_COMPONENT(m_element), &width, &height);

    return height;
}

double AccessibilityUIElement::clickPointX()
{
    return 0.f;
}

double AccessibilityUIElement::clickPointY()
{
    return 0.f;
}

JSStringRef AccessibilityUIElement::orientation() const
{
    return 0;
}

double AccessibilityUIElement::intValue()
{
    GValue value = { 0, { { 0 } } };

    if (!ATK_IS_VALUE(m_element))
        return 0.0f;

    atk_value_get_current_value(ATK_VALUE(m_element), &value);

    if (G_VALUE_HOLDS_DOUBLE(&value))
        return g_value_get_double(&value);
    else if (G_VALUE_HOLDS_INT(&value))
        return static_cast<double>(g_value_get_int(&value));
    else
        return 0.0f;
}

double AccessibilityUIElement::minValue()
{
    GValue value = { 0, { { 0 } } };

    if (!ATK_IS_VALUE(m_element))
        return 0.0f;

    atk_value_get_minimum_value(ATK_VALUE(m_element), &value);

    if (G_VALUE_HOLDS_DOUBLE(&value))
        return g_value_get_double(&value);
    else if (G_VALUE_HOLDS_INT(&value))
        return static_cast<double>(g_value_get_int(&value));
    else
        return 0.0f;
}

double AccessibilityUIElement::maxValue()
{
    GValue value = { 0, { { 0 } } };

    if (!ATK_IS_VALUE(m_element))
        return 0.0f;

    atk_value_get_maximum_value(ATK_VALUE(m_element), &value);

    if (G_VALUE_HOLDS_DOUBLE(&value))
        return g_value_get_double(&value);
    else if (G_VALUE_HOLDS_INT(&value))
        return static_cast<double>(g_value_get_int(&value));
    else
        return 0.0f;
}

JSStringRef AccessibilityUIElement::valueDescription()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

bool AccessibilityUIElement::isEnabled()
{
    // FIXME: implement
    return false;
}


int AccessibilityUIElement::insertionPointLineNumber()
{
    // FIXME: implement
    return 0;
}

bool AccessibilityUIElement::isActionSupported(JSStringRef action)
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isRequired() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isSelected() const
{
    // FIXME: implement
    return false;
}

int AccessibilityUIElement::hierarchicalLevel() const
{
    // FIXME: implement
    return 0;
}

bool AccessibilityUIElement::ariaIsGrabbed() const
{
    return false;
}
 
JSStringRef AccessibilityUIElement::ariaDropEffects() const
{   
    return 0; 
}

bool AccessibilityUIElement::isExpanded() const
{
    // FIXME: implement
    return false;
}

JSStringRef AccessibilityUIElement::attributesOfColumnHeaders()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfRowHeaders()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfColumns()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfRows()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfVisibleCells()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfHeader()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

int AccessibilityUIElement::indexInTable()
{
    // FIXME: implement
    return 0;
}

JSStringRef AccessibilityUIElement::rowIndexRange()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::columnIndexRange()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

int AccessibilityUIElement::lineForIndex(int)
{
    // FIXME: implement
    return 0;
}

JSStringRef AccessibilityUIElement::boundsForRange(unsigned location, unsigned length)
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::stringForRange(unsigned, unsigned) 
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
} 

AccessibilityUIElement AccessibilityUIElement::cellForColumnAndRow(unsigned column, unsigned row)
{
    // FIXME: implement
    return 0;
}

JSStringRef AccessibilityUIElement::selectedTextRange()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

void AccessibilityUIElement::setSelectedTextRange(unsigned location, unsigned length)
{
    // FIXME: implement
}

JSStringRef AccessibilityUIElement::attributeValue(JSStringRef attribute)
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

bool AccessibilityUIElement::isAttributeSettable(JSStringRef attribute)
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isAttributeSupported(JSStringRef attribute)
{
    return false;
}

void AccessibilityUIElement::increment()
{
    // FIXME: implement
}

void AccessibilityUIElement::decrement()
{
    // FIXME: implement
}

void AccessibilityUIElement::showMenu()
{
    // FIXME: implement
}

AccessibilityUIElement AccessibilityUIElement::disclosedRowAtIndex(unsigned index)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::ariaOwnsElementAtIndex(unsigned index)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::ariaFlowToElementAtIndex(unsigned index)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::selectedRowAtIndex(unsigned index)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::disclosedByRow()
{
    return 0;
}

JSStringRef AccessibilityUIElement::accessibilityValue() const
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::documentEncoding()
{
    AtkRole role = atk_object_get_role(ATK_OBJECT(m_element));
    if (role != ATK_ROLE_DOCUMENT_FRAME)
        return JSStringCreateWithCharacters(0, 0);

    return JSStringCreateWithUTF8CString(atk_document_get_attribute_value(ATK_DOCUMENT(m_element), "Encoding"));
}

JSStringRef AccessibilityUIElement::documentURI()
{
    AtkRole role = atk_object_get_role(ATK_OBJECT(m_element));
    if (role != ATK_ROLE_DOCUMENT_FRAME)
        return JSStringCreateWithCharacters(0, 0);

    return JSStringCreateWithUTF8CString(atk_document_get_attribute_value(ATK_DOCUMENT(m_element), "URI"));
}
