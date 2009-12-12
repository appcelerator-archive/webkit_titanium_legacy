/*
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Igalia S.L.
 * Copyright (C) 2009 Jan Alonzo
 *
 * Portions from Mozilla a11y, copyright as follows:
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
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
 */

#include "config.h"
#include "AccessibilityObjectWrapperAtk.h"

#if HAVE(ACCESSIBILITY)

#include "AXObjectCache.h"
#include "AccessibilityListBox.h"
#include "AccessibilityListBoxOption.h"
#include "AccessibilityRenderObject.h"
#include "AccessibilityTable.h"
#include "AccessibilityTableCell.h"
#include "AccessibilityTableColumn.h"
#include "AccessibilityTableRow.h"
#include "AtomicString.h"
#include "CString.h"
#include "Document.h"
#include "DocumentType.h"
#include "Editor.h"
#include "Frame.h"
#include "FrameView.h"
#include "HostWindow.h"
#include "HTMLNames.h"
#include "HTMLTableCaptionElement.h"
#include "HTMLTableElement.h"
#include "InlineTextBox.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "RenderText.h"
#include "TextEncoding.h"

#include <atk/atk.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <libgail-util/gail-util.h>
#include <pango/pango.h>

using namespace WebCore;

static AccessibilityObject* fallbackObject()
{
    static AXObjectCache* fallbackCache = new AXObjectCache;
    static AccessibilityObject* object = 0;
    if (!object) {
        // FIXME: using fallbackCache->getOrCreate(ListBoxOptionRole) is a hack
        object = fallbackCache->getOrCreate(ListBoxOptionRole);
        object->ref();
    }

    return object;
}

// Used to provide const char* returns.
static const char* returnString(const String& str)
{
    static CString returnedString;
    returnedString = str.utf8();
    return returnedString.data();
}

static AccessibilityObject* core(WebKitAccessible* accessible)
{
    if (!accessible)
        return 0;

    return accessible->m_object;
}

static AccessibilityObject* core(AtkObject* object)
{
    if (!WEBKIT_IS_ACCESSIBLE(object))
        return 0;

    return core(WEBKIT_ACCESSIBLE(object));
}

static AccessibilityObject* core(AtkAction* action)
{
    return core(ATK_OBJECT(action));
}

static AccessibilityObject* core(AtkSelection* selection)
{
    return core(ATK_OBJECT(selection));
}

static AccessibilityObject* core(AtkText* text)
{
    return core(ATK_OBJECT(text));
}

static AccessibilityObject* core(AtkEditableText* text)
{
    return core(ATK_OBJECT(text));
}

static AccessibilityObject* core(AtkComponent* component)
{
    return core(ATK_OBJECT(component));
}

static AccessibilityObject* core(AtkImage* image)
{
    return core(ATK_OBJECT(image));
}

static AccessibilityObject* core(AtkTable* table)
{
    return core(ATK_OBJECT(table));
}

static AccessibilityObject* core(AtkDocument* document)
{
    return core(ATK_OBJECT(document));
}

static const gchar* nameFromChildren(AccessibilityObject* object)
{
    if (!object)
        return 0;

    AccessibilityRenderObject::AccessibilityChildrenVector children = object->children();
    // Currently, object->stringValue() should be an empty String. This might not be the case down the road.
    String name = object->stringValue();
    for (unsigned i = 0; i < children.size(); ++i)
        name += children.at(i).get()->stringValue();
    return returnString(name);
}

static const gchar* webkit_accessible_get_name(AtkObject* object)
{
    AccessibilityObject* coreObject = core(object);
    if (!coreObject->isAccessibilityRenderObject())
        return returnString(coreObject->stringValue());

    AccessibilityRenderObject* renderObject = static_cast<AccessibilityRenderObject*>(coreObject);
    if (coreObject->isControl()) {
        AccessibilityObject* label = renderObject->correspondingLabelForControlElement();
        if (label)
            return returnString(nameFromChildren(label));
    }

    if (renderObject->isImage() || renderObject->isInputImage()) {
        Node* node = renderObject->renderer()->node();
        if (node && node->isHTMLElement()) {
            // Get the attribute rather than altText String so as not to fall back on title.
            String alt = static_cast<HTMLElement*>(node)->getAttribute(HTMLNames::altAttr);
            if (!alt.isEmpty())
                return returnString(alt);
        }
    }

    return returnString(coreObject->stringValue());
}

static const gchar* webkit_accessible_get_description(AtkObject* object)
{
    AccessibilityObject* coreObject = core(object);
    Node* node = 0;
    if (coreObject->isAccessibilityRenderObject())
        node = static_cast<AccessibilityRenderObject*>(coreObject)->renderer()->node();
    if (!node || !node->isHTMLElement() || coreObject->ariaRoleAttribute() != UnknownRole)
        return returnString(coreObject->accessibilityDescription());

    // atk_table_get_summary returns an AtkObject. We have no summary object, so expose summary here.
    if (coreObject->roleValue() == TableRole) {
        String summary = static_cast<HTMLTableElement*>(node)->summary();
        if (!summary.isEmpty())
            return returnString(summary);
    }

    // The title attribute should be reliably available as the object's descripton.
    // We do not want to fall back on other attributes in its absence. See bug 25524.
    String title = static_cast<HTMLElement*>(node)->title();
    if (!title.isEmpty())
        return returnString(title);

    return returnString(coreObject->accessibilityDescription());
}

static void setAtkRelationSetFromCoreObject(AccessibilityObject* coreObject, AtkRelationSet* relationSet)
{
    AccessibilityRenderObject* accObject = static_cast<AccessibilityRenderObject*>(coreObject);
    if (accObject->isControl()) {
        AccessibilityObject* label = accObject->correspondingLabelForControlElement();
        if (label)
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABELLED_BY, label->wrapper());
    } else {
        AccessibilityObject* control = accObject->correspondingControlForLabelElement();
        if (control)
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABEL_FOR, control->wrapper());
    }
}

static gpointer webkit_accessible_parent_class = 0;

static AtkObject* atkParentOfWebView(AtkObject* object)
{
    AccessibilityObject* coreParent = core(object)->parentObjectUnignored();

    // The top level web view claims to not have a parent. This makes it
    // impossible for assistive technologies to ascend the accessible
    // hierarchy all the way to the application. (Bug 30489)
    if (!coreParent && core(object)->isWebArea()) {
        HostWindow* hostWindow = core(object)->document()->view()->hostWindow();
        if (hostWindow) {
            PlatformPageClient webView = hostWindow->platformPageClient();
            if (webView) {
                GtkWidget* webViewParent = gtk_widget_get_parent(webView);
                if (webViewParent)
                    return gtk_widget_get_accessible(webViewParent);
            }
        }
    }

    if (!coreParent)
        return 0;

    return coreParent->wrapper();
}

static AtkObject* webkit_accessible_get_parent(AtkObject* object)
{
    AccessibilityObject* coreParent = core(object)->parentObjectUnignored();
    if (!coreParent && core(object)->isWebArea())
        return atkParentOfWebView(object);

    if (!coreParent)
        return 0;

    return coreParent->wrapper();
}

static gint webkit_accessible_get_n_children(AtkObject* object)
{
    return core(object)->children().size();
}

static AtkObject* webkit_accessible_ref_child(AtkObject* object, gint index)
{
    AccessibilityObject* coreObject = core(object);
    AccessibilityObject::AccessibilityChildrenVector children = coreObject->children();
    if (index < 0 || static_cast<unsigned>(index) >= children.size())
        return 0;

    AccessibilityObject* coreChild = children.at(index).get();

    if (!coreChild)
        return 0;

    AtkObject* child = coreChild->wrapper();
    atk_object_set_parent(child, object);
    g_object_ref(child);

    return child;
}

static gint webkit_accessible_get_index_in_parent(AtkObject* object)
{
    AccessibilityObject* coreObject = core(object);
    AccessibilityObject* parent = coreObject->parentObjectUnignored();

    if (!parent && core(object)->isWebArea()) {
        AtkObject* atkParent = atkParentOfWebView(object);
        if (!atkParent)
            return -1;

        unsigned count = atk_object_get_n_accessible_children(atkParent);
        for (unsigned i = 0; i < count; ++i) {
            AtkObject* child = atk_object_ref_accessible_child(atkParent, i);
            bool childIsObject = child == object;
            g_object_unref(child);
            if (childIsObject)
                return i;
        }
    }

    AccessibilityObject::AccessibilityChildrenVector children = parent->children();
    unsigned count = children.size();
    for (unsigned i = 0; i < count; ++i) {
        if (children[i] == coreObject)
            return i;
    }

    return -1;
}

static AtkAttributeSet* addAttributeToSet(AtkAttributeSet* attributeSet, const char* name, const char* value)
{
    AtkAttribute* attribute = static_cast<AtkAttribute*>(g_malloc(sizeof(AtkAttribute)));
    attribute->name = g_strdup(name);
    attribute->value = g_strdup(value);
    attributeSet = g_slist_prepend(attributeSet, attribute);

    return attributeSet;
}

static AtkAttributeSet* webkit_accessible_get_attributes(AtkObject* object)
{
    AtkAttributeSet* attributeSet = 0;

    int headingLevel = core(object)->headingLevel();
    if (headingLevel) {
        String value = String::number(headingLevel);
        attributeSet = addAttributeToSet(attributeSet, "level", value.utf8().data());
    }
    return attributeSet;
}

static AtkRole atkRole(AccessibilityRole role)
{
    switch (role) {
    case UnknownRole:
        return ATK_ROLE_UNKNOWN;
    case ButtonRole:
        return ATK_ROLE_PUSH_BUTTON;
    case RadioButtonRole:
        return ATK_ROLE_RADIO_BUTTON;
    case CheckBoxRole:
        return ATK_ROLE_CHECK_BOX;
    case SliderRole:
        return ATK_ROLE_SLIDER;
    case TabGroupRole:
        return ATK_ROLE_PAGE_TAB_LIST;
    case TextFieldRole:
    case TextAreaRole:
        return ATK_ROLE_ENTRY;
    case StaticTextRole:
        return ATK_ROLE_TEXT;
    case OutlineRole:
        return ATK_ROLE_TREE;
    case MenuBarRole:
        return ATK_ROLE_MENU_BAR;
    case MenuRole:
        return ATK_ROLE_MENU;
    case MenuItemRole:
        return ATK_ROLE_MENU_ITEM;
    case ColumnRole:
        //return ATK_ROLE_TABLE_COLUMN_HEADER; // Is this right?
        return ATK_ROLE_UNKNOWN; // Matches Mozilla
    case RowRole:
        //return ATK_ROLE_TABLE_ROW_HEADER; // Is this right?
        return ATK_ROLE_LIST_ITEM; // Matches Mozilla
    case ToolbarRole:
        return ATK_ROLE_TOOL_BAR;
    case BusyIndicatorRole:
        return ATK_ROLE_PROGRESS_BAR; // Is this right?
    case ProgressIndicatorRole:
        //return ATK_ROLE_SPIN_BUTTON; // Some confusion about this role in AccessibilityRenderObject.cpp
        return ATK_ROLE_PROGRESS_BAR;
    case WindowRole:
        return ATK_ROLE_WINDOW;
    case ComboBoxRole:
        return ATK_ROLE_COMBO_BOX;
    case SplitGroupRole:
        return ATK_ROLE_SPLIT_PANE;
    case SplitterRole:
        return ATK_ROLE_SEPARATOR;
    case ColorWellRole:
        return ATK_ROLE_COLOR_CHOOSER;
    case ListRole:
        return ATK_ROLE_LIST;
    case ScrollBarRole:
        return ATK_ROLE_SCROLL_BAR;
    case GridRole: // Is this right?
    case TableRole:
        return ATK_ROLE_TABLE;
    case ApplicationRole:
        return ATK_ROLE_APPLICATION;
    case GroupRole:
    case RadioGroupRole:
        return ATK_ROLE_PANEL;
    case CellRole:
        return ATK_ROLE_TABLE_CELL;
    case LinkRole:
    case WebCoreLinkRole:
    case ImageMapLinkRole:
        return ATK_ROLE_LINK;
    case ImageMapRole:
    case ImageRole:
        return ATK_ROLE_IMAGE;
    case ListMarkerRole:
        return ATK_ROLE_TEXT;
    case WebAreaRole:
        //return ATK_ROLE_HTML_CONTAINER; // Is this right?
        return ATK_ROLE_DOCUMENT_FRAME;
    case HeadingRole:
        return ATK_ROLE_HEADING;
    case ListBoxRole:
        return ATK_ROLE_LIST;
    case ListBoxOptionRole:
        return ATK_ROLE_LIST_ITEM;
    default:
        return ATK_ROLE_UNKNOWN;
    }
}

static AtkRole webkit_accessible_get_role(AtkObject* object)
{
    AccessibilityObject* axObject = core(object);

    if (!axObject)
        return ATK_ROLE_UNKNOWN;

    // WebCore does not seem to have a role for list items
    if (axObject->isGroup()) {
        AccessibilityObject* parent = axObject->parentObjectUnignored();
        if (parent && parent->isList())
            return ATK_ROLE_LIST_ITEM;
    }

    // WebCore does not know about paragraph role, label role, or section role
    if (axObject->isAccessibilityRenderObject()) {
        Node* node = static_cast<AccessibilityRenderObject*>(axObject)->renderer()->node();
        if (node) {
            if (node->hasTagName(HTMLNames::pTag))
                return ATK_ROLE_PARAGRAPH;
            if (node->hasTagName(HTMLNames::labelTag))
                return ATK_ROLE_LABEL;
            if (node->hasTagName(HTMLNames::divTag))
                return ATK_ROLE_SECTION;
        }
    }

    // Note: Why doesn't WebCore have a password field for this
    if (axObject->isPasswordField())
        return ATK_ROLE_PASSWORD_TEXT;

    return atkRole(axObject->roleValue());
}

static void setAtkStateSetFromCoreObject(AccessibilityObject* coreObject, AtkStateSet* stateSet)
{
    AccessibilityObject* parent = coreObject->parentObject();
    bool isListBoxOption = parent && parent->isListBox();

    // Please keep the state list in alphabetical order
    if (coreObject->isChecked())
        atk_state_set_add_state(stateSet, ATK_STATE_CHECKED);

    // FIXME: isReadOnly does not seem to do the right thing for
    // controls, so check explicitly for them. In addition, because
    // isReadOnly is false for listBoxOptions, we need to add one
    // more check so that we do not present them as being "editable".
    if ((!coreObject->isReadOnly() ||
        (coreObject->isControl() && coreObject->canSetValueAttribute())) &&
        !isListBoxOption)
        atk_state_set_add_state(stateSet, ATK_STATE_EDITABLE);

    // FIXME: Put both ENABLED and SENSITIVE together here for now
    if (coreObject->isEnabled()) {
        atk_state_set_add_state(stateSet, ATK_STATE_ENABLED);
        atk_state_set_add_state(stateSet, ATK_STATE_SENSITIVE);
    }

    if (coreObject->canSetFocusAttribute())
        atk_state_set_add_state(stateSet, ATK_STATE_FOCUSABLE);

    if (coreObject->isFocused())
        atk_state_set_add_state(stateSet, ATK_STATE_FOCUSED);

    // TODO: ATK_STATE_HORIZONTAL

    if (coreObject->isIndeterminate())
        atk_state_set_add_state(stateSet, ATK_STATE_INDETERMINATE);

    if (coreObject->isMultiSelect())
        atk_state_set_add_state(stateSet, ATK_STATE_MULTISELECTABLE);

    // TODO: ATK_STATE_OPAQUE

    if (coreObject->isPressed())
        atk_state_set_add_state(stateSet, ATK_STATE_PRESSED);

    // TODO: ATK_STATE_SELECTABLE_TEXT

    if (coreObject->canSetSelectedAttribute()) {
        atk_state_set_add_state(stateSet, ATK_STATE_SELECTABLE);
        // Items in focusable lists in Gtk have both STATE_SELECT{ABLE,ED}
        // and STATE_FOCUS{ABLE,ED}. We'll fake the latter based on the
        // former.
        if (isListBoxOption)
            atk_state_set_add_state(stateSet, ATK_STATE_FOCUSABLE);
    }

    if (coreObject->isSelected()) {
        atk_state_set_add_state(stateSet, ATK_STATE_SELECTED);
        // Items in focusable lists in Gtk have both STATE_SELECT{ABLE,ED}
        // and STATE_FOCUS{ABLE,ED}. We'll fake the latter based on the
        // former.
        if (isListBoxOption)
            atk_state_set_add_state(stateSet, ATK_STATE_FOCUSED);
    }

    // FIXME: Group both SHOWING and VISIBLE here for now
    // Not sure how to handle this in WebKit, see bug
    // http://bugzilla.gnome.org/show_bug.cgi?id=509650 for other
    // issues with SHOWING vs VISIBLE within GTK+
    if (!coreObject->isOffScreen()) {
        atk_state_set_add_state(stateSet, ATK_STATE_SHOWING);
        atk_state_set_add_state(stateSet, ATK_STATE_VISIBLE);
    }

    // Mutually exclusive, so we group these two
    if (coreObject->roleValue() == TextFieldRole)
        atk_state_set_add_state(stateSet, ATK_STATE_SINGLE_LINE);
    else if (coreObject->roleValue() == TextAreaRole)
        atk_state_set_add_state(stateSet, ATK_STATE_MULTI_LINE);

    // TODO: ATK_STATE_SENSITIVE

    // TODO: ATK_STATE_VERTICAL

    if (coreObject->isVisited())
        atk_state_set_add_state(stateSet, ATK_STATE_VISITED);
}

static AtkStateSet* webkit_accessible_ref_state_set(AtkObject* object)
{
    AtkStateSet* stateSet = ATK_OBJECT_CLASS(webkit_accessible_parent_class)->ref_state_set(object);
    AccessibilityObject* coreObject = core(object);

    if (coreObject == fallbackObject()) {
        atk_state_set_add_state(stateSet, ATK_STATE_DEFUNCT);
        return stateSet;
    }

    setAtkStateSetFromCoreObject(coreObject, stateSet);

    return stateSet;
}

static AtkRelationSet* webkit_accessible_ref_relation_set(AtkObject* object)
{
    AtkRelationSet* relationSet = ATK_OBJECT_CLASS(webkit_accessible_parent_class)->ref_relation_set(object);
    AccessibilityObject* coreObject = core(object);

    setAtkRelationSetFromCoreObject(coreObject, relationSet);

    return relationSet;
}

static void webkit_accessible_init(AtkObject* object, gpointer data)
{
    if (ATK_OBJECT_CLASS(webkit_accessible_parent_class)->initialize)
        ATK_OBJECT_CLASS(webkit_accessible_parent_class)->initialize(object, data);

    WEBKIT_ACCESSIBLE(object)->m_object = reinterpret_cast<AccessibilityObject*>(data);
}

static void webkit_accessible_finalize(GObject* object)
{
    // This is a good time to clear the return buffer.
    returnString(String());

    G_OBJECT_CLASS(webkit_accessible_parent_class)->finalize(object);
}

static void webkit_accessible_class_init(AtkObjectClass* klass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(klass);

    webkit_accessible_parent_class = g_type_class_peek_parent(klass);

    gobjectClass->finalize = webkit_accessible_finalize;

    klass->initialize = webkit_accessible_init;
    klass->get_name = webkit_accessible_get_name;
    klass->get_description = webkit_accessible_get_description;
    klass->get_parent = webkit_accessible_get_parent;
    klass->get_n_children = webkit_accessible_get_n_children;
    klass->ref_child = webkit_accessible_ref_child;
    klass->get_role = webkit_accessible_get_role;
    klass->ref_state_set = webkit_accessible_ref_state_set;
    klass->get_index_in_parent = webkit_accessible_get_index_in_parent;
    klass->get_attributes = webkit_accessible_get_attributes;
    klass->ref_relation_set = webkit_accessible_ref_relation_set;
}

GType
webkit_accessible_get_type(void)
{
    static volatile gsize type_volatile = 0;

    if (g_once_init_enter(&type_volatile)) {
        static const GTypeInfo tinfo = {
            sizeof(WebKitAccessibleClass),
            (GBaseInitFunc) 0,
            (GBaseFinalizeFunc) 0,
            (GClassInitFunc) webkit_accessible_class_init,
            (GClassFinalizeFunc) 0,
            0, /* class data */
            sizeof(WebKitAccessible), /* instance size */
            0, /* nb preallocs */
            (GInstanceInitFunc) 0,
            0 /* value table */
        };

        GType type = g_type_register_static(ATK_TYPE_OBJECT,
                                            "WebKitAccessible", &tinfo, GTypeFlags(0));
        g_once_init_leave(&type_volatile, type);
    }

    return type_volatile;
}

static gboolean webkit_accessible_action_do_action(AtkAction* action, gint i)
{
    g_return_val_if_fail(i == 0, FALSE);
    return core(action)->performDefaultAction();
}

static gint webkit_accessible_action_get_n_actions(AtkAction* action)
{
    return 1;
}

static const gchar* webkit_accessible_action_get_description(AtkAction* action, gint i)
{
    g_return_val_if_fail(i == 0, 0);
    // TODO: Need a way to provide/localize action descriptions.
    notImplemented();
    return "";
}

static const gchar* webkit_accessible_action_get_keybinding(AtkAction* action, gint i)
{
    g_return_val_if_fail(i == 0, 0);
    // FIXME: Construct a proper keybinding string.
    return returnString(core(action)->accessKey().string());
}

static const gchar* webkit_accessible_action_get_name(AtkAction* action, gint i)
{
    g_return_val_if_fail(i == 0, 0);
    return returnString(core(action)->actionVerb());
}

static void atk_action_interface_init(AtkActionIface* iface)
{
    iface->do_action = webkit_accessible_action_do_action;
    iface->get_n_actions = webkit_accessible_action_get_n_actions;
    iface->get_description = webkit_accessible_action_get_description;
    iface->get_keybinding = webkit_accessible_action_get_keybinding;
    iface->get_name = webkit_accessible_action_get_name;
}

// Selection (for controls)

static AccessibilityObject* optionFromList(AtkSelection* selection, gint i)
{
    AccessibilityObject* coreSelection = core(selection);
    if (!coreSelection || i < 0)
        return 0;

    AccessibilityRenderObject::AccessibilityChildrenVector options = core(selection)->children();
    if (i < static_cast<gint>(options.size()))
        return options.at(i).get();

    return 0;
}

static AccessibilityObject* optionFromSelection(AtkSelection* selection, gint i)
{
    // i is the ith selection as opposed to the ith child.

    AccessibilityObject* coreSelection = core(selection);
    if (!coreSelection || i < 0)
        return 0;

    AccessibilityRenderObject::AccessibilityChildrenVector selectedItems;
    if (coreSelection->isListBox())
        static_cast<AccessibilityListBox*>(coreSelection)->selectedChildren(selectedItems);

    // TODO: Combo boxes

    if (i < static_cast<gint>(selectedItems.size()))
        return selectedItems.at(i).get();

    return 0;
}

static gboolean webkit_accessible_selection_add_selection(AtkSelection* selection, gint i)
{
    AccessibilityObject* option = optionFromList(selection, i);
    if (option && core(selection)->isListBox()) {
        AccessibilityListBoxOption* listBoxOption = static_cast<AccessibilityListBoxOption*>(option);
        listBoxOption->setSelected(true);
        return listBoxOption->isSelected();
    }

    return false;
}

static gboolean webkit_accessible_selection_clear_selection(AtkSelection* selection)
{
    AccessibilityObject* coreSelection = core(selection);
    if (!coreSelection)
        return false;

    AccessibilityRenderObject::AccessibilityChildrenVector selectedItems;
    if (coreSelection->isListBox()) {
        // Set the list of selected items to an empty list; then verify that it worked.
        AccessibilityListBox* listBox = static_cast<AccessibilityListBox*>(coreSelection);
        listBox->setSelectedChildren(selectedItems);
        listBox->selectedChildren(selectedItems);
        return selectedItems.size() == 0;
    }
    return false;
}

static AtkObject* webkit_accessible_selection_ref_selection(AtkSelection* selection, gint i)
{
    AccessibilityObject* option = optionFromSelection(selection, i);
    if (option) {
        AtkObject* child = option->wrapper();
        g_object_ref(child);
        return child;
    }

    return 0;
}

static gint webkit_accessible_selection_get_selection_count(AtkSelection* selection)
{
    AccessibilityObject* coreSelection = core(selection);
    if (coreSelection && coreSelection->isListBox()) {
        AccessibilityRenderObject::AccessibilityChildrenVector selectedItems;
        static_cast<AccessibilityListBox*>(coreSelection)->selectedChildren(selectedItems);
        return static_cast<gint>(selectedItems.size());
    }

    return 0;
}

static gboolean webkit_accessible_selection_is_child_selected(AtkSelection* selection, gint i)
{
    AccessibilityObject* option = optionFromList(selection, i);
    if (option && core(selection)->isListBox())
        return static_cast<AccessibilityListBoxOption*>(option)->isSelected();

    return false;
}

static gboolean webkit_accessible_selection_remove_selection(AtkSelection* selection, gint i)
{
    // TODO: This is only getting called if i == 0. What is preventing the rest?
    AccessibilityObject* option = optionFromSelection(selection, i);
    if (option && core(selection)->isListBox()) {
        AccessibilityListBoxOption* listBoxOption = static_cast<AccessibilityListBoxOption*>(option);
        listBoxOption->setSelected(false);
        return !listBoxOption->isSelected();
    }

    return false;
}

static gboolean webkit_accessible_selection_select_all_selection(AtkSelection* selection)
{
    AccessibilityObject* coreSelection = core(selection);
    if (!coreSelection || !coreSelection->isMultiSelect())
        return false;

    AccessibilityRenderObject::AccessibilityChildrenVector children = coreSelection->children();
    if (coreSelection->isListBox()) {
        AccessibilityListBox* listBox = static_cast<AccessibilityListBox*>(coreSelection);
        listBox->setSelectedChildren(children);
        AccessibilityRenderObject::AccessibilityChildrenVector selectedItems;
        listBox->selectedChildren(selectedItems);
        return selectedItems.size() == children.size();
    }

    return false;
}

static void atk_selection_interface_init(AtkSelectionIface* iface)
{
    iface->add_selection = webkit_accessible_selection_add_selection;
    iface->clear_selection = webkit_accessible_selection_clear_selection;
    iface->ref_selection = webkit_accessible_selection_ref_selection;
    iface->get_selection_count = webkit_accessible_selection_get_selection_count;
    iface->is_child_selected = webkit_accessible_selection_is_child_selected;
    iface->remove_selection = webkit_accessible_selection_remove_selection;
    iface->select_all_selection = webkit_accessible_selection_select_all_selection;
}

// Text

static gchar* webkit_accessible_text_get_text(AtkText* text, gint startOffset, gint endOffset)
{
    AccessibilityObject* coreObject = core(text);
    String ret;
    unsigned start = startOffset;
    if (endOffset == -1)
        endOffset = coreObject->stringValue().length();
    int length = endOffset - startOffset;

    if (coreObject->isTextControl())
        ret = coreObject->doAXStringForRange(PlainTextRange(start, length));
    else
        ret = coreObject->textUnderElement().substring(start, length);

    return g_strdup(ret.utf8().data());
}

static GailTextUtil* getGailTextUtilForAtk(AtkText* textObject)
{
    gpointer data = g_object_get_data(G_OBJECT(textObject), "webkit-accessible-gail-text-util");
    if (data)
        return static_cast<GailTextUtil*>(data);

    GailTextUtil* gailTextUtil = gail_text_util_new();
    gail_text_util_text_setup(gailTextUtil, webkit_accessible_text_get_text(textObject, 0, -1));
    g_object_set_data_full(G_OBJECT(textObject), "webkit-accessible-gail-text-util", gailTextUtil, g_object_unref);
    return gailTextUtil;
}

static gchar* utf8Substr(const gchar* string, gint start, gint end)
{
    ASSERT(string);
    glong strLen = g_utf8_strlen(string, -1);
    if (start > strLen || end > strLen)
        return 0;
    gchar* startPtr = g_utf8_offset_to_pointer(string, start);
    gsize lenInBytes = g_utf8_offset_to_pointer(string, end) -  startPtr + 1;
    gchar* output = static_cast<gchar*>(g_malloc0(lenInBytes + 1));
    return g_utf8_strncpy(output, startPtr, end - start + 1);
}

// This function is not completely general, is it's tied to the
// internals of WebCore's text presentation.
static gchar* convertUniCharToUTF8(const UChar* characters, gint length, int from, int to)
{
    CString stringUTF8 = UTF8Encoding().encode(characters, length, QuestionMarksForUnencodables);
    gchar* utf8String = utf8Substr(stringUTF8.data(), from, to);
    if (!g_utf8_validate(utf8String, -1, 0)) {
        g_free(utf8String);
        return 0;
    }
    gsize len = strlen(utf8String);
    GString* ret = g_string_new_len(0, len);
    gchar* ptr = utf8String;

    // WebCore introduces line breaks in the text that do not reflect
    // the layout you see on the screen, replace them with spaces
    while (len > 0) {
        gint index, start;
        pango_find_paragraph_boundary(ptr, len, &index, &start);
        g_string_append_len(ret, ptr, index);
        if (index == start)
            break;
        g_string_append_c(ret, ' ');
        ptr += start;
        len -= start;
    }

    g_free(utf8String);
    return g_string_free(ret, FALSE);
}

static PangoLayout* getPangoLayoutForAtk(AtkText* textObject)
{
    AccessibilityObject* coreObject = core(textObject);

    HostWindow* hostWindow = coreObject->document()->view()->hostWindow();
    if (!hostWindow)
        return 0;
    PlatformPageClient webView = hostWindow->platformPageClient();
    if (!webView)
        return 0;

    GString* str = g_string_new(0);

    AccessibilityRenderObject* accObject = static_cast<AccessibilityRenderObject*>(coreObject);
    if (!accObject)
        return 0;
    RenderText* renderText = toRenderText(accObject->renderer());
    if (!renderText)
        return 0;

    // Create a string with the layout as it appears on the screen
    if (accObject->isTextControl()) {
        unsigned textLength = accObject->textLength();
        int lineNumber = 0;
        PlainTextRange range = accObject->doAXRangeForLine(lineNumber);
        while (range.length) {
            // When a line of text wraps in a text area, the final space is removed.
            if (range.start + range.length < textLength)
                range.length -= 1;
            String lineText = accObject->doAXStringForRange(range);
            g_string_append(str, lineText.utf8().data());
            g_string_append(str, "\n");
            range = accObject->doAXRangeForLine(++lineNumber);
        }
    } else {
        InlineTextBox* box = renderText->firstTextBox();
        while (box) {
            gchar* text = convertUniCharToUTF8(renderText->characters(), renderText->textLength(), box->start(), box->end());
            g_string_append(str, text);
            // Newline chars in the source result in separate text boxes, so check
            // before adding a newline in the layout. See bug 25415 comment #78.
            if (!box->nextOnLineExists())
                g_string_append(str, "\n");
            box = box->nextTextBox();
        }
    }

    PangoLayout* layout = gtk_widget_create_pango_layout(static_cast<GtkWidget*>(webView), g_string_free(str, FALSE));
    g_object_set_data_full(G_OBJECT(textObject), "webkit-accessible-pango-layout", layout, g_object_unref);
    return layout;
}

static gchar* webkit_accessible_text_get_text_after_offset(AtkText* text, gint offset, AtkTextBoundary boundaryType, gint* startOffset, gint* endOffset)
{
    return gail_text_util_get_text(getGailTextUtilForAtk(text), getPangoLayoutForAtk(text), GAIL_AFTER_OFFSET, boundaryType, offset, startOffset, endOffset);
}

static gchar* webkit_accessible_text_get_text_at_offset(AtkText* text, gint offset, AtkTextBoundary boundaryType, gint* startOffset, gint* endOffset)
{
    return gail_text_util_get_text(getGailTextUtilForAtk(text), getPangoLayoutForAtk(text), GAIL_AT_OFFSET, boundaryType, offset, startOffset, endOffset);
}

static gchar* webkit_accessible_text_get_text_before_offset(AtkText* text, gint offset, AtkTextBoundary boundaryType, gint* startOffset, gint* endOffset)
{
    return gail_text_util_get_text(getGailTextUtilForAtk(text), getPangoLayoutForAtk(text), GAIL_BEFORE_OFFSET, boundaryType, offset, startOffset, endOffset);
}

static gunichar webkit_accessible_text_get_character_at_offset(AtkText* text, gint offset)
{
    notImplemented();
    return 0;
}

static gint webkit_accessible_text_get_caret_offset(AtkText* text)
{
    // TODO: Verify this for RTL text.
    return core(text)->selection().end().offsetInContainerNode();
}

static AtkAttributeSet* webkit_accessible_text_get_run_attributes(AtkText* text, gint offset, gint* start_offset, gint* end_offset)
{
    notImplemented();
    return 0;
}

static AtkAttributeSet* webkit_accessible_text_get_default_attributes(AtkText* text)
{
    notImplemented();
    return 0;
}

static void webkit_accessible_text_get_character_extents(AtkText* text, gint offset, gint* x, gint* y, gint* width, gint* height, AtkCoordType coords)
{
    IntRect extents = core(text)->doAXBoundsForRange(PlainTextRange(offset, 1));
    // FIXME: Use the AtkCoordType
    // Requires WebCore::ScrollView::contentsToScreen() to be implemented

#if 0
    switch(coords) {
    case ATK_XY_SCREEN:
        extents = core(text)->document()->view()->contentsToScreen(extents);
        break;
    case ATK_XY_WINDOW:
        // No-op
        break;
    }
#endif

    *x = extents.x();
    *y = extents.y();
    *width = extents.width();
    *height = extents.height();
}

static gint webkit_accessible_text_get_character_count(AtkText* text)
{
    AccessibilityObject* coreObject = core(text);

    if (coreObject->isTextControl())
        return coreObject->textLength();
    else
        return coreObject->textUnderElement().length();
}

static gint webkit_accessible_text_get_offset_at_point(AtkText* text, gint x, gint y, AtkCoordType coords)
{
    // FIXME: Use the AtkCoordType
    // TODO: Is it correct to ignore range.length?
    IntPoint pos(x, y);
    PlainTextRange range = core(text)->doAXRangeForPosition(pos);
    return range.start;
}

static bool selectionBelongsToObject(AccessibilityObject* coreObject, VisibleSelection& selection)
{
    if (!coreObject->isAccessibilityRenderObject())
        return false;

    Node* node = static_cast<AccessibilityRenderObject*>(coreObject)->renderer()->node();
    return node == selection.base().containerNode();
}

static gint webkit_accessible_text_get_n_selections(AtkText* text)
{
    AccessibilityObject* coreObject = core(text);
    VisibleSelection selection = coreObject->selection();

    // We don't support multiple selections for now, so there's only
    // two possibilities
    // Also, we don't want to do anything if the selection does not
    // belong to the currently selected object. We have to check since
    // there's no way to get the selection for a given object, only
    // the global one (the API is a bit confusing)
    return !selectionBelongsToObject(coreObject, selection) || selection.isNone() ? 0 : 1;
}

static gchar* webkit_accessible_text_get_selection(AtkText* text, gint selection_num, gint* start_offset, gint* end_offset)
{
    AccessibilityObject* coreObject = core(text);
    VisibleSelection selection = coreObject->selection();

    // WebCore does not support multiple selection, so anything but 0 does not make sense for now.
    // Also, we don't want to do anything if the selection does not
    // belong to the currently selected object. We have to check since
    // there's no way to get the selection for a given object, only
    // the global one (the API is a bit confusing)
    if (selection_num != 0 || !selectionBelongsToObject(coreObject, selection)) {
        *start_offset = *end_offset = 0;
        return 0;
    }

    *start_offset = selection.start().offsetInContainerNode();
    *end_offset = selection.end().offsetInContainerNode();

    return webkit_accessible_text_get_text(text, *start_offset, *end_offset);
}

static gboolean webkit_accessible_text_add_selection(AtkText* text, gint start_offset, gint end_offset)
{
    notImplemented();
    return FALSE;
}

static gboolean webkit_accessible_text_remove_selection(AtkText* text, gint selection_num)
{
    notImplemented();
    return FALSE;
}

static gboolean webkit_accessible_text_set_selection(AtkText* text, gint selection_num, gint start_offset, gint end_offset)
{
    notImplemented();
    return FALSE;
}

static gboolean webkit_accessible_text_set_caret_offset(AtkText* text, gint offset)
{
    AccessibilityObject* coreObject = core(text);

    // FIXME: We need to reimplement visiblePositionRangeForRange here
    // because the actual function checks the offset is within the
    // boundaries of text().length(), but text() only works for text
    // controls...
    VisiblePosition startPosition = coreObject->visiblePositionForIndex(offset);
    startPosition.setAffinity(DOWNSTREAM);
    VisiblePosition endPosition = coreObject->visiblePositionForIndex(offset);
    VisiblePositionRange range = VisiblePositionRange(startPosition, endPosition);

    coreObject->setSelectedVisiblePositionRange(range);
    return TRUE;
}

static void atk_text_interface_init(AtkTextIface* iface)
{
    iface->get_text = webkit_accessible_text_get_text;
    iface->get_text_after_offset = webkit_accessible_text_get_text_after_offset;
    iface->get_text_at_offset = webkit_accessible_text_get_text_at_offset;
    iface->get_character_at_offset = webkit_accessible_text_get_character_at_offset;
    iface->get_text_before_offset = webkit_accessible_text_get_text_before_offset;
    iface->get_caret_offset = webkit_accessible_text_get_caret_offset;
    iface->get_run_attributes = webkit_accessible_text_get_run_attributes;
    iface->get_default_attributes = webkit_accessible_text_get_default_attributes;
    iface->get_character_extents = webkit_accessible_text_get_character_extents;
    iface->get_character_count = webkit_accessible_text_get_character_count;
    iface->get_offset_at_point = webkit_accessible_text_get_offset_at_point;
    iface->get_n_selections = webkit_accessible_text_get_n_selections;
    iface->get_selection = webkit_accessible_text_get_selection;

    // set methods
    iface->add_selection = webkit_accessible_text_add_selection;
    iface->remove_selection = webkit_accessible_text_remove_selection;
    iface->set_selection = webkit_accessible_text_set_selection;
    iface->set_caret_offset = webkit_accessible_text_set_caret_offset;
}

// EditableText

static gboolean webkit_accessible_editable_text_set_run_attributes(AtkEditableText* text, AtkAttributeSet* attrib_set, gint start_offset, gint end_offset)
{
    notImplemented();
    return FALSE;
}

static void webkit_accessible_editable_text_set_text_contents(AtkEditableText* text, const gchar* string)
{
    // FIXME: string nullcheck?
    core(text)->setValue(String::fromUTF8(string));
}

static void webkit_accessible_editable_text_insert_text(AtkEditableText* text, const gchar* string, gint length, gint* position)
{
    // FIXME: string nullcheck?

    AccessibilityObject* coreObject = core(text);
    // FIXME: Not implemented in WebCore
    //coreObject->setSelectedTextRange(PlainTextRange(*position, 0));
    //coreObject->setSelectedText(String::fromUTF8(string));

    if (!coreObject->document() || !coreObject->document()->frame())
        return;
    coreObject->setSelectedVisiblePositionRange(coreObject->visiblePositionRangeForRange(PlainTextRange(*position, 0)));
    coreObject->setFocused(true);
    // FIXME: We should set position to the actual inserted text length, which may be less than that requested.
    if (coreObject->document()->frame()->editor()->insertTextWithoutSendingTextEvent(String::fromUTF8(string), false, 0))
        *position += length;
}

static void webkit_accessible_editable_text_copy_text(AtkEditableText* text, gint start_pos, gint end_pos)
{
    notImplemented();
}

static void webkit_accessible_editable_text_cut_text(AtkEditableText* text, gint start_pos, gint end_pos)
{
    notImplemented();
}

static void webkit_accessible_editable_text_delete_text(AtkEditableText* text, gint start_pos, gint end_pos)
{
    AccessibilityObject* coreObject = core(text);
    // FIXME: Not implemented in WebCore
    //coreObject->setSelectedTextRange(PlainTextRange(start_pos, end_pos - start_pos));
    //coreObject->setSelectedText(String());

    if (!coreObject->document() || !coreObject->document()->frame())
        return;
    coreObject->setSelectedVisiblePositionRange(coreObject->visiblePositionRangeForRange(PlainTextRange(start_pos, end_pos - start_pos)));
    coreObject->setFocused(true);
    coreObject->document()->frame()->editor()->performDelete();
}

static void webkit_accessible_editable_text_paste_text(AtkEditableText* text, gint position)
{
    notImplemented();
}

static void atk_editable_text_interface_init(AtkEditableTextIface* iface)
{
    iface->set_run_attributes = webkit_accessible_editable_text_set_run_attributes;
    iface->set_text_contents = webkit_accessible_editable_text_set_text_contents;
    iface->insert_text = webkit_accessible_editable_text_insert_text;
    iface->copy_text = webkit_accessible_editable_text_copy_text;
    iface->cut_text = webkit_accessible_editable_text_cut_text;
    iface->delete_text = webkit_accessible_editable_text_delete_text;
    iface->paste_text = webkit_accessible_editable_text_paste_text;
}

static void contentsToAtk(AccessibilityObject* coreObject, AtkCoordType coordType, IntRect rect, gint* x, gint* y, gint* width = 0, gint* height = 0)
{
    FrameView* frameView = coreObject->documentFrameView();

    if (frameView) {
        switch (coordType) {
        case ATK_XY_WINDOW:
            rect = frameView->contentsToWindow(rect);
            break;
        case ATK_XY_SCREEN:
            rect = frameView->contentsToScreen(rect);
            break;
        }
    }

    if (x)
        *x = rect.x();
    if (y)
        *y = rect.y();
    if (width)
        *width = rect.width();
    if (height)
        *height = rect.height();
}

static IntPoint atkToContents(AccessibilityObject* coreObject, AtkCoordType coordType, gint x, gint y)
{
    IntPoint pos(x, y);

    FrameView* frameView = coreObject->documentFrameView();
    if (frameView) {
        switch (coordType) {
        case ATK_XY_SCREEN:
            return frameView->screenToContents(pos);
        case ATK_XY_WINDOW:
            return frameView->windowToContents(pos);
        }
    }

    return pos;
}

static AtkObject* webkit_accessible_component_ref_accessible_at_point(AtkComponent* component, gint x, gint y, AtkCoordType coordType)
{
    IntPoint pos = atkToContents(core(component), coordType, x, y);
    AccessibilityObject* target = core(component)->doAccessibilityHitTest(pos);
    if (!target)
        return 0;
    g_object_ref(target->wrapper());
    return target->wrapper();
}

static void webkit_accessible_component_get_extents(AtkComponent* component, gint* x, gint* y, gint* width, gint* height, AtkCoordType coordType)
{
    IntRect rect = core(component)->elementRect();
    contentsToAtk(core(component), coordType, rect, x, y, width, height);
}

static gboolean webkit_accessible_component_grab_focus(AtkComponent* component)
{
    core(component)->setFocused(true);
    return core(component)->isFocused();
}

static void atk_component_interface_init(AtkComponentIface* iface)
{
    iface->ref_accessible_at_point = webkit_accessible_component_ref_accessible_at_point;
    iface->get_extents = webkit_accessible_component_get_extents;
    iface->grab_focus = webkit_accessible_component_grab_focus;
}

// Image

static void webkit_accessible_image_get_image_position(AtkImage* image, gint* x, gint* y, AtkCoordType coordType)
{
    IntRect rect = core(image)->elementRect();
    contentsToAtk(core(image), coordType, rect, x, y);
}

static const gchar* webkit_accessible_image_get_image_description(AtkImage* image)
{
    return returnString(core(image)->accessibilityDescription());
}

static void webkit_accessible_image_get_image_size(AtkImage* image, gint* width, gint* height)
{
    IntSize size = core(image)->size();

    if (width)
        *width = size.width();
    if (height)
        *height = size.height();
}

static void atk_image_interface_init(AtkImageIface* iface)
{
    iface->get_image_position = webkit_accessible_image_get_image_position;
    iface->get_image_description = webkit_accessible_image_get_image_description;
    iface->get_image_size = webkit_accessible_image_get_image_size;
}

// Table

static AccessibilityTableCell* cell(AtkTable* table, guint row, guint column)
{
    AccessibilityObject* accTable = core(table);
    if (accTable->isAccessibilityRenderObject())
        return static_cast<AccessibilityTable*>(accTable)->cellForColumnAndRow(column, row);
    return 0;
}

static gint cellIndex(AccessibilityTableCell* axCell, AccessibilityTable* axTable)
{
    // Calculate the cell's index as if we had a traditional Gtk+ table in
    // which cells are all direct children of the table, arranged row-first.
    AccessibilityObject::AccessibilityChildrenVector allCells;
    axTable->cells(allCells);
    AccessibilityObject::AccessibilityChildrenVector::iterator position;
    position = std::find(allCells.begin(), allCells.end(), axCell);
    if (position == allCells.end())
        return -1;
    return position - allCells.begin();
}

static AccessibilityTableCell* cellAtIndex(AtkTable* table, gint index)
{
    AccessibilityObject* accTable = core(table);
    if (accTable->isAccessibilityRenderObject()) {
        AccessibilityObject::AccessibilityChildrenVector allCells;
        static_cast<AccessibilityTable*>(accTable)->cells(allCells);
        if (0 <= index && static_cast<unsigned>(index) < allCells.size()) {
            AccessibilityObject* accCell = allCells.at(index).get();
            return static_cast<AccessibilityTableCell*>(accCell);
        }
    }
    return 0;
}

static AtkObject* webkit_accessible_table_ref_at(AtkTable* table, gint row, gint column)
{
    AccessibilityTableCell* axCell = cell(table, row, column);
    if (!axCell)
        return 0;
    return axCell->wrapper();
}

static gint webkit_accessible_table_get_index_at(AtkTable* table, gint row, gint column)
{
    AccessibilityTableCell* axCell = cell(table, row, column);
    AccessibilityTable* axTable = static_cast<AccessibilityTable*>(core(table));
    return cellIndex(axCell, axTable);
}

static gint webkit_accessible_table_get_column_at_index(AtkTable* table, gint index)
{
    AccessibilityTableCell* axCell = cellAtIndex(table, index);
    if (axCell){
        pair<int, int> columnRange;
        axCell->columnIndexRange(columnRange);
        return columnRange.first;
    }
    return -1;
}

static gint webkit_accessible_table_get_row_at_index(AtkTable* table, gint index)
{
    AccessibilityTableCell* axCell = cellAtIndex(table, index);
    if (axCell){
        pair<int, int> rowRange;
        axCell->rowIndexRange(rowRange);
        return rowRange.first;
    }
    return -1;
}

static gint webkit_accessible_table_get_n_columns(AtkTable* table)
{
    AccessibilityObject* accTable = core(table);
    if (accTable->isAccessibilityRenderObject())
        return static_cast<AccessibilityTable*>(accTable)->columnCount();
    return 0;
}

static gint webkit_accessible_table_get_n_rows(AtkTable* table)
{
    AccessibilityObject* accTable = core(table);
    if (accTable->isAccessibilityRenderObject())
        return static_cast<AccessibilityTable*>(accTable)->rowCount();
    return 0;
}

static gint webkit_accessible_table_get_column_extent_at(AtkTable* table, gint row, gint column)
{
    AccessibilityTableCell* axCell = cell(table, row, column);
    if (axCell) {
        pair<int, int> columnRange;
        axCell->columnIndexRange(columnRange);
        return columnRange.second;
    }
    return 0;
}

static gint webkit_accessible_table_get_row_extent_at(AtkTable* table, gint row, gint column)
{
    AccessibilityTableCell* axCell = cell(table, row, column);
    if (axCell) {
        pair<int, int> rowRange;
        axCell->rowIndexRange(rowRange);
        return rowRange.second;
    }
    return 0;
}

static AtkObject* webkit_accessible_table_get_column_header(AtkTable* table, gint column)
{
    // FIXME: This needs to be implemented.
    notImplemented();
    return 0;
}

static AtkObject* webkit_accessible_table_get_row_header(AtkTable* table, gint row)
{
    AccessibilityObject* accTable = core(table);
    if (accTable->isAccessibilityRenderObject()) {
        AccessibilityObject::AccessibilityChildrenVector allRowHeaders;
        static_cast<AccessibilityTable*>(accTable)->rowHeaders(allRowHeaders);

        unsigned rowCount = allRowHeaders.size();
        for (unsigned k = 0; k < rowCount; ++k) {
            AccessibilityObject* rowObject = allRowHeaders[k]->parentObject();
            if (static_cast<AccessibilityTableRow*>(rowObject)->rowIndex() == row)
                return allRowHeaders[k]->wrapper();
        }
    }
    return 0;
}

static AtkObject* webkit_accessible_table_get_caption(AtkTable* table)
{
    AccessibilityObject* accTable = core(table);
    if (accTable->isAccessibilityRenderObject()) {
        Node* node = static_cast<AccessibilityRenderObject*>(accTable)->renderer()->node();
        if (node && node->hasTagName(HTMLNames::tableTag)) {
            HTMLTableCaptionElement* caption = static_cast<HTMLTableElement*>(node)->caption();
            if (caption)
                return AccessibilityObject::firstAccessibleObjectFromNode(caption->renderer()->node())->wrapper();
        }
    }
    return 0;
}

static const gchar* webkit_accessible_table_get_column_description(AtkTable* table, gint column)
{
    AtkObject* columnHeader = atk_table_get_column_header(table, column);
    if (columnHeader)
        return returnString(nameFromChildren(core(columnHeader)));

    return 0;
}

static const gchar* webkit_accessible_table_get_row_description(AtkTable* table, gint row)
{
    AtkObject* rowHeader = atk_table_get_row_header(table, row);
    if (rowHeader)
        return returnString(nameFromChildren(core(rowHeader)));

    return 0;
}

static void atk_table_interface_init(AtkTableIface* iface)
{
    iface->ref_at = webkit_accessible_table_ref_at;
    iface->get_index_at = webkit_accessible_table_get_index_at;
    iface->get_column_at_index = webkit_accessible_table_get_column_at_index;
    iface->get_row_at_index = webkit_accessible_table_get_row_at_index;
    iface->get_n_columns = webkit_accessible_table_get_n_columns;
    iface->get_n_rows = webkit_accessible_table_get_n_rows;
    iface->get_column_extent_at = webkit_accessible_table_get_column_extent_at;
    iface->get_row_extent_at = webkit_accessible_table_get_row_extent_at;
    iface->get_column_header = webkit_accessible_table_get_column_header;
    iface->get_row_header = webkit_accessible_table_get_row_header;
    iface->get_caption = webkit_accessible_table_get_caption;
    iface->get_column_description = webkit_accessible_table_get_column_description;
    iface->get_row_description = webkit_accessible_table_get_row_description;
}

static const gchar* documentAttributeValue(AtkDocument* document, const gchar* attribute)
{
    Document* coreDocument = core(document)->document();
    if (!coreDocument)
        return 0;

    String value = String();
    if (!g_ascii_strcasecmp(attribute, "DocType") && coreDocument->doctype())
        value = coreDocument->doctype()->name();
    else if (!g_ascii_strcasecmp(attribute, "Encoding"))
        value = coreDocument->charset();
    else if (!g_ascii_strcasecmp(attribute, "URI"))
        value = coreDocument->documentURI();
    if (!value.isEmpty())
        return returnString(value);

    return 0;
}

static const gchar* webkit_accessible_document_get_attribute_value(AtkDocument* document, const gchar* attribute)
{
    return documentAttributeValue(document, attribute);
}

static AtkAttributeSet* webkit_accessible_document_get_attributes(AtkDocument* document)
{
    AtkAttributeSet* attributeSet = 0;
    const gchar* attributes [] = {"DocType", "Encoding", "URI"};

    for (unsigned i = 0; i < G_N_ELEMENTS(attributes); i++) {
        const gchar* value = documentAttributeValue(document, attributes[i]);
        if (value)
            attributeSet = addAttributeToSet(attributeSet, attributes[i], value);
    }

    return attributeSet;
}

static const gchar* webkit_accessible_document_get_locale(AtkDocument* document)
{

    // TODO: Should we fall back on lang xml:lang when the following comes up empty?
    String language = static_cast<AccessibilityRenderObject*>(core(document))->language();
    if (!language.isEmpty())
        return returnString(language);

    return 0;
}

static void atk_document_interface_init(AtkDocumentIface* iface)
{
    iface->get_document_attribute_value = webkit_accessible_document_get_attribute_value;
    iface->get_document_attributes = webkit_accessible_document_get_attributes;
    iface->get_document_locale = webkit_accessible_document_get_locale;
}

static const GInterfaceInfo AtkInterfacesInitFunctions[] = {
    {(GInterfaceInitFunc)atk_action_interface_init,
     (GInterfaceFinalizeFunc) 0, 0},
    {(GInterfaceInitFunc)atk_selection_interface_init,
     (GInterfaceFinalizeFunc) 0, 0},
    {(GInterfaceInitFunc)atk_editable_text_interface_init,
     (GInterfaceFinalizeFunc) 0, 0},
    {(GInterfaceInitFunc)atk_text_interface_init,
     (GInterfaceFinalizeFunc) 0, 0},
    {(GInterfaceInitFunc)atk_component_interface_init,
     (GInterfaceFinalizeFunc) 0, 0},
    {(GInterfaceInitFunc)atk_image_interface_init,
     (GInterfaceFinalizeFunc) 0, 0},
    {(GInterfaceInitFunc)atk_table_interface_init,
     (GInterfaceFinalizeFunc) 0, 0},
    {(GInterfaceInitFunc)atk_document_interface_init,
     (GInterfaceFinalizeFunc) 0, 0}
};

enum WAIType {
    WAI_ACTION,
    WAI_SELECTION,
    WAI_EDITABLE_TEXT,
    WAI_TEXT,
    WAI_COMPONENT,
    WAI_IMAGE,
    WAI_TABLE,
    WAI_DOCUMENT
};

static GType GetAtkInterfaceTypeFromWAIType(WAIType type)
{
  switch (type) {
  case WAI_ACTION:
      return ATK_TYPE_ACTION;
  case WAI_SELECTION:
      return ATK_TYPE_SELECTION;
  case WAI_EDITABLE_TEXT:
      return ATK_TYPE_EDITABLE_TEXT;
  case WAI_TEXT:
      return ATK_TYPE_TEXT;
  case WAI_COMPONENT:
      return ATK_TYPE_COMPONENT;
  case WAI_IMAGE:
      return ATK_TYPE_IMAGE;
  case WAI_TABLE:
      return ATK_TYPE_TABLE;
  case WAI_DOCUMENT:
      return ATK_TYPE_DOCUMENT;
  }

  return G_TYPE_INVALID;
}

static guint16 getInterfaceMaskFromObject(AccessibilityObject* coreObject)
{
    guint16 interfaceMask = 0;

    // Component interface is always supported
    interfaceMask |= 1 << WAI_COMPONENT;

    // Action
    if (!coreObject->actionVerb().isEmpty())
        interfaceMask |= 1 << WAI_ACTION;

    // Selection
    if (coreObject->isListBox())
        interfaceMask |= 1 << WAI_SELECTION;

    // Text & Editable Text
    AccessibilityRole role = coreObject->roleValue();

    if (role == StaticTextRole)
        interfaceMask |= 1 << WAI_TEXT;
    else if (coreObject->isAccessibilityRenderObject() && coreObject->isTextControl()) {
        interfaceMask |= 1 << WAI_TEXT;
        if (!coreObject->isReadOnly())
            interfaceMask |= 1 << WAI_EDITABLE_TEXT;
    }

    // Image
    if (coreObject->isImage())
        interfaceMask |= 1 << WAI_IMAGE;

    // Table
    if (role == TableRole)
        interfaceMask |= 1 << WAI_TABLE;

    // Document
    if (role == WebAreaRole)
        interfaceMask |= 1 << WAI_DOCUMENT;

    return interfaceMask;
}

static const char* getUniqueAccessibilityTypeName(guint16 interfaceMask)
{
#define WAI_TYPE_NAME_LEN (30) /* Enough for prefix + 5 hex characters (max) */
    static char name[WAI_TYPE_NAME_LEN + 1];

    g_sprintf(name, "WAIType%x", interfaceMask);
    name[WAI_TYPE_NAME_LEN] = '\0';

    return name;
}

static GType getAccessibilityTypeFromObject(AccessibilityObject* coreObject)
{
    static const GTypeInfo typeInfo = {
        sizeof(WebKitAccessibleClass),
        (GBaseInitFunc) 0,
        (GBaseFinalizeFunc) 0,
        (GClassInitFunc) 0,
        (GClassFinalizeFunc) 0,
        0, /* class data */
        sizeof(WebKitAccessible), /* instance size */
        0, /* nb preallocs */
        (GInstanceInitFunc) 0,
        0 /* value table */
    };

    guint16 interfaceMask = getInterfaceMaskFromObject(coreObject);
    const char* atkTypeName = getUniqueAccessibilityTypeName(interfaceMask);
    GType type = g_type_from_name(atkTypeName);
    if (type)
        return type;

    type = g_type_register_static(WEBKIT_TYPE_ACCESSIBLE,
                                  atkTypeName,
                                  &typeInfo, GTypeFlags(0));
    for (guint i = 0; i < G_N_ELEMENTS(AtkInterfacesInitFunctions); i++) {
        if (interfaceMask & (1 << i))
            g_type_add_interface_static(type,
                                        GetAtkInterfaceTypeFromWAIType(static_cast<WAIType>(i)),
                                        &AtkInterfacesInitFunctions[i]);
    }

    return type;
}

WebKitAccessible* webkit_accessible_new(AccessibilityObject* coreObject)
{
    GType type = getAccessibilityTypeFromObject(coreObject);
    AtkObject* object = static_cast<AtkObject*>(g_object_new(type, 0));

    atk_object_initialize(object, coreObject);

    return WEBKIT_ACCESSIBLE(object);
}

AccessibilityObject* webkit_accessible_get_accessibility_object(WebKitAccessible* accessible)
{
    return accessible->m_object;
}

void webkit_accessible_detach(WebKitAccessible* accessible)
{
    ASSERT(accessible->m_object);

    // We replace the WebCore AccessibilityObject with a fallback object that
    // provides default implementations to avoid repetitive null-checking after
    // detachment.
    accessible->m_object = fallbackObject();
}

AtkObject* webkit_accessible_get_focused_element(WebKitAccessible* accessible)
{
    if (!accessible->m_object)
        return 0;

    RefPtr<AccessibilityObject> focusedObj = accessible->m_object->focusedUIElement();
    if (!focusedObj)
        return 0;

    return focusedObj->wrapper();
}

#endif // HAVE(ACCESSIBILITY)
