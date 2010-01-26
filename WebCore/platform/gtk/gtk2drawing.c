/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>  (Original Author)
 *  Pierre Chanial <p_ch@verizon.net>
 *  Michael Ventnor <m.ventnor@gmail.com>
 *  Alp Toker <alp@nuanti.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * This file contains painting functions for each of the gtk2 widgets.
 * Adapted from the gtkdrawing.c, and gtk+2.0 source.
 */

#include <gdk/gdkprivate.h>
#include "gtkdrawing.h"

#include "Assertions.h"

#include <math.h>
#include <string.h>

#define XTHICKNESS(style) (style->xthickness)
#define YTHICKNESS(style) (style->ythickness)
#define WINDOW_IS_MAPPED(window) ((window) && GDK_IS_WINDOW(window) && gdk_window_is_visible(window))

static GtkThemeParts *gParts = NULL;
static style_prop_t style_prop_func;
static gboolean have_arrow_scaling;
static gboolean is_initialized;

void
moz_gtk_use_theme_parts(GtkThemeParts* parts)
{
    gParts = parts;
}

/* Because we have such an unconventional way of drawing widgets, signal to the GTK theme engine
   that they are drawing for Mozilla instead of a conventional GTK app so they can do any specific
   things they may want to do. */
static void
moz_gtk_set_widget_name(GtkWidget* widget)
{
    gtk_widget_set_name(widget, "MozillaGtkWidget");
}

gint
moz_gtk_enable_style_props(style_prop_t styleGetProp)
{
    style_prop_func = styleGetProp;
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_window_widget()
{
    if (!gParts->protoWindow) {
        gParts->protoWindow = gtk_window_new(GTK_WINDOW_POPUP);

        if (gParts->colormap)
            gtk_widget_set_colormap(gParts->protoWindow, gParts->colormap);

        gtk_widget_realize(gParts->protoWindow);
        moz_gtk_set_widget_name(gParts->protoWindow);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
setup_widget_prototype(GtkWidget* widget)
{
    ensure_window_widget();
    if (!gParts->protoLayout) {
        gParts->protoLayout = gtk_fixed_new();
        gtk_container_add(GTK_CONTAINER(gParts->protoWindow), gParts->protoLayout);
    }

    gtk_container_add(GTK_CONTAINER(gParts->protoLayout), widget);
    gtk_widget_realize(widget);
    g_object_set_data(G_OBJECT(widget), "transparent-bg-hint", GINT_TO_POINTER(TRUE));
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_button_widget()
{
    if (!gParts->buttonWidget) {
        gParts->buttonWidget = gtk_button_new_with_label("M");
        setup_widget_prototype(gParts->buttonWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_hpaned_widget()
{
    if (!gParts->hpanedWidget) {
        gParts->hpanedWidget = gtk_hpaned_new();
        setup_widget_prototype(gParts->hpanedWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_vpaned_widget()
{
    if (!gParts->vpanedWidget) {
        gParts->vpanedWidget = gtk_vpaned_new();
        setup_widget_prototype(gParts->vpanedWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_toggle_button_widget()
{
    if (!gParts->toggleButtonWidget) {
        gParts->toggleButtonWidget = gtk_toggle_button_new();
        setup_widget_prototype(gParts->toggleButtonWidget);
        /* toggle button must be set active to get the right style on hover. */
        GTK_TOGGLE_BUTTON(gParts->toggleButtonWidget)->active = TRUE;
  }
  return MOZ_GTK_SUCCESS;
}

static gint
ensure_button_arrow_widget()
{
    if (!gParts->buttonArrowWidget) {
        ensure_toggle_button_widget();

        gParts->buttonArrowWidget = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
        gtk_container_add(GTK_CONTAINER(gParts->toggleButtonWidget), gParts->buttonArrowWidget);
        gtk_widget_realize(gParts->buttonArrowWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_checkbox_widget()
{
    if (!gParts->checkboxWidget) {
        gParts->checkboxWidget = gtk_check_button_new_with_label("M");
        setup_widget_prototype(gParts->checkboxWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_radiobutton_widget()
{
    if (!gParts->radiobuttonWidget) {
        gParts->radiobuttonWidget = gtk_radio_button_new_with_label(NULL, "M");
        setup_widget_prototype(gParts->radiobuttonWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_scrollbar_widget()
{
    if (!gParts->vertScrollbarWidget) {
        gParts->vertScrollbarWidget = gtk_vscrollbar_new(NULL);
        setup_widget_prototype(gParts->vertScrollbarWidget);
    }
    if (!gParts->horizScrollbarWidget) {
        gParts->horizScrollbarWidget = gtk_hscrollbar_new(NULL);
        setup_widget_prototype(gParts->horizScrollbarWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_spin_widget()
{
  if (!gParts->spinWidget) {
    gParts->spinWidget = gtk_spin_button_new(NULL, 1, 0);
    setup_widget_prototype(gParts->spinWidget);
  }
  return MOZ_GTK_SUCCESS;
}

static gint
ensure_scale_widget()
{
  if (!gParts->hScaleWidget) {
    gParts->hScaleWidget = gtk_hscale_new(NULL);
    setup_widget_prototype(gParts->hScaleWidget);
  }
  if (!gParts->vScaleWidget) {
    gParts->vScaleWidget = gtk_vscale_new(NULL);
    setup_widget_prototype(gParts->vScaleWidget);
  }
  return MOZ_GTK_SUCCESS;
}

static gint
ensure_entry_widget()
{
    if (!gParts->entryWidget) {
        gParts->entryWidget = gtk_entry_new();
        setup_widget_prototype(gParts->entryWidget);
    }
    return MOZ_GTK_SUCCESS;
}

/* We need to have pointers to the inner widgets (button, separator, arrow)
 * of the ComboBox to get the correct rendering from theme engines which
 * special cases their look. Since the inner layout can change, we ask GTK
 * to NULL our pointers when they are about to become invalid because the
 * corresponding widgets don't exist anymore. It's the role of
 * g_object_add_weak_pointer().
 * Note that if we don't find the inner widgets (which shouldn't happen), we
 * fallback to use generic "non-inner" widgets, and they don't need that kind
 * of weak pointer since they are explicit children of gParts->protoWindow and as
 * such GTK holds a strong reference to them. */
static void
moz_gtk_get_combo_box_inner_button(GtkWidget *widget, gpointer client_data)
{
    if (GTK_IS_TOGGLE_BUTTON(widget)) {
        gParts->comboBoxButtonWidget = widget;
        g_object_add_weak_pointer(G_OBJECT(widget),
                                  (gpointer) &gParts->comboBoxButtonWidget);
        gtk_widget_realize(widget);
        g_object_set_data(G_OBJECT(widget), "transparent-bg-hint", GINT_TO_POINTER(TRUE));
    }
}

static void
moz_gtk_get_combo_box_button_inner_widgets(GtkWidget *widget,
                                           gpointer client_data)
{
    if (GTK_IS_SEPARATOR(widget)) {
        gParts->comboBoxSeparatorWidget = widget;
        g_object_add_weak_pointer(G_OBJECT(widget),
                                  (gpointer) &gParts->comboBoxSeparatorWidget);
    } else if (GTK_IS_ARROW(widget)) {
        gParts->comboBoxArrowWidget = widget;
        g_object_add_weak_pointer(G_OBJECT(widget),
                                  (gpointer) &gParts->comboBoxArrowWidget);
    } else
        return;
    gtk_widget_realize(widget);
    g_object_set_data(G_OBJECT(widget), "transparent-bg-hint", GINT_TO_POINTER(TRUE));
}

static gint
ensure_combo_box_widgets()
{
    GtkWidget* buttonChild;

    if (gParts->comboBoxButtonWidget && gParts->comboBoxArrowWidget)
        return MOZ_GTK_SUCCESS;

    /* Create a ComboBox if needed */
    if (!gParts->comboBoxWidget) {
        gParts->comboBoxWidget = gtk_combo_box_new();
        setup_widget_prototype(gParts->comboBoxWidget);
    }

    /* Get its inner Button */
    gtk_container_forall(GTK_CONTAINER(gParts->comboBoxWidget),
                         moz_gtk_get_combo_box_inner_button,
                         NULL);

    if (gParts->comboBoxButtonWidget) {
        /* Get the widgets inside the Button */
        buttonChild = GTK_BIN(gParts->comboBoxButtonWidget)->child;
        if (GTK_IS_HBOX(buttonChild)) {
            /* appears-as-list = FALSE, cell-view = TRUE; the button
             * contains an hbox. This hbox is there because the ComboBox
             * needs to place a cell renderer, a separator, and an arrow in
             * the button when appears-as-list is FALSE. */
            gtk_container_forall(GTK_CONTAINER(buttonChild),
                                 moz_gtk_get_combo_box_button_inner_widgets,
                                 NULL);
        } else if(GTK_IS_ARROW(buttonChild)) {
            /* appears-as-list = TRUE, or cell-view = FALSE;
             * the button only contains an arrow */
            gParts->comboBoxArrowWidget = buttonChild;
            g_object_add_weak_pointer(G_OBJECT(buttonChild), (gpointer)
                                      &gParts->comboBoxArrowWidget);
            gtk_widget_realize(gParts->comboBoxArrowWidget);
            g_object_set_data(G_OBJECT(gParts->comboBoxArrowWidget),
                              "transparent-bg-hint", GINT_TO_POINTER(TRUE));
        }
    } else {
        /* Shouldn't be reached with current internal gtk implementation; we
         * use a generic toggle button as last resort fallback to avoid
         * crashing. */
        ensure_toggle_button_widget();
        gParts->comboBoxButtonWidget = gParts->toggleButtonWidget;
    }

    if (!gParts->comboBoxArrowWidget) {
        /* Shouldn't be reached with current internal gtk implementation;
         * we gParts->buttonArrowWidget as last resort fallback to avoid
         * crashing. */
        ensure_button_arrow_widget();
        gParts->comboBoxArrowWidget = gParts->buttonArrowWidget;
    }

    /* We don't test the validity of gParts->comboBoxSeparatorWidget since there
     * is none when "appears-as-list" = TRUE or "cell-view" = FALSE; if it
     * is invalid we just won't paint it. */

    return MOZ_GTK_SUCCESS;
}

/* We need to have pointers to the inner widgets (entry, button, arrow) of
 * the ComboBoxEntry to get the correct rendering from theme engines which
 * special cases their look. Since the inner layout can change, we ask GTK
 * to NULL our pointers when they are about to become invalid because the
 * corresponding widgets don't exist anymore. It's the role of
 * g_object_add_weak_pointer().
 * Note that if we don't find the inner widgets (which shouldn't happen), we
 * fallback to use generic "non-inner" widgets, and they don't need that kind
 * of weak pointer since they are explicit children of gParts->protoWindow and as
 * such GTK holds a strong reference to them. */
static void
moz_gtk_get_combo_box_entry_inner_widgets(GtkWidget *widget,
                                          gpointer client_data)
{
    if (GTK_IS_TOGGLE_BUTTON(widget)) {
        gParts->comboBoxEntryButtonWidget = widget;
        g_object_add_weak_pointer(G_OBJECT(widget),
                                  (gpointer) &gParts->comboBoxEntryButtonWidget);
    } else if (GTK_IS_ENTRY(widget)) {
        gParts->comboBoxEntryTextareaWidget = widget;
        g_object_add_weak_pointer(G_OBJECT(widget),
                                  (gpointer) &gParts->comboBoxEntryTextareaWidget);
    } else
        return;
    gtk_widget_realize(widget);
    g_object_set_data(G_OBJECT(widget), "transparent-bg-hint", GINT_TO_POINTER(TRUE));
}

static void
moz_gtk_get_combo_box_entry_arrow(GtkWidget *widget, gpointer client_data)
{
    if (GTK_IS_ARROW(widget)) {
        gParts->comboBoxEntryArrowWidget = widget;
        g_object_add_weak_pointer(G_OBJECT(widget),
                                  (gpointer) &gParts->comboBoxEntryArrowWidget);
        gtk_widget_realize(widget);
        g_object_set_data(G_OBJECT(widget), "transparent-bg-hint", GINT_TO_POINTER(TRUE));
    }
}

static gint
ensure_combo_box_entry_widgets()
{
    GtkWidget* buttonChild;

    if (gParts->comboBoxEntryTextareaWidget &&
            gParts->comboBoxEntryButtonWidget &&
            gParts->comboBoxEntryArrowWidget)
        return MOZ_GTK_SUCCESS;

    /* Create a ComboBoxEntry if needed */
    if (!gParts->comboBoxEntryWidget) {
        gParts->comboBoxEntryWidget = gtk_combo_box_entry_new();
        setup_widget_prototype(gParts->comboBoxEntryWidget);
    }

    /* Get its inner Entry and Button */
    gtk_container_forall(GTK_CONTAINER(gParts->comboBoxEntryWidget),
                         moz_gtk_get_combo_box_entry_inner_widgets,
                         NULL);

    if (!gParts->comboBoxEntryTextareaWidget) {
        ensure_entry_widget();
        gParts->comboBoxEntryTextareaWidget = gParts->entryWidget;
    }

    if (gParts->comboBoxEntryButtonWidget) {
        /* Get the Arrow inside the Button */
        buttonChild = GTK_BIN(gParts->comboBoxEntryButtonWidget)->child;
        if (GTK_IS_HBOX(buttonChild)) {
            /* appears-as-list = FALSE, cell-view = TRUE; the button
             * contains an hbox. This hbox is there because ComboBoxEntry
             * inherits from ComboBox which needs to place a cell renderer,
             * a separator, and an arrow in the button when appears-as-list
             * is FALSE. Here the hbox should only contain an arrow, since
             * a ComboBoxEntry doesn't need all those widgets in the
             * button. */
            gtk_container_forall(GTK_CONTAINER(buttonChild),
                                 moz_gtk_get_combo_box_entry_arrow,
                                 NULL);
        } else if(GTK_IS_ARROW(buttonChild)) {
            /* appears-as-list = TRUE, or cell-view = FALSE;
             * the button only contains an arrow */
            gParts->comboBoxEntryArrowWidget = buttonChild;
            g_object_add_weak_pointer(G_OBJECT(buttonChild), (gpointer)
                                      &gParts->comboBoxEntryArrowWidget);
            gtk_widget_realize(gParts->comboBoxEntryArrowWidget);
            g_object_set_data(G_OBJECT(gParts->comboBoxEntryArrowWidget),
                              "transparent-bg-hint", GINT_TO_POINTER(TRUE));
        }
    } else {
        /* Shouldn't be reached with current internal gtk implementation;
         * we use a generic toggle button as last resort fallback to avoid
         * crashing. */
        ensure_toggle_button_widget();
        gParts->comboBoxEntryButtonWidget = gParts->toggleButtonWidget;
    }

    if (!gParts->comboBoxEntryArrowWidget) {
        /* Shouldn't be reached with current internal gtk implementation;
         * we gParts->buttonArrowWidget as last resort fallback to avoid
         * crashing. */
        ensure_button_arrow_widget();
        gParts->comboBoxEntryArrowWidget = gParts->buttonArrowWidget;
    }

    return MOZ_GTK_SUCCESS;
}


static gint
ensure_handlebox_widget()
{
    if (!gParts->handleBoxWidget) {
        gParts->handleBoxWidget = gtk_handle_box_new();
        setup_widget_prototype(gParts->handleBoxWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_toolbar_widget()
{
    if (!gParts->toolbarWidget) {
        ensure_handlebox_widget();
        gParts->toolbarWidget = gtk_toolbar_new();
        gtk_container_add(GTK_CONTAINER(gParts->handleBoxWidget), gParts->toolbarWidget);
        gtk_widget_realize(gParts->toolbarWidget);
        g_object_set_data(G_OBJECT(gParts->toolbarWidget), "transparent-bg-hint", GINT_TO_POINTER(TRUE));
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_toolbar_separator_widget()
{
    if (!gParts->toolbarSeparatorWidget) {
        ensure_toolbar_widget();
        gParts->toolbarSeparatorWidget = GTK_WIDGET(gtk_separator_tool_item_new());
        setup_widget_prototype(gParts->toolbarSeparatorWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_tooltip_widget()
{
    if (!gParts->tooltipWidget) {
        gParts->tooltipWidget = gtk_window_new(GTK_WINDOW_POPUP);
        gtk_widget_realize(gParts->tooltipWidget);
        moz_gtk_set_widget_name(gParts->tooltipWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_tab_widget()
{
    if (!gParts->tabWidget) {
        gParts->tabWidget = gtk_notebook_new();
        setup_widget_prototype(gParts->tabWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_progress_widget()
{
    if (!gParts->progresWidget) {
        gParts->progresWidget = gtk_progress_bar_new();
        setup_widget_prototype(gParts->progresWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_statusbar_widget()
{
    if (!gParts->statusbarWidget) {
      gParts->statusbarWidget = gtk_statusbar_new();
      setup_widget_prototype(gParts->statusbarWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_frame_widget()
{
    if (!gParts->frameWidget) {
        ensure_statusbar_widget();
        gParts->frameWidget = gtk_frame_new(NULL);
        gtk_container_add(GTK_CONTAINER(gParts->statusbarWidget), gParts->frameWidget);
        gtk_widget_realize(gParts->frameWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_menu_bar_widget()
{
    if (!gParts->menuBarWidget) {
        gParts->menuBarWidget = gtk_menu_bar_new();
        setup_widget_prototype(gParts->menuBarWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_menu_bar_item_widget()
{
    if (!gParts->menuBarItemWidget) {
        ensure_menu_bar_widget();
        gParts->menuBarItemWidget = gtk_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(gParts->menuBarWidget),
                              gParts->menuBarItemWidget);
        gtk_widget_realize(gParts->menuBarItemWidget);
        g_object_set_data(G_OBJECT(gParts->menuBarItemWidget),
                          "transparent-bg-hint", GINT_TO_POINTER(TRUE));
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_menu_popup_widget()
{
    if (!gParts->menuPopupWidget) {
        ensure_menu_bar_item_widget();
        gParts->menuPopupWidget = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(gParts->menuBarItemWidget),
                                  gParts->menuPopupWidget);
        gtk_widget_realize(gParts->menuPopupWidget);
        g_object_set_data(G_OBJECT(gParts->menuPopupWidget),
                          "transparent-bg-hint", GINT_TO_POINTER(TRUE));
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_menu_item_widget()
{
    if (!gParts->menuItemWidget) {
        ensure_menu_popup_widget();
        gParts->menuItemWidget = gtk_menu_item_new_with_label("M");
        gtk_menu_shell_append(GTK_MENU_SHELL(gParts->menuPopupWidget),
                              gParts->menuItemWidget);
        gtk_widget_realize(gParts->menuItemWidget);
        g_object_set_data(G_OBJECT(gParts->menuItemWidget),
                          "transparent-bg-hint", GINT_TO_POINTER(TRUE));
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_image_menu_item_widget()
{
    if (!gParts->imageMenuItemWidget) {
        ensure_menu_popup_widget();
        gParts->imageMenuItemWidget = gtk_image_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(gParts->menuPopupWidget),
                              gParts->imageMenuItemWidget);
        gtk_widget_realize(gParts->imageMenuItemWidget);
        g_object_set_data(G_OBJECT(gParts->imageMenuItemWidget),
                          "transparent-bg-hint", GINT_TO_POINTER(TRUE));
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_menu_separator_widget()
{
    if (!gParts->menuSeparatorWidget) {
        ensure_menu_popup_widget();
        gParts->menuSeparatorWidget = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(gParts->menuPopupWidget),
                              gParts->menuSeparatorWidget);
        gtk_widget_realize(gParts->menuSeparatorWidget);
        g_object_set_data(G_OBJECT(gParts->menuSeparatorWidget),
                          "transparent-bg-hint", GINT_TO_POINTER(TRUE));
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_check_menu_item_widget()
{
    if (!gParts->checkMenuItemWidget) {
        ensure_menu_popup_widget();
        gParts->checkMenuItemWidget = gtk_check_menu_item_new_with_label("M");
        gtk_menu_shell_append(GTK_MENU_SHELL(gParts->menuPopupWidget),
                              gParts->checkMenuItemWidget);
        gtk_widget_realize(gParts->checkMenuItemWidget);
        g_object_set_data(G_OBJECT(gParts->checkMenuItemWidget),
                          "transparent-bg-hint", GINT_TO_POINTER(TRUE));
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_tree_view_widget()
{
    if (!gParts->treeViewWidget) {
        gParts->treeViewWidget = gtk_tree_view_new();
        setup_widget_prototype(gParts->treeViewWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_tree_header_cell_widget()
{
    if(!gParts->treeHeaderCellWidget) {
        /*
         * Some GTK engines paint the first and last cell
         * of a TreeView header with a highlight.
         * Since we do not know where our widget will be relative
         * to the other buttons in the TreeView header, we must
         * paint it as a button that is between two others,
         * thus ensuring it is neither the first or last button
         * in the header.
         * GTK doesn't give us a way to do this explicitly,
         * so we must paint with a button that is between two
         * others.
         */

        GtkTreeViewColumn* firstTreeViewColumn;
        GtkTreeViewColumn* lastTreeViewColumn;

        ensure_tree_view_widget();

        /* Create and append our three columns */
        firstTreeViewColumn = gtk_tree_view_column_new();
        gtk_tree_view_column_set_title(firstTreeViewColumn, "M");
        gtk_tree_view_append_column(GTK_TREE_VIEW(gParts->treeViewWidget), firstTreeViewColumn);

        gParts->middleTreeViewColumn = gtk_tree_view_column_new();
        gtk_tree_view_column_set_title(gParts->middleTreeViewColumn, "M");
        gtk_tree_view_append_column(GTK_TREE_VIEW(gParts->treeViewWidget),
                                    gParts->middleTreeViewColumn);

        lastTreeViewColumn = gtk_tree_view_column_new();
        gtk_tree_view_column_set_title(lastTreeViewColumn, "M");
        gtk_tree_view_append_column(GTK_TREE_VIEW(gParts->treeViewWidget), lastTreeViewColumn);

        /* Use the middle column's header for our button */
        gParts->treeHeaderCellWidget = gParts->middleTreeViewColumn->button;
        gParts->treeHeaderSortArrowWidget = gParts->middleTreeViewColumn->arrow;
        g_object_set_data(G_OBJECT(gParts->treeHeaderCellWidget),
                          "transparent-bg-hint", GINT_TO_POINTER(TRUE));
        g_object_set_data(G_OBJECT(gParts->treeHeaderSortArrowWidget),
                          "transparent-bg-hint", GINT_TO_POINTER(TRUE));
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_expander_widget()
{
    if (!gParts->expanderWidget) {
        gParts->expanderWidget = gtk_expander_new("M");
        setup_widget_prototype(gParts->expanderWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_scrolled_window_widget()
{
    if (!gParts->scrolledWindowWidget) {
        gParts->scrolledWindowWidget = gtk_scrolled_window_new(NULL, NULL);
        setup_widget_prototype(gParts->scrolledWindowWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static GtkStateType
ConvertGtkState(GtkWidgetState* state)
{
    if (state->disabled)
        return GTK_STATE_INSENSITIVE;
    else if (state->depressed)
        return (state->inHover ? GTK_STATE_PRELIGHT : GTK_STATE_ACTIVE);
    else if (state->inHover)
        return (state->active ? GTK_STATE_ACTIVE : GTK_STATE_PRELIGHT);
    else
        return GTK_STATE_NORMAL;
}

static gint
TSOffsetStyleGCArray(GdkGC** gcs, gint xorigin, gint yorigin)
{
    int i;
    /* there are 5 gc's in each array, for each of the widget states */
    for (i = 0; i < 5; ++i)
        gdk_gc_set_ts_origin(gcs[i], xorigin, yorigin);
    return MOZ_GTK_SUCCESS;
}

static gint
TSOffsetStyleGCs(GtkStyle* style, gint xorigin, gint yorigin)
{
    TSOffsetStyleGCArray(style->fg_gc, xorigin, yorigin);
    TSOffsetStyleGCArray(style->bg_gc, xorigin, yorigin);
    TSOffsetStyleGCArray(style->light_gc, xorigin, yorigin);
    TSOffsetStyleGCArray(style->dark_gc, xorigin, yorigin);
    TSOffsetStyleGCArray(style->mid_gc, xorigin, yorigin);
    TSOffsetStyleGCArray(style->text_gc, xorigin, yorigin);
    TSOffsetStyleGCArray(style->base_gc, xorigin, yorigin);
    gdk_gc_set_ts_origin(style->black_gc, xorigin, yorigin);
    gdk_gc_set_ts_origin(style->white_gc, xorigin, yorigin);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_button_paint(GdkDrawable* drawable, GdkRectangle* rect,
                     GdkRectangle* cliprect, GtkWidgetState* state,
                     GtkReliefStyle relief, GtkWidget* widget,
                     GtkTextDirection direction)
{
    GtkShadowType shadow_type;
    GtkStyle* style = widget->style;
    GtkStateType button_state = ConvertGtkState(state);
    gint x = rect->x, y=rect->y, width=rect->width, height=rect->height;

    gboolean interior_focus;
    gint focus_width, focus_pad;

    moz_gtk_widget_get_focus(widget, &interior_focus, &focus_width, &focus_pad);

    if (WINDOW_IS_MAPPED(drawable)) {
        gdk_window_set_back_pixmap(drawable, NULL, TRUE);
        gdk_window_clear_area(drawable, cliprect->x, cliprect->y,
                              cliprect->width, cliprect->height);
    }

    gtk_widget_set_state(widget, button_state);
    gtk_widget_set_direction(widget, direction);

    if (state->isDefault)
        GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_DEFAULT);

    GTK_BUTTON(widget)->relief = relief;

    /* Some theme engines love to cause us pain in that gtk_paint_focus is a
       no-op on buttons and button-like widgets. They only listen to this flag. */
    if (state->focused && !state->disabled)
        GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_FOCUS);

    if (!interior_focus && state->focused) {
        x += focus_width + focus_pad;
        y += focus_width + focus_pad;
        width -= 2 * (focus_width + focus_pad);
        height -= 2 * (focus_width + focus_pad);
    }

    shadow_type = button_state == GTK_STATE_ACTIVE ||
                      state->depressed ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
 
    if (state->isDefault && relief == GTK_RELIEF_NORMAL) {
        gtk_paint_box(style, drawable, button_state, shadow_type, cliprect,
                      widget, "buttondefault", x, y, width, height);                   
    }
 
    if (relief != GTK_RELIEF_NONE || state->depressed ||
           (button_state != GTK_STATE_NORMAL &&
            button_state != GTK_STATE_INSENSITIVE)) {
        TSOffsetStyleGCs(style, x, y);
        /* the following line can trigger an assertion (Crux theme)
           file ../../gdk/gdkwindow.c: line 1846 (gdk_window_clear_area):
           assertion `GDK_IS_WINDOW (window)' failed */
        gtk_paint_box(style, drawable, button_state, shadow_type, cliprect,
                      widget, "button", x, y, width, height);
    }

    if (state->focused) {
        if (interior_focus) {
            x += widget->style->xthickness + focus_pad;
            y += widget->style->ythickness + focus_pad;
            width -= 2 * (widget->style->xthickness + focus_pad);
            height -= 2 * (widget->style->ythickness + focus_pad);
        } else {
            x -= focus_width + focus_pad;
            y -= focus_width + focus_pad;
            width += 2 * (focus_width + focus_pad);
            height += 2 * (focus_width + focus_pad);
        }

        TSOffsetStyleGCs(style, x, y);
        gtk_paint_focus(style, drawable, button_state, cliprect,
                        widget, "button", x, y, width, height);
    }

    GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_DEFAULT);
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_FOCUS);
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_init()
{
    GtkWidgetClass *entry_class;

    is_initialized = TRUE;
    have_arrow_scaling = (gtk_major_version > 2 ||
                          (gtk_major_version == 2 && gtk_minor_version >= 12));

    /* Add style property to GtkEntry.
     * Adding the style property to the normal GtkEntry class means that it
     * will work without issues inside GtkComboBox and for Spinbuttons. */
    entry_class = g_type_class_ref(GTK_TYPE_ENTRY);
    gtk_widget_class_install_style_property(entry_class,
        g_param_spec_boolean("honors-transparent-bg-hint",
                             "Transparent BG enabling flag",
                             "If TRUE, the theme is able to draw the GtkEntry on non-prefilled background.",
                             FALSE,
                             G_PARAM_READWRITE));

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_checkbox_get_metrics(gint* indicator_size, gint* indicator_spacing)
{
    ensure_checkbox_widget();

    gtk_widget_style_get (gParts->checkboxWidget,
                          "indicator_size", indicator_size,
                          "indicator_spacing", indicator_spacing,
                          NULL);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_radio_get_metrics(gint* indicator_size, gint* indicator_spacing)
{
    ensure_radiobutton_widget();

    gtk_widget_style_get (gParts->radiobuttonWidget,
                          "indicator_size", indicator_size,
                          "indicator_spacing", indicator_spacing,
                          NULL);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_widget_get_focus(GtkWidget* widget, gboolean* interior_focus,
                         gint* focus_width, gint* focus_pad) 
{
    gtk_widget_style_get (widget,
                          "interior-focus", interior_focus,
                          "focus-line-width", focus_width,
                          "focus-padding", focus_pad,
                          NULL);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_splitter_get_metrics(gint orientation, gint* size)
{
    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        ensure_hpaned_widget();
        gtk_widget_style_get(gParts->hpanedWidget, "handle_size", size, NULL);
    } else {
        ensure_vpaned_widget();
        gtk_widget_style_get(gParts->vpanedWidget, "handle_size", size, NULL);
    }
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_button_get_inner_border(GtkWidget* widget, GtkBorder* inner_border)
{
    static const GtkBorder default_inner_border = { 1, 1, 1, 1 };
    GtkBorder *tmp_border;

    gtk_widget_style_get (widget, "inner-border", &tmp_border, NULL);

    if (tmp_border) {
        *inner_border = *tmp_border;
        gtk_border_free(tmp_border);
    }
    else
        *inner_border = default_inner_border;

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_toggle_paint(GdkDrawable* drawable, GdkRectangle* rect,
                     GdkRectangle* cliprect, GtkWidgetState* state,
                     gboolean selected, gboolean inconsistent,
                     gboolean isradio, GtkTextDirection direction)
{
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = (selected)?GTK_SHADOW_IN:GTK_SHADOW_OUT;
    gint indicator_size, indicator_spacing;
    gint x, y, width, height;
    gint focus_x, focus_y, focus_width, focus_height;
    GtkWidget *w;
    GtkStyle *style;

    if (isradio) {
        moz_gtk_radio_get_metrics(&indicator_size, &indicator_spacing);
        w = gParts->radiobuttonWidget;
    } else {
        moz_gtk_checkbox_get_metrics(&indicator_size, &indicator_spacing);
        w = gParts->checkboxWidget;
    }

    // "GetMinimumWidgetSize was ignored"
    // FIXME: This assert causes a build failure in WebKitGTK+ debug
    // builds, because it uses 'false' in its definition. We may want
    // to force this file to be built with g++, by renaming it.
    // ASSERT(rect->width == indicator_size);

    /*
     * vertically center in the box, since XUL sometimes ignores our
     * GetMinimumWidgetSize in the vertical dimension
     */
    x = rect->x;
    y = rect->y + (rect->height - indicator_size) / 2;
    width = indicator_size;
    height = indicator_size;

    focus_x = x - indicator_spacing;
    focus_y = y - indicator_spacing;
    focus_width = width + 2 * indicator_spacing;
    focus_height = height + 2 * indicator_spacing;
  
    style = w->style;
    TSOffsetStyleGCs(style, x, y);

    gtk_widget_set_sensitive(w, !state->disabled);
    gtk_widget_set_direction(w, direction);
    GTK_TOGGLE_BUTTON(w)->active = selected;
      
    if (isradio) {
        gtk_paint_option(style, drawable, state_type, shadow_type, cliprect,
                         gParts->radiobuttonWidget, "radiobutton", x, y,
                         width, height);
        if (state->focused) {
            gtk_paint_focus(style, drawable, GTK_STATE_ACTIVE, cliprect,
                            gParts->radiobuttonWidget, "radiobutton", focus_x, focus_y,
                            focus_width, focus_height);
        }
    }
    else {
       /*
        * 'indeterminate' type on checkboxes. In GTK, the shadow type
        * must also be changed for the state to be drawn.
        */
        if (inconsistent) {
            gtk_toggle_button_set_inconsistent(GTK_TOGGLE_BUTTON(gParts->checkboxWidget), TRUE);
            shadow_type = GTK_SHADOW_ETCHED_IN;
        } else {
            gtk_toggle_button_set_inconsistent(GTK_TOGGLE_BUTTON(gParts->checkboxWidget), FALSE);
        }

        gtk_paint_check(style, drawable, state_type, shadow_type, cliprect, 
                        gParts->checkboxWidget, "checkbutton", x, y, width, height);
        if (state->focused) {
            gtk_paint_focus(style, drawable, GTK_STATE_ACTIVE, cliprect,
                            gParts->checkboxWidget, "checkbutton", focus_x, focus_y,
                            focus_width, focus_height);
        }
    }

    return MOZ_GTK_SUCCESS;
}

static gint
calculate_button_inner_rect(GtkWidget* button, GdkRectangle* rect,
                            GdkRectangle* inner_rect,
                            GtkTextDirection direction,
                            gboolean ignore_focus)
{
    GtkBorder inner_border;
    gboolean interior_focus;
    gint focus_width, focus_pad;
    GtkStyle* style;

    style = button->style;

    /* This mirrors gtkbutton's child positioning */
    moz_gtk_button_get_inner_border(button, &inner_border);
    moz_gtk_widget_get_focus(button, &interior_focus,
                             &focus_width, &focus_pad);

    if (ignore_focus)
        focus_width = focus_pad = 0;

    inner_rect->x = rect->x + XTHICKNESS(style) + focus_width + focus_pad;
    inner_rect->x += direction == GTK_TEXT_DIR_LTR ?
                        inner_border.left : inner_border.right;
    inner_rect->y = rect->y + inner_border.top + YTHICKNESS(style) +
                    focus_width + focus_pad;
    inner_rect->width = MAX(1, rect->width - inner_border.left -
       inner_border.right - (XTHICKNESS(style) + focus_pad + focus_width) * 2);
    inner_rect->height = MAX(1, rect->height - inner_border.top -
       inner_border.bottom - (YTHICKNESS(style) + focus_pad + focus_width) * 2);

    return MOZ_GTK_SUCCESS;
}


static gint
calculate_arrow_rect(GtkWidget* arrow, GdkRectangle* rect,
                     GdkRectangle* arrow_rect, GtkTextDirection direction)
{
    /* defined in gtkarrow.c */
    gfloat arrow_scaling = 0.7;
    gfloat xalign, xpad;
    gint extent;
    GtkMisc* misc = GTK_MISC(arrow);

    if (have_arrow_scaling)
        gtk_widget_style_get(arrow, "arrow_scaling", &arrow_scaling, NULL);

    extent = MIN((rect->width - misc->xpad * 2),
                 (rect->height - misc->ypad * 2)) * arrow_scaling;

    xalign = direction == GTK_TEXT_DIR_LTR ? misc->xalign : 1.0 - misc->xalign;
    xpad = misc->xpad + (rect->width - extent) * xalign;

    arrow_rect->x = direction == GTK_TEXT_DIR_LTR ?
                        floor(rect->x + xpad) : ceil(rect->x + xpad);
    arrow_rect->y = floor(rect->y + misc->ypad +
                          ((rect->height - extent) * misc->yalign));

    arrow_rect->width = arrow_rect->height = extent;

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scrollbar_button_paint(GdkDrawable* drawable, GdkRectangle* rect,
                               GdkRectangle* cliprect, GtkWidgetState* state,
                               GtkScrollbarButtonFlags flags,
                               GtkTextDirection direction)
{
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = (state->active) ?
        GTK_SHADOW_IN : GTK_SHADOW_OUT;
    GdkRectangle arrow_rect;
    GtkStyle* style;
    GtkWidget *scrollbar;
    GtkArrowType arrow_type;
    gint arrow_displacement_x, arrow_displacement_y;
    const char* detail = (flags & MOZ_GTK_STEPPER_VERTICAL) ?
                           "vscrollbar" : "hscrollbar";

    ensure_scrollbar_widget();

    if (flags & MOZ_GTK_STEPPER_VERTICAL)
        scrollbar = gParts->vertScrollbarWidget;
    else
        scrollbar = gParts->horizScrollbarWidget;

    gtk_widget_set_direction(scrollbar, direction);

    /* Some theme engines (i.e., ClearLooks) check the scrollbar's allocation
       to determine where it should paint rounded corners on the buttons.
       We need to trick them into drawing the buttons the way we want them. */

    scrollbar->allocation.x = rect->x;
    scrollbar->allocation.y = rect->y;
    scrollbar->allocation.width = rect->width;
    scrollbar->allocation.height = rect->height;

    if (flags & MOZ_GTK_STEPPER_VERTICAL) {
        scrollbar->allocation.height *= 5;
        if (flags & MOZ_GTK_STEPPER_DOWN) {
            arrow_type = GTK_ARROW_DOWN;
            if (flags & MOZ_GTK_STEPPER_BOTTOM)
                scrollbar->allocation.y -= 4 * rect->height;
            else
                scrollbar->allocation.y -= rect->height;

        } else {
            arrow_type = GTK_ARROW_UP;
            if (flags & MOZ_GTK_STEPPER_BOTTOM)
                scrollbar->allocation.y -= 3 * rect->height;
        }
    } else {
        scrollbar->allocation.width *= 5;
        if (flags & MOZ_GTK_STEPPER_DOWN) {
            arrow_type = GTK_ARROW_RIGHT;
            if (flags & MOZ_GTK_STEPPER_BOTTOM)
                scrollbar->allocation.x -= 4 * rect->width;
            else
                scrollbar->allocation.x -= rect->width;
        } else {
            arrow_type = GTK_ARROW_LEFT;
            if (flags & MOZ_GTK_STEPPER_BOTTOM)
                scrollbar->allocation.x -= 3 * rect->width;
        }
    }

    style = scrollbar->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);

    gtk_paint_box(style, drawable, state_type, shadow_type, cliprect,
                  scrollbar, detail, rect->x, rect->y,
                  rect->width, rect->height);

    arrow_rect.width = rect->width / 2;
    arrow_rect.height = rect->height / 2;
    arrow_rect.x = rect->x + (rect->width - arrow_rect.width) / 2;
    arrow_rect.y = rect->y + (rect->height - arrow_rect.height) / 2;

    if (state_type == GTK_STATE_ACTIVE) {
        gtk_widget_style_get(scrollbar,
                             "arrow-displacement-x", &arrow_displacement_x,
                             "arrow-displacement-y", &arrow_displacement_y,
                             NULL);

        arrow_rect.x += arrow_displacement_x;
        arrow_rect.y += arrow_displacement_y;
    }

    gtk_paint_arrow(style, drawable, state_type, shadow_type, cliprect,
                    scrollbar, detail, arrow_type, TRUE, arrow_rect.x,
                    arrow_rect.y, arrow_rect.width, arrow_rect.height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scrollbar_trough_paint(GtkThemeWidgetType widget,
                               GdkDrawable* drawable, GdkRectangle* rect,
                               GdkRectangle* cliprect, GtkWidgetState* state,
                               GtkTextDirection direction)
{
    GtkStyle* style;
    GtkScrollbar *scrollbar;

    ensure_scrollbar_widget();

    if (widget ==  MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL)
        scrollbar = GTK_SCROLLBAR(gParts->horizScrollbarWidget);
    else
        scrollbar = GTK_SCROLLBAR(gParts->vertScrollbarWidget);

    gtk_widget_set_direction(GTK_WIDGET(scrollbar), direction);

    style = GTK_WIDGET(scrollbar)->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_style_apply_default_background(style, drawable, TRUE, GTK_STATE_ACTIVE,
                                       cliprect, rect->x, rect->y,
                                       rect->width, rect->height);

    gtk_paint_box(style, drawable, GTK_STATE_ACTIVE, GTK_SHADOW_IN, cliprect,
                  GTK_WIDGET(scrollbar), "trough", rect->x, rect->y,
                  rect->width, rect->height);

    if (state->focused) {
        gtk_paint_focus(style, drawable, GTK_STATE_ACTIVE, cliprect,
                        GTK_WIDGET(scrollbar), "trough",
                        rect->x, rect->y, rect->width, rect->height);
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scrollbar_thumb_paint(GtkThemeWidgetType widget,
                              GdkDrawable* drawable, GdkRectangle* rect,
                              GdkRectangle* cliprect, GtkWidgetState* state,
                              GtkTextDirection direction)
{
    GtkStateType state_type = (state->inHover || state->active) ?
        GTK_STATE_PRELIGHT : GTK_STATE_NORMAL;
    GtkShadowType shadow_type = GTK_SHADOW_OUT;
    GtkStyle* style;
    GtkScrollbar *scrollbar;
    GtkAdjustment *adj;
    gboolean activate_slider;

    ensure_scrollbar_widget();

    if (widget == MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL)
        scrollbar = GTK_SCROLLBAR(gParts->horizScrollbarWidget);
    else
        scrollbar = GTK_SCROLLBAR(gParts->vertScrollbarWidget);

    gtk_widget_set_direction(GTK_WIDGET(scrollbar), direction);

    /* Make sure to set the scrollbar range before painting so that
       everything is drawn properly.  At least the bluecurve (and
       maybe other) themes don't draw the top or bottom black line
       surrounding the scrollbar if the theme thinks that it's butted
       up against the scrollbar arrows.  Note the increases of the
       clip rect below. */
    /* Changing the cliprect is pretty bogus. This lets themes draw
       outside the frame, which means we don't invalidate them
       correctly. See bug 297508. But some themes do seem to need
       it. So we modify the frame's overflow area to account for what
       we're doing here; see nsNativeThemeGTK::GetWidgetOverflow. */
    adj = gtk_range_get_adjustment(GTK_RANGE(scrollbar));

    if (widget == MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL) {
        cliprect->x -= 1;
        cliprect->width += 2;
        adj->page_size = rect->width;
    }
    else {
        cliprect->y -= 1;
        cliprect->height += 2;
        adj->page_size = rect->height;
    }

    adj->lower = 0;
    adj->value = state->curpos;
    adj->upper = state->maxpos;
    gtk_adjustment_changed(adj);

    style = GTK_WIDGET(scrollbar)->style;
    
    gtk_widget_style_get(GTK_WIDGET(scrollbar), "activate-slider",
                         &activate_slider, NULL);
    
    if (activate_slider && state->active) {
        shadow_type = GTK_SHADOW_IN;
        state_type = GTK_STATE_ACTIVE;
    }

    TSOffsetStyleGCs(style, rect->x, rect->y);

    gtk_paint_slider(style, drawable, state_type, shadow_type, cliprect,
                     GTK_WIDGET(scrollbar), "slider", rect->x, rect->y,
                     rect->width,  rect->height,
                     (widget == MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL) ?
                     GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_spin_paint(GdkDrawable* drawable, GdkRectangle* rect,
                   GtkTextDirection direction)
{
    GtkStyle* style;

    ensure_spin_widget();
    gtk_widget_set_direction(gParts->spinWidget, direction);
    style = gParts->spinWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_IN, NULL,
                  gParts->spinWidget, "spinbutton",
                  rect->x, rect->y, rect->width, rect->height);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_spin_updown_paint(GdkDrawable* drawable, GdkRectangle* rect,
                          gboolean isDown, GtkWidgetState* state,
                          GtkTextDirection direction)
{
    GdkRectangle arrow_rect;
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = state_type == GTK_STATE_ACTIVE ?
                                  GTK_SHADOW_IN : GTK_SHADOW_OUT;
    GtkStyle* style;

    ensure_spin_widget();
    style = gParts->spinWidget->style;
    gtk_widget_set_direction(gParts->spinWidget, direction);

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box(style, drawable, state_type, shadow_type, NULL, gParts->spinWidget,
                  isDown ? "spinbutton_down" : "spinbutton_up",
                  rect->x, rect->y, rect->width, rect->height);

    /* hard code these values */
    arrow_rect.width = 6;
    arrow_rect.height = 6;
    arrow_rect.x = rect->x + (rect->width - arrow_rect.width) / 2;
    arrow_rect.y = rect->y + (rect->height - arrow_rect.height) / 2;
    arrow_rect.y += isDown ? -1 : 1;

    gtk_paint_arrow(style, drawable, state_type, shadow_type, NULL,
                    gParts->spinWidget, "spinbutton",
                    isDown ? GTK_ARROW_DOWN : GTK_ARROW_UP, TRUE,
                    arrow_rect.x, arrow_rect.y,
                    arrow_rect.width, arrow_rect.height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scale_paint(GdkDrawable* drawable, GdkRectangle* rect,
                    GdkRectangle* cliprect, GtkWidgetState* state,
                    GtkOrientation flags, GtkTextDirection direction)
{
  gint x = 0, y = 0;
  GtkStateType state_type = ConvertGtkState(state);
  GtkStyle* style;
  GtkWidget* widget;

  ensure_scale_widget();
  widget = ((flags == GTK_ORIENTATION_HORIZONTAL) ? gParts->hScaleWidget : gParts->vScaleWidget);
  gtk_widget_set_direction(widget, direction);

  style = widget->style;

  if (flags == GTK_ORIENTATION_HORIZONTAL) {
    x = XTHICKNESS(style);
    y++;
  }
  else {
    x++;
    y = YTHICKNESS(style);
  }

  TSOffsetStyleGCs(style, rect->x, rect->y);
  gtk_style_apply_default_background(style, drawable, TRUE, GTK_STATE_NORMAL,
                                     cliprect, rect->x, rect->y,
                                     rect->width, rect->height);

  gtk_paint_box(style, drawable, GTK_STATE_ACTIVE, GTK_SHADOW_IN, cliprect,
                widget, "trough", rect->x + x, rect->y + y,
                rect->width - 2*x, rect->height - 2*y);

  if (state->focused)
    gtk_paint_focus(style, drawable, state_type, cliprect, widget, "trough",
                    rect->x, rect->y, rect->width, rect->height);

  return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scale_thumb_paint(GdkDrawable* drawable, GdkRectangle* rect,
                          GdkRectangle* cliprect, GtkWidgetState* state,
                          GtkOrientation flags, GtkTextDirection direction)
{
  GtkStateType state_type = ConvertGtkState(state);
  GtkStyle* style;
  GtkWidget* widget;
  gint thumb_width, thumb_height, x, y;

  ensure_scale_widget();
  widget = ((flags == GTK_ORIENTATION_HORIZONTAL) ? gParts->hScaleWidget : gParts->vScaleWidget);
  gtk_widget_set_direction(widget, direction);

  style = widget->style;

  /* determine the thumb size, and position the thumb in the center in the opposite axis */
  if (flags == GTK_ORIENTATION_HORIZONTAL) {
    moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_HORIZONTAL, &thumb_width, &thumb_height);
    x = rect->x;
    y = rect->y + (rect->height - thumb_height) / 2;
  }
  else {
    moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_VERTICAL, &thumb_height, &thumb_width);
    x = rect->x + (rect->width - thumb_width) / 2;
    y = rect->y;
  }

  TSOffsetStyleGCs(style, rect->x, rect->y);
  gtk_paint_slider(style, drawable, state_type, GTK_SHADOW_OUT, cliprect,
                   widget, (flags == GTK_ORIENTATION_HORIZONTAL) ? "hscale" : "vscale",
                   x, y, thumb_width, thumb_height, flags);

  return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_gripper_paint(GdkDrawable* drawable, GdkRectangle* rect,
                      GdkRectangle* cliprect, GtkWidgetState* state,
                      GtkTextDirection direction)
{
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type;
    GtkStyle* style;

    ensure_handlebox_widget();
    gtk_widget_set_direction(gParts->handleBoxWidget, direction);

    style = gParts->handleBoxWidget->style;
    shadow_type = GTK_HANDLE_BOX(gParts->handleBoxWidget)->shadow_type;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box(style, drawable, state_type, shadow_type, cliprect,
                  gParts->handleBoxWidget, "handlebox_bin", rect->x, rect->y,
                  rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_hpaned_paint(GdkDrawable* drawable, GdkRectangle* rect,
                     GdkRectangle* cliprect, GtkWidgetState* state)
{
    GtkStateType hpaned_state = ConvertGtkState(state);

    ensure_hpaned_widget();
    gtk_paint_handle(gParts->hpanedWidget->style, drawable, hpaned_state,
                     GTK_SHADOW_NONE, cliprect, gParts->hpanedWidget, "paned",
                     rect->x, rect->y, rect->width, rect->height,
                     GTK_ORIENTATION_VERTICAL);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_vpaned_paint(GdkDrawable* drawable, GdkRectangle* rect,
                     GdkRectangle* cliprect, GtkWidgetState* state)
{
    GtkStateType vpaned_state = ConvertGtkState(state);

    ensure_vpaned_widget();
    gtk_paint_handle(gParts->vpanedWidget->style, drawable, vpaned_state,
                     GTK_SHADOW_NONE, cliprect, gParts->vpanedWidget, "paned",
                     rect->x, rect->y, rect->width, rect->height,
                     GTK_ORIENTATION_HORIZONTAL);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_caret_paint(GdkDrawable* drawable, GdkRectangle* rect,
                    GdkRectangle* cliprect, GtkTextDirection direction)
{
    GdkRectangle location = *rect;
    if (direction == GTK_TEXT_DIR_RTL) {
        /* gtk_draw_insertion_cursor ignores location.width */
        location.x = rect->x + rect->width;
    }

    ensure_entry_widget();
    gtk_draw_insertion_cursor(gParts->entryWidget, drawable, cliprect,
                              &location, TRUE, direction, FALSE);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_entry_paint(GdkDrawable* drawable, GdkRectangle* rect,
                    GdkRectangle* cliprect, GtkWidgetState* state,
                    GtkWidget* widget, GtkTextDirection direction)
{
    GtkStateType bg_state = state->disabled ?
                                GTK_STATE_INSENSITIVE : GTK_STATE_NORMAL;
    gint x, y, width = rect->width, height = rect->height;
    GtkStyle* style;
    gboolean interior_focus;
    gboolean theme_honors_transparency = FALSE;
    gint focus_width;

    gtk_widget_set_direction(widget, direction);

    style = widget->style;

    gtk_widget_style_get(widget,
                         "interior-focus", &interior_focus,
                         "focus-line-width", &focus_width,
                         "honors-transparent-bg-hint", &theme_honors_transparency,
                         NULL);

    /* gtkentry.c uses two windows, one for the entire widget and one for the
     * text area inside it. The background of both windows is set to the "base"
     * color of the new state in gtk_entry_state_changed, but only the inner
     * textarea window uses gtk_paint_flat_box when exposed */

    TSOffsetStyleGCs(style, rect->x, rect->y);

    /* This gets us a lovely greyish disabledish look */
    gtk_widget_set_sensitive(widget, !state->disabled);

    /* GTK fills the outer widget window with the base color before drawing the widget.
     * Some older themes rely on this behavior, but many themes nowadays use rounded
     * corners on their widgets. While most GTK apps are blissfully unaware of this
     * problem due to their use of the default window background, we render widgets on
     * many kinds of backgrounds on the web.
     * If the theme is able to cope with transparency, then we can skip pre-filling
     * and notify the theme it will paint directly on the canvas. */
    if (theme_honors_transparency) {
        g_object_set_data(G_OBJECT(widget), "transparent-bg-hint", GINT_TO_POINTER(TRUE));
    } else {
        gdk_draw_rectangle(drawable, style->base_gc[bg_state], TRUE,
                           cliprect->x, cliprect->y, cliprect->width, cliprect->height);
        g_object_set_data(G_OBJECT(widget), "transparent-bg-hint", GINT_TO_POINTER(FALSE));
    }

    /* Get the position of the inner window, see _gtk_entry_get_borders */
    x = XTHICKNESS(style);
    y = YTHICKNESS(style);

    if (!interior_focus) {
        x += focus_width;
        y += focus_width;
    }

    /* Simulate an expose of the inner window */
    gtk_paint_flat_box(style, drawable, bg_state, GTK_SHADOW_NONE,
                       cliprect, widget, "entry_bg",  rect->x + x,
                       rect->y + y, rect->width - 2*x, rect->height - 2*y);

    /* Now paint the shadow and focus border.
     * We do like in gtk_entry_draw_frame, we first draw the shadow, a tad
     * smaller when focused if the focus is not interior, then the focus. */
    x = rect->x;
    y = rect->y;

    if (state->focused && !state->disabled) {
        /* This will get us the lit borders that focused textboxes enjoy on
         * some themes. */
        GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_FOCUS);

        if (!interior_focus) {
            /* Indent the border a little bit if we have exterior focus 
               (this is what GTK does to draw native entries) */
            x += focus_width;
            y += focus_width;
            width -= 2 * focus_width;
            height -= 2 * focus_width;
        }
    }

    gtk_paint_shadow(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_IN,
                     cliprect, widget, "entry", x, y, width, height);

    if (state->focused && !state->disabled) {
        if (!interior_focus) {
            gtk_paint_focus(style, drawable,  GTK_STATE_NORMAL, cliprect,
                            widget, "entry",
                            rect->x, rect->y, rect->width, rect->height);
        }

        /* Now unset the focus flag. We don't want other entries to look
         * like they're focused too! */
        GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_FOCUS);
    }

    return MOZ_GTK_SUCCESS;
}

static gint 
moz_gtk_treeview_paint(GdkDrawable* drawable, GdkRectangle* rect,
                       GdkRectangle* cliprect, GtkWidgetState* state,
                       GtkTextDirection direction)
{
    gint xthickness, ythickness;

    GtkStyle *style;
    GtkStateType state_type;

    ensure_tree_view_widget();
    ensure_scrolled_window_widget();

    gtk_widget_set_direction(gParts->treeViewWidget, direction);
    gtk_widget_set_direction(gParts->scrolledWindowWidget, direction);

    /* only handle disabled and normal states, otherwise the whole background
     * area will be painted differently with other states */
    state_type = state->disabled ? GTK_STATE_INSENSITIVE : GTK_STATE_NORMAL;

    /* In GTK the treeview sets the background of the window
     * which contains the cells to the treeview base color.
     * If we don't set it here the background color will not be correct.*/
    gtk_widget_modify_bg(gParts->treeViewWidget, state_type,
                         &gParts->treeViewWidget->style->base[state_type]);

    style = gParts->scrolledWindowWidget->style;
    xthickness = XTHICKNESS(style);
    ythickness = YTHICKNESS(style);

    TSOffsetStyleGCs(gParts->treeViewWidget->style, rect->x, rect->y);
    TSOffsetStyleGCs(style, rect->x, rect->y);

    gtk_paint_flat_box(gParts->treeViewWidget->style, drawable, state_type,
                       GTK_SHADOW_NONE, cliprect, gParts->treeViewWidget, "treeview",
                       rect->x + xthickness, rect->y + ythickness,
                       rect->width - 2 * xthickness,
                       rect->height - 2 * ythickness);

    gtk_paint_shadow(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_IN,
                     cliprect, gParts->scrolledWindowWidget, "scrolled_window",
                     rect->x, rect->y, rect->width, rect->height); 

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_tree_header_cell_paint(GdkDrawable* drawable, GdkRectangle* rect,
                               GdkRectangle* cliprect, GtkWidgetState* state,
                               gboolean isSorted, GtkTextDirection direction)
{
    gtk_tree_view_column_set_sort_indicator(gParts->middleTreeViewColumn,
                                            isSorted);

    moz_gtk_button_paint(drawable, rect, cliprect, state, GTK_RELIEF_NORMAL,
                         gParts->treeHeaderCellWidget, direction);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_tree_header_sort_arrow_paint(GdkDrawable* drawable, GdkRectangle* rect,
                                     GdkRectangle* cliprect,
                                     GtkWidgetState* state, GtkArrowType flags,
                                     GtkTextDirection direction)
{
    GdkRectangle arrow_rect;
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = GTK_SHADOW_IN;
    GtkArrowType arrow_type = flags;
    GtkStyle* style;

    ensure_tree_header_cell_widget();
    gtk_widget_set_direction(gParts->treeHeaderSortArrowWidget, direction);

    /* hard code these values */
    arrow_rect.width = 11;
    arrow_rect.height = 11;
    arrow_rect.x = rect->x + (rect->width - arrow_rect.width) / 2;
    arrow_rect.y = rect->y + (rect->height - arrow_rect.height) / 2;

    style = gParts->treeHeaderSortArrowWidget->style;
    TSOffsetStyleGCs(style, arrow_rect.x, arrow_rect.y);

    gtk_paint_arrow(style, drawable, state_type, shadow_type, cliprect,
                    gParts->treeHeaderSortArrowWidget, "arrow",  arrow_type, TRUE,
                    arrow_rect.x, arrow_rect.y,
                    arrow_rect.width, arrow_rect.height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_treeview_expander_paint(GdkDrawable* drawable, GdkRectangle* rect,
                                GdkRectangle* cliprect, GtkWidgetState* state,
                                GtkExpanderStyle expander_state,
                                GtkTextDirection direction)
{
    GtkStyle *style;
    GtkStateType state_type;

    ensure_tree_view_widget();
    gtk_widget_set_direction(gParts->treeViewWidget, direction);

    style = gParts->treeViewWidget->style;

    /* Because the frame we get is of the entire treeview, we can't get the precise
     * event state of one expander, thus rendering hover and active feedback useless. */
    state_type = state->disabled ? GTK_STATE_INSENSITIVE : GTK_STATE_NORMAL;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_expander(style, drawable, state_type, cliprect, gParts->treeViewWidget, "treeview",
                       rect->x + rect->width / 2, rect->y + rect->height / 2, expander_state);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_expander_paint(GdkDrawable* drawable, GdkRectangle* rect,
                       GdkRectangle* cliprect, GtkWidgetState* state,
                       GtkExpanderStyle expander_state,
                       GtkTextDirection direction)
{
    GtkStyle *style;
    GtkStateType state_type = ConvertGtkState(state);

    ensure_expander_widget();
    gtk_widget_set_direction(gParts->expanderWidget, direction);

    style = gParts->expanderWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_expander(style, drawable, state_type, cliprect, gParts->expanderWidget, "expander",
                       rect->x + rect->width / 2, rect->y + rect->height / 2, expander_state);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_combo_box_paint(GdkDrawable* drawable, GdkRectangle* rect,
                        GdkRectangle* cliprect, GtkWidgetState* state,
                        gboolean ishtml, GtkTextDirection direction)
{
    GdkRectangle arrow_rect, real_arrow_rect;
    gint /* arrow_size, */ separator_width;
    gboolean wide_separators;
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = state->active ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
    GtkStyle* style;
    GtkRequisition arrow_req;

    ensure_combo_box_widgets();

    /* Also sets the direction on gParts->comboBoxButtonWidget, which is then
     * inherited by the separator and arrow */
    moz_gtk_button_paint(drawable, rect, cliprect, state, GTK_RELIEF_NORMAL,
                         gParts->comboBoxButtonWidget, direction);

    calculate_button_inner_rect(gParts->comboBoxButtonWidget,
                                rect, &arrow_rect, direction, ishtml);
    /* Now arrow_rect contains the inner rect ; we want to correct the width
     * to what the arrow needs (see gtk_combo_box_size_allocate) */
    gtk_widget_size_request(gParts->comboBoxArrowWidget, &arrow_req);
    if (direction == GTK_TEXT_DIR_LTR)
        arrow_rect.x += arrow_rect.width - arrow_req.width;
    arrow_rect.width = arrow_req.width;

    calculate_arrow_rect(gParts->comboBoxArrowWidget,
                         &arrow_rect, &real_arrow_rect, direction);

    style = gParts->comboBoxArrowWidget->style;
    TSOffsetStyleGCs(style, rect->x, rect->y);

    gtk_widget_size_allocate(gParts->comboBoxWidget, rect);

    gtk_paint_arrow(style, drawable, state_type, shadow_type, cliprect,
                    gParts->comboBoxArrowWidget, "arrow",  GTK_ARROW_DOWN, TRUE,
                    real_arrow_rect.x, real_arrow_rect.y,
                    real_arrow_rect.width, real_arrow_rect.height);


    /* If there is no separator in the theme, there's nothing left to do. */
    if (!gParts->comboBoxSeparatorWidget)
        return MOZ_GTK_SUCCESS;

    style = gParts->comboBoxSeparatorWidget->style;
    TSOffsetStyleGCs(style, rect->x, rect->y);

    gtk_widget_style_get(gParts->comboBoxSeparatorWidget,
                         "wide-separators", &wide_separators,
                         "separator-width", &separator_width,
                         NULL);

    if (wide_separators) {
        if (direction == GTK_TEXT_DIR_LTR)
            arrow_rect.x -= separator_width;
        else
            arrow_rect.x += arrow_rect.width;

        gtk_paint_box(style, drawable,
                      GTK_STATE_NORMAL, GTK_SHADOW_ETCHED_OUT,
                      cliprect, gParts->comboBoxSeparatorWidget, "vseparator",
                      arrow_rect.x, arrow_rect.y,
                      separator_width, arrow_rect.height);
    } else {
        if (direction == GTK_TEXT_DIR_LTR)
            arrow_rect.x -= XTHICKNESS(style);
        else
            arrow_rect.x += arrow_rect.width;

        gtk_paint_vline(style, drawable, GTK_STATE_NORMAL, cliprect,
                        gParts->comboBoxSeparatorWidget, "vseparator",
                        arrow_rect.y, arrow_rect.y + arrow_rect.height,
                        arrow_rect.x);
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_downarrow_paint(GdkDrawable* drawable, GdkRectangle* rect,
                        GdkRectangle* cliprect, GtkWidgetState* state)
{
    GtkStyle* style;
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = state->active ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
    GdkRectangle arrow_rect;

    ensure_button_arrow_widget();
    style = gParts->buttonArrowWidget->style;

    calculate_arrow_rect(gParts->buttonArrowWidget, rect, &arrow_rect,
                         GTK_TEXT_DIR_LTR);

    TSOffsetStyleGCs(style, arrow_rect.x, arrow_rect.y);
    gtk_paint_arrow(style, drawable, state_type, shadow_type, cliprect,
                    gParts->buttonArrowWidget, "arrow",  GTK_ARROW_DOWN, TRUE,
                    arrow_rect.x, arrow_rect.y, arrow_rect.width, arrow_rect.height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_combo_box_entry_button_paint(GdkDrawable* drawable, GdkRectangle* rect,
                                     GdkRectangle* cliprect,
                                     GtkWidgetState* state,
                                     gboolean input_focus,
                                     GtkTextDirection direction)
{
    gint x_displacement, y_displacement;
    GdkRectangle arrow_rect, real_arrow_rect;
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = state->active ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
    GtkStyle* style;

    ensure_combo_box_entry_widgets();

    if (input_focus) {
        /* Some themes draw a complementary focus ring for the dropdown button
         * when the dropdown entry has focus */
        GTK_WIDGET_SET_FLAGS(gParts->comboBoxEntryTextareaWidget, GTK_HAS_FOCUS);
    }

    moz_gtk_button_paint(drawable, rect, cliprect, state, GTK_RELIEF_NORMAL,
                         gParts->comboBoxEntryButtonWidget, direction);

    if (input_focus)
        GTK_WIDGET_UNSET_FLAGS(gParts->comboBoxEntryTextareaWidget, GTK_HAS_FOCUS);

    calculate_button_inner_rect(gParts->comboBoxEntryButtonWidget,
                                rect, &arrow_rect, direction, FALSE);
    if (state_type == GTK_STATE_ACTIVE) {
        gtk_widget_style_get(gParts->comboBoxEntryButtonWidget,
                             "child-displacement-x", &x_displacement,
                             "child-displacement-y", &y_displacement,
                             NULL);
        arrow_rect.x += x_displacement;
        arrow_rect.y += y_displacement;
    }

    calculate_arrow_rect(gParts->comboBoxEntryArrowWidget,
                         &arrow_rect, &real_arrow_rect, direction);

    style = gParts->comboBoxEntryArrowWidget->style;
    TSOffsetStyleGCs(style, real_arrow_rect.x, real_arrow_rect.y);

    gtk_paint_arrow(style, drawable, state_type, shadow_type, cliprect,
                    gParts->comboBoxEntryArrowWidget, "arrow",  GTK_ARROW_DOWN, TRUE,
                    real_arrow_rect.x, real_arrow_rect.y,
                    real_arrow_rect.width, real_arrow_rect.height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_container_paint(GdkDrawable* drawable, GdkRectangle* rect,
                        GdkRectangle* cliprect, GtkWidgetState* state, 
                        gboolean isradio, GtkTextDirection direction)
{
    GtkStateType state_type = ConvertGtkState(state);
    GtkStyle* style;
    GtkWidget *widget;
    gboolean interior_focus;
    gint focus_width, focus_pad;

    if (isradio) {
        ensure_radiobutton_widget();
        widget = gParts->radiobuttonWidget;
    } else {
        ensure_checkbox_widget();
        widget = gParts->checkboxWidget;
    }
    gtk_widget_set_direction(widget, direction);

    style = widget->style;
    moz_gtk_widget_get_focus(widget, &interior_focus, &focus_width,
                             &focus_pad);

    TSOffsetStyleGCs(style, rect->x, rect->y);

    /* The detail argument for the gtk_paint_* calls below are "checkbutton"
       even for radio buttons, to match what gtk does. */

    /* this is for drawing a prelight box */
    if (state_type == GTK_STATE_PRELIGHT || state_type == GTK_STATE_ACTIVE) {
        gtk_paint_flat_box(style, drawable, GTK_STATE_PRELIGHT,
                           GTK_SHADOW_ETCHED_OUT, cliprect, widget,
                           "checkbutton",
                           rect->x, rect->y, rect->width, rect->height);
    }

    if (state_type != GTK_STATE_NORMAL && state_type != GTK_STATE_PRELIGHT)
        state_type = GTK_STATE_NORMAL;

    if (state->focused && !interior_focus) {
        gtk_paint_focus(style, drawable, state_type, cliprect, widget,
                        "checkbutton",
                        rect->x, rect->y, rect->width, rect->height);
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_toggle_label_paint(GdkDrawable* drawable, GdkRectangle* rect,
                           GdkRectangle* cliprect, GtkWidgetState* state, 
                           gboolean isradio, GtkTextDirection direction)
{
    GtkStateType state_type;
    GtkStyle *style;
    GtkWidget *widget;
    gboolean interior_focus;

    if (!state->focused)
        return MOZ_GTK_SUCCESS;

    if (isradio) {
        ensure_radiobutton_widget();
        widget = gParts->radiobuttonWidget;
    } else {
        ensure_checkbox_widget();
        widget = gParts->checkboxWidget;
    }
    gtk_widget_set_direction(widget, direction);

    gtk_widget_style_get(widget, "interior-focus", &interior_focus, NULL);
    if (!interior_focus)
        return MOZ_GTK_SUCCESS;

    state_type = ConvertGtkState(state);

    style = widget->style;
    TSOffsetStyleGCs(style, rect->x, rect->y);

    /* Always "checkbutton" to match gtkcheckbutton.c */
    gtk_paint_focus(style, drawable, state_type, cliprect, widget,
                    "checkbutton",
                    rect->x, rect->y, rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_toolbar_paint(GdkDrawable* drawable, GdkRectangle* rect,
                      GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;
    GtkShadowType shadow_type;

    ensure_toolbar_widget();
    gtk_widget_set_direction(gParts->toolbarWidget, direction);

    style = gParts->toolbarWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);

    gtk_style_apply_default_background(style, drawable, TRUE,
                                       GTK_STATE_NORMAL,
                                       cliprect, rect->x, rect->y,
                                       rect->width, rect->height);

    gtk_widget_style_get(gParts->toolbarWidget, "shadow-type", &shadow_type, NULL);

    gtk_paint_box (style, drawable, GTK_STATE_NORMAL, shadow_type,
                   cliprect, gParts->toolbarWidget, "toolbar",
                   rect->x, rect->y, rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_toolbar_separator_paint(GdkDrawable* drawable, GdkRectangle* rect,
                                GdkRectangle* cliprect,
                                GtkTextDirection direction)
{
    GtkStyle* style;
    gint     separator_width;
    gint     paint_width;
    gboolean wide_separators;
    
    /* Defined as constants in GTK+ 2.10.14 */
    const double start_fraction = 0.2;
    const double end_fraction = 0.8;

    ensure_toolbar_separator_widget();
    gtk_widget_set_direction(gParts->toolbarSeparatorWidget, direction);

    style = gParts->toolbarSeparatorWidget->style;

    gtk_widget_style_get(gParts->toolbarWidget,
                         "wide-separators", &wide_separators,
                         "separator-width", &separator_width,
                         NULL);

    TSOffsetStyleGCs(style, rect->x, rect->y);

    if (wide_separators) {
        if (separator_width > rect->width)
            separator_width = rect->width;

        gtk_paint_box(style, drawable,
                      GTK_STATE_NORMAL, GTK_SHADOW_ETCHED_OUT,
                      cliprect, gParts->toolbarWidget, "vseparator",
                      rect->x + (rect->width - separator_width) / 2,
                      rect->y + rect->height * start_fraction,
                      separator_width,
                      rect->height * (end_fraction - start_fraction));
                       
    } else {
        paint_width = style->xthickness;
        
        if (paint_width > rect->width)
            paint_width = rect->width;
    
        gtk_paint_vline(style, drawable,
                        GTK_STATE_NORMAL, cliprect, gParts->toolbarSeparatorWidget,
                        "toolbar",
                        rect->y + rect->height * start_fraction,
                        rect->y + rect->height * end_fraction,
                        rect->x + (rect->width - paint_width) / 2);
    }
    
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_tooltip_paint(GdkDrawable* drawable, GdkRectangle* rect,
                      GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;

    ensure_tooltip_widget();
    gtk_widget_set_direction(gParts->tooltipWidget, direction);

    style = gtk_rc_get_style_by_paths(gtk_settings_get_default(),
                                      "gtk-tooltips", "GtkWindow",
                                      GTK_TYPE_WINDOW);

    style = gtk_style_attach(style, gParts->tooltipWidget->window);
    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_flat_box(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                       cliprect, gParts->tooltipWidget, "tooltip",
                       rect->x, rect->y, rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_resizer_paint(GdkDrawable* drawable, GdkRectangle* rect,
                      GdkRectangle* cliprect, GtkWidgetState* state,
                      GtkTextDirection direction)
{
    GtkStyle* style;
    GtkStateType state_type = ConvertGtkState(state);

    ensure_window_widget();
    gtk_widget_set_direction(gParts->protoWindow, direction);

    style = gParts->protoWindow->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);

    gtk_paint_resize_grip(style, drawable, state_type, cliprect, gParts->protoWindow,
                          NULL, (direction == GTK_TEXT_DIR_LTR) ?
                          GDK_WINDOW_EDGE_SOUTH_EAST :
                          GDK_WINDOW_EDGE_SOUTH_WEST,
                          rect->x, rect->y, rect->width, rect->height);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_frame_paint(GdkDrawable* drawable, GdkRectangle* rect,
                    GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;
    GtkShadowType shadow_type;

    ensure_frame_widget();
    gtk_widget_set_direction(gParts->frameWidget, direction);

    style = gParts->frameWidget->style;

    gtk_widget_style_get(gParts->statusbarWidget, "shadow-type", &shadow_type, NULL);

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_shadow(style, drawable, GTK_STATE_NORMAL, shadow_type,
                     cliprect, gParts->frameWidget, "frame", rect->x, rect->y,
                     rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_progressbar_paint(GdkDrawable* drawable, GdkRectangle* rect,
                          GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;

    ensure_progress_widget();
    gtk_widget_set_direction(gParts->progresWidget, direction);

    style = gParts->progresWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_IN,
                  cliprect, gParts->progresWidget, "trough", rect->x, rect->y,
                  rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_progress_chunk_paint(GdkDrawable* drawable, GdkRectangle* rect,
                             GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;

    ensure_progress_widget();
    gtk_widget_set_direction(gParts->progresWidget, direction);

    style = gParts->progresWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box(style, drawable, GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
                  cliprect, gParts->progresWidget, "bar", rect->x, rect->y,
                  rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_tab_thickness(void)
{
    ensure_tab_widget();
    if (YTHICKNESS(gParts->tabWidget->style) < 2)
        return 2; /* some themes don't set ythickness correctly */

    return YTHICKNESS(gParts->tabWidget->style);
}

static gint
moz_gtk_tab_paint(GdkDrawable* drawable, GdkRectangle* rect,
                  GdkRectangle* cliprect, GtkTabFlags flags,
                  GtkTextDirection direction)
{
    /* When the tab isn't selected, we just draw a notebook extension.
     * When it is selected, we overwrite the adjacent border of the tabpanel
     * touching the tab with a pierced border (called "the gap") to make the
     * tab appear physically attached to the tabpanel; see details below. */

    GtkStyle* style;

    ensure_tab_widget();
    gtk_widget_set_direction(gParts->tabWidget, direction);

    style = gParts->tabWidget->style;
    TSOffsetStyleGCs(style, rect->x, rect->y);

    if ((flags & MOZ_GTK_TAB_SELECTED) == 0) {
        /* Only draw the tab */
        gtk_paint_extension(style, drawable, GTK_STATE_ACTIVE, GTK_SHADOW_OUT,
                            cliprect, gParts->tabWidget, "tab",
                            rect->x, rect->y, rect->width, rect->height,
                            (flags & MOZ_GTK_TAB_BOTTOM) ?
                                GTK_POS_TOP : GTK_POS_BOTTOM );
    } else {
        /* Draw the tab and the gap
         * We want the gap to be positionned exactly on the tabpanel top
         * border; since tabbox.css may set a negative margin so that the tab
         * frame rect already overlaps the tabpanel frame rect, we need to take
         * that into account when drawing. To that effect, nsNativeThemeGTK
         * passes us this negative margin (bmargin in the graphic below) in the
         * lowest bits of |flags|.  We use it to set gap_voffset, the distance
         * between the top of the gap and the bottom of the tab (resp. the
         * bottom of the gap and the top of the tab when we draw a bottom tab),
         * while ensuring that the gap always touches the border of the tab,
         * i.e. 0 <= gap_voffset <= gap_height, to avoid surprinsing results
         * with big negative or positive margins.
         * Here is a graphical explanation in the case of top tabs:
         *             ___________________________
         *            /                           \
         *           |            T A B            |
         * ----------|. . . . . . . . . . . . . . .|----- top of tabpanel
         *           :    ^       bmargin          :  ^
         *           :    | (-negative margin,     :  |
         *  bottom   :    v  passed in flags)      :  |       gap_height
         *    of  -> :.............................:  |    (the size of the
         * the tab   .       part of the gap       .  |  tabpanel top border)
         *           .      outside of the tab     .  v
         * ----------------------------------------------
         *
         * To draw the gap, we use gtk_paint_box_gap(), see comment in
         * moz_gtk_tabpanels_paint(). This box_gap is made 3 * gap_height tall,
         * which should suffice to ensure that the only visible border is the
         * pierced one.  If the tab is in the middle, we make the box_gap begin
         * a bit to the left of the tab and end a bit to the right, adjusting
         * the gap position so it still is under the tab, because we want the
         * rendering of a gap in the middle of a tabpanel.  This is the role of
         * the gints gap_{l,r}_offset. On the contrary, if the tab is the
         * first, we align the start border of the box_gap with the start
         * border of the tab (left if LTR, right if RTL), by setting the
         * appropriate offset to 0.*/
        gint gap_loffset, gap_roffset, gap_voffset, gap_height;

        /* Get height needed by the gap */
        gap_height = moz_gtk_get_tab_thickness();

        /* Extract gap_voffset from the first bits of flags */
        gap_voffset = flags & MOZ_GTK_TAB_MARGIN_MASK;
        if (gap_voffset > gap_height)
            gap_voffset = gap_height;

        /* Set gap_{l,r}_offset to appropriate values */
        gap_loffset = gap_roffset = 20; /* should be enough */
        if (flags & MOZ_GTK_TAB_FIRST) {
            if (direction == GTK_TEXT_DIR_RTL)
                gap_roffset = 0;
            else
                gap_loffset = 0;
        }

        if (flags & MOZ_GTK_TAB_BOTTOM) {
            /* Enlarge the cliprect to have room for the full gap height */
            cliprect->height += gap_height - gap_voffset;
            cliprect->y -= gap_height - gap_voffset;

            /* Draw the tab */
            gtk_paint_extension(style, drawable, GTK_STATE_NORMAL,
                                GTK_SHADOW_OUT, cliprect, gParts->tabWidget, "tab",
                                rect->x, rect->y + gap_voffset, rect->width,
                                rect->height - gap_voffset, GTK_POS_TOP);

            /* Draw the gap; erase with background color before painting in
             * case theme does not */
            gtk_style_apply_default_background(style, drawable, TRUE,
                                               GTK_STATE_NORMAL, cliprect,
                                               rect->x,
                                               rect->y + gap_voffset
                                                       - gap_height,
                                               rect->width, gap_height);
            gtk_paint_box_gap(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                              cliprect, gParts->tabWidget, "notebook",
                              rect->x - gap_loffset,
                              rect->y + gap_voffset - 3 * gap_height,
                              rect->width + gap_loffset + gap_roffset,
                              3 * gap_height, GTK_POS_BOTTOM,
                              gap_loffset, rect->width);
        } else {
            /* Enlarge the cliprect to have room for the full gap height */
            cliprect->height += gap_height - gap_voffset;

            /* Draw the tab */
            gtk_paint_extension(style, drawable, GTK_STATE_NORMAL,
                                GTK_SHADOW_OUT, cliprect, gParts->tabWidget, "tab",
                                rect->x, rect->y, rect->width,
                                rect->height - gap_voffset, GTK_POS_BOTTOM);

            /* Draw the gap; erase with background color before painting in
             * case theme does not */
            gtk_style_apply_default_background(style, drawable, TRUE,
                                               GTK_STATE_NORMAL, cliprect,
                                               rect->x,
                                               rect->y + rect->height
                                                       - gap_voffset,
                                               rect->width, gap_height);
            gtk_paint_box_gap(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                              cliprect, gParts->tabWidget, "notebook",
                              rect->x - gap_loffset,
                              rect->y + rect->height - gap_voffset,
                              rect->width + gap_loffset + gap_roffset,
                              3 * gap_height, GTK_POS_TOP,
                              gap_loffset, rect->width);
        }

    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_tabpanels_paint(GdkDrawable* drawable, GdkRectangle* rect,
                        GdkRectangle* cliprect, GtkTextDirection direction)
{
    /* We use gtk_paint_box_gap() to draw the tabpanels widget. gtk_paint_box()
     * draws an all-purpose box, which a lot of themes render differently.
     * A zero-width gap is still visible in most themes, so we hide it to the
     * left (10px should be enough) */
    GtkStyle* style;

    ensure_tab_widget();
    gtk_widget_set_direction(gParts->tabWidget, direction);

    style = gParts->tabWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box_gap(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                      cliprect, gParts->tabWidget, "notebook", rect->x, rect->y,
                      rect->width, rect->height,
                      GTK_POS_TOP, -10, 0);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_tab_scroll_arrow_paint(GdkDrawable* drawable, GdkRectangle* rect,
                               GdkRectangle* cliprect, GtkWidgetState* state,
                               GtkArrowType arrow_type,
                               GtkTextDirection direction)
{
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = state->active ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
    GtkStyle* style;
    gint arrow_size = MIN(rect->width, rect->height);
    gint x = rect->x + (rect->width - arrow_size) / 2;
    gint y = rect->y + (rect->height - arrow_size) / 2;

    ensure_tab_widget();

    style = gParts->tabWidget->style;
    TSOffsetStyleGCs(style, rect->x, rect->y);

    if (direction == GTK_TEXT_DIR_RTL) {
        arrow_type = (arrow_type == GTK_ARROW_LEFT) ?
                         GTK_ARROW_RIGHT : GTK_ARROW_LEFT;
    }

    gtk_paint_arrow(style, drawable, state_type, shadow_type, NULL,
                    gParts->tabWidget, "notebook", arrow_type, TRUE,
                    x, y, arrow_size, arrow_size);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_menu_bar_paint(GdkDrawable* drawable, GdkRectangle* rect,
                       GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;
    GtkShadowType shadow_type;
    ensure_menu_bar_widget();
    gtk_widget_set_direction(gParts->menuBarWidget, direction);

    gtk_widget_style_get(gParts->menuBarWidget, "shadow-type", &shadow_type, NULL);

    style = gParts->menuBarWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_style_apply_default_background(style, drawable, TRUE, GTK_STATE_NORMAL,
                                       cliprect, rect->x, rect->y,
                                       rect->width, rect->height);

    gtk_paint_box(style, drawable, GTK_STATE_NORMAL, shadow_type,
                  cliprect, gParts->menuBarWidget, "menubar", rect->x, rect->y,
                  rect->width, rect->height);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_menu_popup_paint(GdkDrawable* drawable, GdkRectangle* rect,
                         GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;
    ensure_menu_popup_widget();
    gtk_widget_set_direction(gParts->menuPopupWidget, direction);

    style = gParts->menuPopupWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_style_apply_default_background(style, drawable, TRUE, GTK_STATE_NORMAL,
                                       cliprect, rect->x, rect->y,
                                       rect->width, rect->height);
    gtk_paint_box(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_OUT, 
                  cliprect, gParts->menuPopupWidget, "menu",
                  rect->x, rect->y, rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_menu_separator_paint(GdkDrawable* drawable, GdkRectangle* rect,
                             GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;
    gboolean wide_separators;
    gint separator_height;
    guint horizontal_padding;
    gint paint_height;

    ensure_menu_separator_widget();
    gtk_widget_set_direction(gParts->menuSeparatorWidget, direction);

    style = gParts->menuSeparatorWidget->style;

    gtk_widget_style_get(gParts->menuSeparatorWidget,
                         "wide-separators",    &wide_separators,
                         "separator-height",   &separator_height,
                         "horizontal-padding", &horizontal_padding,
                         NULL);

    TSOffsetStyleGCs(style, rect->x, rect->y);

    if (wide_separators) {
        if (separator_height > rect->height)
            separator_height = rect->height;

        gtk_paint_box(style, drawable,
                      GTK_STATE_NORMAL, GTK_SHADOW_ETCHED_OUT,
                      cliprect, gParts->menuSeparatorWidget, "hseparator",
                      rect->x + horizontal_padding + style->xthickness,
                      rect->y + (rect->height - separator_height - style->ythickness) / 2,
                      rect->width - 2 * (horizontal_padding + style->xthickness),
                      separator_height);
    } else {
        paint_height = style->ythickness;
        if (paint_height > rect->height)
            paint_height = rect->height;

        gtk_paint_hline(style, drawable,
                        GTK_STATE_NORMAL, cliprect, gParts->menuSeparatorWidget,
                        "menuitem",
                        rect->x + horizontal_padding + style->xthickness,
                        rect->x + rect->width - horizontal_padding - style->xthickness - 1,
                        rect->y + (rect->height - style->ythickness) / 2);
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_menu_item_paint(GdkDrawable* drawable, GdkRectangle* rect,
                        GdkRectangle* cliprect, GtkWidgetState* state,
                        gint flags, GtkTextDirection direction)
{
    GtkStyle* style;
    GtkShadowType shadow_type;
    GtkWidget* item_widget;

    if (state->inHover && !state->disabled) {
        if (flags & MOZ_TOPLEVEL_MENU_ITEM) {
            ensure_menu_bar_item_widget();
            item_widget = gParts->menuBarItemWidget;
        } else {
            ensure_menu_item_widget();
            item_widget = gParts->menuItemWidget;
        }
        gtk_widget_set_direction(item_widget, direction);
        
        style = item_widget->style;
        TSOffsetStyleGCs(style, rect->x, rect->y);

        gtk_widget_style_get(item_widget, "selected-shadow-type",
                             &shadow_type, NULL);

        gtk_paint_box(style, drawable, GTK_STATE_PRELIGHT, shadow_type,
                      cliprect, item_widget, "menuitem", rect->x, rect->y,
                      rect->width, rect->height);
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_menu_arrow_paint(GdkDrawable* drawable, GdkRectangle* rect,
                         GdkRectangle* cliprect, GtkWidgetState* state,
                         GtkTextDirection direction)
{
    GtkStyle* style;
    GtkStateType state_type = ConvertGtkState(state);

    ensure_menu_item_widget();
    gtk_widget_set_direction(gParts->menuItemWidget, direction);

    style = gParts->menuItemWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_arrow(style, drawable, state_type,
                    (state_type == GTK_STATE_PRELIGHT) ? GTK_SHADOW_IN : GTK_SHADOW_OUT,
                    cliprect, gParts->menuItemWidget, "menuitem",
                    (direction == GTK_TEXT_DIR_LTR) ? GTK_ARROW_RIGHT : GTK_ARROW_LEFT,
                    TRUE, rect->x, rect->y, rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_check_menu_item_paint(GdkDrawable* drawable, GdkRectangle* rect,
                              GdkRectangle* cliprect, GtkWidgetState* state,
                              gboolean checked, gboolean isradio,
                              GtkTextDirection direction)
{
    GtkStateType state_type = ConvertGtkState(state);
    GtkStyle* style;
    GtkShadowType shadow_type = (checked)?GTK_SHADOW_IN:GTK_SHADOW_OUT;
    gint offset;
    gint indicator_size;
    gint x, y;

    moz_gtk_menu_item_paint(drawable, rect, cliprect, state, FALSE, direction);

    ensure_check_menu_item_widget();
    gtk_widget_set_direction(gParts->checkMenuItemWidget, direction);

    gtk_widget_style_get (gParts->checkMenuItemWidget,
                          "indicator-size", &indicator_size,
                          NULL);

    if (checked || GTK_CHECK_MENU_ITEM(gParts->checkMenuItemWidget)->always_show_toggle) {
      style = gParts->checkMenuItemWidget->style;

      offset = GTK_CONTAINER(gParts->checkMenuItemWidget)->border_width +
             gParts->checkMenuItemWidget->style->xthickness + 2;

      /* while normally this "3" would be the horizontal-padding style value, passing it to Gecko
         as the value of menuitem padding causes problems with dropdowns (bug 406129), so in the menu.css
         file this is hardcoded as 3px. Yes it sucks, but we don't really have a choice. */
      x = (direction == GTK_TEXT_DIR_RTL) ?
            rect->width - indicator_size - offset - 3: rect->x + offset + 3;
      y = rect->y + (rect->height - indicator_size) / 2;

      TSOffsetStyleGCs(style, x, y);
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gParts->checkMenuItemWidget),
                                     checked);

      if (isradio) {
        gtk_paint_option(style, drawable, state_type, shadow_type, cliprect,
                         gParts->checkMenuItemWidget, "option",
                         x, y, indicator_size, indicator_size);
      } else {
        gtk_paint_check(style, drawable, state_type, shadow_type, cliprect,
                        gParts->checkMenuItemWidget, "check",
                        x, y, indicator_size, indicator_size);
      }
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_window_paint(GdkDrawable* drawable, GdkRectangle* rect,
                     GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;

    ensure_window_widget();
    gtk_widget_set_direction(gParts->protoWindow, direction);

    style = gParts->protoWindow->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_style_apply_default_background(style, drawable, TRUE,
                                       GTK_STATE_NORMAL,
                                       cliprect, rect->x, rect->y,
                                       rect->width, rect->height);
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_widget_border(GtkThemeWidgetType widget, gint* left, gint* top,
                          gint* right, gint* bottom, GtkTextDirection direction,
                          gboolean inhtml)
{
    GtkWidget* w;

    switch (widget) {
    case MOZ_GTK_BUTTON:
        {
            GtkBorder inner_border;
            gboolean interior_focus;
            gint focus_width, focus_pad;

            ensure_button_widget();
            *left = *top = *right = *bottom = GTK_CONTAINER(gParts->buttonWidget)->border_width;

            /* Don't add this padding in HTML, otherwise the buttons will
               become too big and stuff the layout. */
            if (!inhtml) {
                moz_gtk_widget_get_focus(gParts->buttonWidget, &interior_focus, &focus_width, &focus_pad);
                moz_gtk_button_get_inner_border(gParts->buttonWidget, &inner_border);
                *left += focus_width + focus_pad + inner_border.left;
                *right += focus_width + focus_pad + inner_border.right;
                *top += focus_width + focus_pad + inner_border.top;
                *bottom += focus_width + focus_pad + inner_border.bottom;
            }

            *left += gParts->buttonWidget->style->xthickness;
            *right += gParts->buttonWidget->style->xthickness;
            *top += gParts->buttonWidget->style->ythickness;
            *bottom += gParts->buttonWidget->style->ythickness;
            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_ENTRY:
        ensure_entry_widget();
        w = gParts->entryWidget;
        break;
    case MOZ_GTK_TREEVIEW:
        ensure_tree_view_widget();
        w = gParts->treeViewWidget;
        break;
    case MOZ_GTK_TREE_HEADER_CELL:
        {
            /* A Tree Header in GTK is just a different styled button 
             * It must be placed in a TreeView for getting the correct style
             * assigned.
             * That is why the following code is the same as for MOZ_GTK_BUTTON.  
             * */

            GtkBorder inner_border;
            gboolean interior_focus;
            gint focus_width, focus_pad;

            ensure_tree_header_cell_widget();
            *left = *top = *right = *bottom = GTK_CONTAINER(gParts->treeHeaderCellWidget)->border_width;

            moz_gtk_widget_get_focus(gParts->treeHeaderCellWidget, &interior_focus, &focus_width, &focus_pad);
            moz_gtk_button_get_inner_border(gParts->treeHeaderCellWidget, &inner_border);
            *left += focus_width + focus_pad + inner_border.left;
            *right += focus_width + focus_pad + inner_border.right;
            *top += focus_width + focus_pad + inner_border.top;
            *bottom += focus_width + focus_pad + inner_border.bottom;
            
            *left += gParts->treeHeaderCellWidget->style->xthickness;
            *right += gParts->treeHeaderCellWidget->style->xthickness;
            *top += gParts->treeHeaderCellWidget->style->ythickness;
            *bottom += gParts->treeHeaderCellWidget->style->ythickness;
            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_TREE_HEADER_SORTARROW:
        ensure_tree_header_cell_widget();
        w = gParts->treeHeaderSortArrowWidget;
        break;
    case MOZ_GTK_DROPDOWN_ENTRY:
        ensure_combo_box_entry_widgets();
        w = gParts->comboBoxEntryTextareaWidget;
        break;
    case MOZ_GTK_DROPDOWN_ARROW:
        ensure_combo_box_entry_widgets();
        w = gParts->comboBoxEntryButtonWidget;
        break;
    case MOZ_GTK_DROPDOWN:
        {
            /* We need to account for the arrow on the dropdown, so text
             * doesn't come too close to the arrow, or in some cases spill
             * into the arrow. */
            gboolean ignored_interior_focus, wide_separators;
            gint focus_width, focus_pad, separator_width;
            GtkRequisition arrow_req;

            ensure_combo_box_widgets();

            *left = GTK_CONTAINER(gParts->comboBoxButtonWidget)->border_width;

            if (!inhtml) {
                moz_gtk_widget_get_focus(gParts->comboBoxButtonWidget,
                                         &ignored_interior_focus,
                                         &focus_width, &focus_pad);
                *left += focus_width + focus_pad;
            }

            *top = *left + gParts->comboBoxButtonWidget->style->ythickness;
            *left += gParts->comboBoxButtonWidget->style->xthickness;

            *right = *left; *bottom = *top;

            /* If there is no separator, don't try to count its width. */
            separator_width = 0;
            if (gParts->comboBoxSeparatorWidget) {
                gtk_widget_style_get(gParts->comboBoxSeparatorWidget,
                                     "wide-separators", &wide_separators,
                                     "separator-width", &separator_width,
                                     NULL);

                if (!wide_separators)
                    separator_width =
                        XTHICKNESS(gParts->comboBoxSeparatorWidget->style);
            }

            gtk_widget_size_request(gParts->comboBoxArrowWidget, &arrow_req);

            if (direction == GTK_TEXT_DIR_RTL)
                *left += separator_width + arrow_req.width;
            else
                *right += separator_width + arrow_req.width;

            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_TABPANELS:
        ensure_tab_widget();
        w = gParts->tabWidget;
        break;
    case MOZ_GTK_PROGRESSBAR:
        ensure_progress_widget();
        w = gParts->progresWidget;
        break;
    case MOZ_GTK_SPINBUTTON_ENTRY:
    case MOZ_GTK_SPINBUTTON_UP:
    case MOZ_GTK_SPINBUTTON_DOWN:
        ensure_spin_widget();
        w = gParts->spinWidget;
        break;
    case MOZ_GTK_SCALE_HORIZONTAL:
        ensure_scale_widget();
        w = gParts->hScaleWidget;
        break;
    case MOZ_GTK_SCALE_VERTICAL:
        ensure_scale_widget();
        w = gParts->vScaleWidget;
        break;
    case MOZ_GTK_FRAME:
        ensure_frame_widget();
        w = gParts->frameWidget;
        break;
    case MOZ_GTK_CHECKBUTTON_LABEL:
    case MOZ_GTK_RADIOBUTTON_LABEL:
        {
            gboolean interior_focus;
            gint focus_width, focus_pad;

            /* If the focus is interior, then the label has a border of
               (focus_width + focus_pad). */
            if (widget == MOZ_GTK_CHECKBUTTON_LABEL) {
                ensure_checkbox_widget();
                moz_gtk_widget_get_focus(gParts->checkboxWidget, &interior_focus,
                                           &focus_width, &focus_pad);
            }
            else {
                ensure_radiobutton_widget();
                moz_gtk_widget_get_focus(gParts->radiobuttonWidget, &interior_focus,
                                        &focus_width, &focus_pad);
            }

            if (interior_focus)
                *left = *top = *right = *bottom = (focus_width + focus_pad);
            else
                *left = *top = *right = *bottom = 0;

            return MOZ_GTK_SUCCESS;
        }

    case MOZ_GTK_CHECKBUTTON_CONTAINER:
    case MOZ_GTK_RADIOBUTTON_CONTAINER:
        {
            gboolean interior_focus;
            gint focus_width, focus_pad;

            /* If the focus is _not_ interior, then the container has a border
               of (focus_width + focus_pad). */
            if (widget == MOZ_GTK_CHECKBUTTON_CONTAINER) {
                ensure_checkbox_widget();
                moz_gtk_widget_get_focus(gParts->checkboxWidget, &interior_focus,
                                           &focus_width, &focus_pad);
                w = gParts->checkboxWidget;
            } else {
                ensure_radiobutton_widget();
                moz_gtk_widget_get_focus(gParts->radiobuttonWidget, &interior_focus,
                                        &focus_width, &focus_pad);
                w = gParts->radiobuttonWidget;
            }

            *left = *top = *right = *bottom = GTK_CONTAINER(w)->border_width;

            if (!interior_focus) {
                *left += (focus_width + focus_pad);
                *right += (focus_width + focus_pad);
                *top += (focus_width + focus_pad);
                *bottom += (focus_width + focus_pad);
            }

            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_MENUPOPUP:
        ensure_menu_popup_widget();
        w = gParts->menuPopupWidget;
        break;
    case MOZ_GTK_MENUITEM:
        ensure_menu_item_widget();
        ensure_menu_bar_item_widget();
        w = gParts->menuItemWidget;
        break;
    case MOZ_GTK_CHECKMENUITEM:
    case MOZ_GTK_RADIOMENUITEM:
        ensure_check_menu_item_widget();
        w = gParts->checkMenuItemWidget;
        break;
    case MOZ_GTK_TAB:
        ensure_tab_widget();
        w = gParts->tabWidget;
        break;
    /* These widgets have no borders, since they are not containers. */
    case MOZ_GTK_SPLITTER_HORIZONTAL:
    case MOZ_GTK_SPLITTER_VERTICAL:
    case MOZ_GTK_CHECKBUTTON:
    case MOZ_GTK_RADIOBUTTON:
    case MOZ_GTK_SCROLLBAR_BUTTON:
    case MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_TRACK_VERTICAL:
    case MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:
    case MOZ_GTK_SCALE_THUMB_HORIZONTAL:
    case MOZ_GTK_SCALE_THUMB_VERTICAL:
    case MOZ_GTK_GRIPPER:
    case MOZ_GTK_PROGRESS_CHUNK:
    case MOZ_GTK_EXPANDER:
    case MOZ_GTK_TREEVIEW_EXPANDER:
    case MOZ_GTK_TOOLBAR_SEPARATOR:
    case MOZ_GTK_MENUSEPARATOR:
    /* These widgets have no borders.*/
    case MOZ_GTK_SPINBUTTON:
    case MOZ_GTK_TOOLTIP:
    case MOZ_GTK_WINDOW:
    case MOZ_GTK_RESIZER:
    case MOZ_GTK_MENUARROW:
    case MOZ_GTK_TOOLBARBUTTON_ARROW:
    case MOZ_GTK_TOOLBAR:
    case MOZ_GTK_MENUBAR:
    case MOZ_GTK_TAB_SCROLLARROW:
    case MOZ_GTK_ENTRY_CARET:
        *left = *top = *right = *bottom = 0;
        return MOZ_GTK_SUCCESS;
    default:
        g_warning("Unsupported widget type: %d", widget);
        return MOZ_GTK_UNKNOWN_WIDGET;
    }

    *right = *left = XTHICKNESS(w->style);
    *bottom = *top = YTHICKNESS(w->style);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_combo_box_entry_button_size(gint* width, gint* height)
{
    /*
     * We get the requisition of the drop down button, which includes
     * all padding, border and focus line widths the button uses,
     * as well as the minimum arrow size and its padding
     * */
    GtkRequisition requisition;
    ensure_combo_box_entry_widgets();

    gtk_widget_size_request(gParts->comboBoxEntryButtonWidget, &requisition);
    *width = requisition.width;
    *height = requisition.height;

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_tab_scroll_arrow_size(gint* width, gint* height)
{
    gint arrow_size;

    ensure_tab_widget();
    gtk_widget_style_get(gParts->tabWidget,
                         "scroll-arrow-hlength", &arrow_size,
                         NULL);

    *height = *width = arrow_size;

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_downarrow_size(gint* width, gint* height)
{
    GtkRequisition requisition;
    ensure_button_arrow_widget();

    gtk_widget_size_request(gParts->buttonArrowWidget, &requisition);
    *width = requisition.width;
    *height = requisition.height;

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_toolbar_separator_width(gint* size)
{
    gboolean wide_separators;
    gint separator_width;
    GtkStyle* style;

    ensure_toolbar_widget();

    style = gParts->toolbarWidget->style;

    gtk_widget_style_get(gParts->toolbarWidget,
                         "space-size", size,
                         "wide-separators",  &wide_separators,
                         "separator-width", &separator_width,
                         NULL);

    /* Just in case... */
    *size = MAX(*size, (wide_separators ? separator_width : style->xthickness));

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_expander_size(gint* size)
{
    ensure_expander_widget();
    gtk_widget_style_get(gParts->expanderWidget,
                         "expander-size", size,
                         NULL);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_treeview_expander_size(gint* size)
{
    ensure_tree_view_widget();
    gtk_widget_style_get(gParts->treeViewWidget,
                         "expander-size", size,
                         NULL);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_menu_separator_height(gint *size)
{
    gboolean wide_separators;
    gint     separator_height;

    ensure_menu_separator_widget();

    gtk_widget_style_get(gParts->menuSeparatorWidget,
                          "wide-separators",  &wide_separators,
                          "separator-height", &separator_height,
                          NULL);

    if (wide_separators)
        *size = separator_height + gParts->menuSeparatorWidget->style->ythickness;
    else
        *size = gParts->menuSeparatorWidget->style->ythickness * 2;

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_scalethumb_metrics(GtkOrientation orient, gint* thumb_length, gint* thumb_height)
{
  GtkWidget* widget;

  ensure_scale_widget();
  widget = ((orient == GTK_ORIENTATION_HORIZONTAL) ? gParts->hScaleWidget : gParts->vScaleWidget);

  gtk_widget_style_get (widget,
                        "slider_length", thumb_length,
                        "slider_width", thumb_height,
                        NULL);

  return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_scrollbar_metrics(MozGtkScrollbarMetrics *metrics)
{
    ensure_scrollbar_widget();

    gtk_widget_style_get (gParts->horizScrollbarWidget,
                          "slider_width", &metrics->slider_width,
                          "trough_border", &metrics->trough_border,
                          "stepper_size", &metrics->stepper_size,
                          "stepper_spacing", &metrics->stepper_spacing,
                          NULL);

    metrics->min_slider_size =
        GTK_RANGE(gParts->horizScrollbarWidget)->min_slider_size;

    return MOZ_GTK_SUCCESS;
}

gboolean
moz_gtk_images_in_menus()
{
    gboolean result;
    GtkSettings* settings;

    ensure_image_menu_item_widget();
    settings = gtk_widget_get_settings(gParts->imageMenuItemWidget);

    g_object_get(settings, "gtk-menu-images", &result, NULL);
    return result;
}

gint
moz_gtk_widget_paint(GtkThemeWidgetType widget, GdkDrawable* drawable,
                     GdkRectangle* rect, GdkRectangle* cliprect,
                     GtkWidgetState* state, gint flags,
                     GtkTextDirection direction)
{
    switch (widget) {
    case MOZ_GTK_BUTTON:
        if (state->depressed) {
            ensure_toggle_button_widget();
            return moz_gtk_button_paint(drawable, rect, cliprect, state,
                                        (GtkReliefStyle) flags,
                                        gParts->toggleButtonWidget, direction);
        }
        ensure_button_widget();
        return moz_gtk_button_paint(drawable, rect, cliprect, state,
                                    (GtkReliefStyle) flags, gParts->buttonWidget,
                                    direction);
        break;
    case MOZ_GTK_CHECKBUTTON:
    case MOZ_GTK_RADIOBUTTON:
        return moz_gtk_toggle_paint(drawable, rect, cliprect, state,
                                    !!(flags & MOZ_GTK_WIDGET_CHECKED),
                                    !!(flags & MOZ_GTK_WIDGET_INCONSISTENT),
                                    (widget == MOZ_GTK_RADIOBUTTON),
                                    direction);
        break;
    case MOZ_GTK_SCROLLBAR_BUTTON:
        return moz_gtk_scrollbar_button_paint(drawable, rect, cliprect, state,
                                              (GtkScrollbarButtonFlags) flags,
                                              direction);
        break;
    case MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_TRACK_VERTICAL:
        return moz_gtk_scrollbar_trough_paint(widget, drawable, rect,
                                              cliprect, state, direction);
        break;
    case MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:
        return moz_gtk_scrollbar_thumb_paint(widget, drawable, rect,
                                             cliprect, state, direction);
        break;
    case MOZ_GTK_SCALE_HORIZONTAL:
    case MOZ_GTK_SCALE_VERTICAL:
        return moz_gtk_scale_paint(drawable, rect, cliprect, state,
                                   (GtkOrientation) flags, direction);
        break;
    case MOZ_GTK_SCALE_THUMB_HORIZONTAL:
    case MOZ_GTK_SCALE_THUMB_VERTICAL:
        return moz_gtk_scale_thumb_paint(drawable, rect, cliprect, state,
                                         (GtkOrientation) flags, direction);
        break;
    case MOZ_GTK_SPINBUTTON:
        return moz_gtk_spin_paint(drawable, rect, direction);
        break;
    case MOZ_GTK_SPINBUTTON_UP:
    case MOZ_GTK_SPINBUTTON_DOWN:
        return moz_gtk_spin_updown_paint(drawable, rect,
                                         (widget == MOZ_GTK_SPINBUTTON_DOWN),
                                         state, direction);
        break;
    case MOZ_GTK_SPINBUTTON_ENTRY:
        ensure_spin_widget();
        return moz_gtk_entry_paint(drawable, rect, cliprect, state,
                                   gParts->spinWidget, direction);
        break;
    case MOZ_GTK_GRIPPER:
        return moz_gtk_gripper_paint(drawable, rect, cliprect, state,
                                     direction);
        break;
    case MOZ_GTK_TREEVIEW:
        return moz_gtk_treeview_paint(drawable, rect, cliprect, state,
                                      direction);
        break;
    case MOZ_GTK_TREE_HEADER_CELL:
        return moz_gtk_tree_header_cell_paint(drawable, rect, cliprect, state,
                                              flags, direction);
        break;
    case MOZ_GTK_TREE_HEADER_SORTARROW:
        return moz_gtk_tree_header_sort_arrow_paint(drawable, rect, cliprect,
                                                    state,
                                                    (GtkArrowType) flags,
                                                    direction);
        break;
    case MOZ_GTK_TREEVIEW_EXPANDER:
        return moz_gtk_treeview_expander_paint(drawable, rect, cliprect, state,
                                               (GtkExpanderStyle) flags, direction);
        break;
    case MOZ_GTK_EXPANDER:
        return moz_gtk_expander_paint(drawable, rect, cliprect, state,
                                      (GtkExpanderStyle) flags, direction);
        break;
    case MOZ_GTK_ENTRY:
        ensure_entry_widget();
        return moz_gtk_entry_paint(drawable, rect, cliprect, state,
                                   gParts->entryWidget, direction);
        break;
    case MOZ_GTK_ENTRY_CARET:
        return moz_gtk_caret_paint(drawable, rect, cliprect, direction);
        break;
    case MOZ_GTK_DROPDOWN:
        return moz_gtk_combo_box_paint(drawable, rect, cliprect, state,
                                       (gboolean) flags, direction);
        break;
    case MOZ_GTK_DROPDOWN_ARROW:
        return moz_gtk_combo_box_entry_button_paint(drawable, rect, cliprect,
                                                    state, flags, direction);
        break;
    case MOZ_GTK_DROPDOWN_ENTRY:
        ensure_combo_box_entry_widgets();
        return moz_gtk_entry_paint(drawable, rect, cliprect, state,
                                   gParts->comboBoxEntryTextareaWidget, direction);
        break;
    case MOZ_GTK_CHECKBUTTON_CONTAINER:
    case MOZ_GTK_RADIOBUTTON_CONTAINER:
        return moz_gtk_container_paint(drawable, rect, cliprect, state,
                                       (widget == MOZ_GTK_RADIOBUTTON_CONTAINER),
                                       direction);
        break;
    case MOZ_GTK_CHECKBUTTON_LABEL:
    case MOZ_GTK_RADIOBUTTON_LABEL:
        return moz_gtk_toggle_label_paint(drawable, rect, cliprect, state,
                                          (widget == MOZ_GTK_RADIOBUTTON_LABEL),
                                          direction);
        break;
    case MOZ_GTK_TOOLBAR:
        return moz_gtk_toolbar_paint(drawable, rect, cliprect, direction);
        break;
    case MOZ_GTK_TOOLBAR_SEPARATOR:
        return moz_gtk_toolbar_separator_paint(drawable, rect, cliprect,
                                               direction);
        break;
    case MOZ_GTK_TOOLTIP:
        return moz_gtk_tooltip_paint(drawable, rect, cliprect, direction);
        break;
    case MOZ_GTK_FRAME:
        return moz_gtk_frame_paint(drawable, rect, cliprect, direction);
        break;
    case MOZ_GTK_RESIZER:
        return moz_gtk_resizer_paint(drawable, rect, cliprect, state,
                                     direction);
        break;
    case MOZ_GTK_PROGRESSBAR:
        return moz_gtk_progressbar_paint(drawable, rect, cliprect, direction);
        break;
    case MOZ_GTK_PROGRESS_CHUNK:
        return moz_gtk_progress_chunk_paint(drawable, rect, cliprect,
                                            direction);
        break;
    case MOZ_GTK_TAB:
        return moz_gtk_tab_paint(drawable, rect, cliprect,
                                 (GtkTabFlags) flags, direction);
        break;
    case MOZ_GTK_TABPANELS:
        return moz_gtk_tabpanels_paint(drawable, rect, cliprect, direction);
        break;
    case MOZ_GTK_TAB_SCROLLARROW:
        return moz_gtk_tab_scroll_arrow_paint(drawable, rect, cliprect, state,
                                              (GtkArrowType) flags, direction);
        break;
    case MOZ_GTK_MENUBAR:
        return moz_gtk_menu_bar_paint(drawable, rect, cliprect, direction);
        break;
    case MOZ_GTK_MENUPOPUP:
        return moz_gtk_menu_popup_paint(drawable, rect, cliprect, direction);
        break;
    case MOZ_GTK_MENUSEPARATOR:
        return moz_gtk_menu_separator_paint(drawable, rect, cliprect,
                                            direction);
        break;
    case MOZ_GTK_MENUITEM:
        return moz_gtk_menu_item_paint(drawable, rect, cliprect, state, flags,
                                       direction);
        break;
    case MOZ_GTK_MENUARROW:
        return moz_gtk_menu_arrow_paint(drawable, rect, cliprect, state,
                                        direction);
        break;
    case MOZ_GTK_TOOLBARBUTTON_ARROW:
        return moz_gtk_downarrow_paint(drawable, rect, cliprect, state);
        break;
    case MOZ_GTK_CHECKMENUITEM:
    case MOZ_GTK_RADIOMENUITEM:
        return moz_gtk_check_menu_item_paint(drawable, rect, cliprect, state,
                                             (gboolean) flags,
                                             (widget == MOZ_GTK_RADIOMENUITEM),
                                             direction);
        break;
    case MOZ_GTK_SPLITTER_HORIZONTAL:
        return moz_gtk_vpaned_paint(drawable, rect, cliprect, state);
        break;
    case MOZ_GTK_SPLITTER_VERTICAL:
        return moz_gtk_hpaned_paint(drawable, rect, cliprect, state);
        break;
    case MOZ_GTK_WINDOW:
        return moz_gtk_window_paint(drawable, rect, cliprect, direction);
        break;
    default:
        g_warning("Unknown widget type: %d", widget);
    }

    return MOZ_GTK_UNKNOWN_WIDGET;
}

GtkWidget* moz_gtk_get_scrollbar_widget(void)
{
    if (!is_initialized)
        return NULL;
    ensure_scrollbar_widget();
    return gParts->horizScrollbarWidget;
}

gint
moz_gtk_shutdown()
{
    GtkWidgetClass *entry_class;
    entry_class = g_type_class_peek(GTK_TYPE_ENTRY);
    g_type_class_unref(entry_class);

    is_initialized = FALSE;

    return MOZ_GTK_SUCCESS;
}

void moz_gtk_destroy_theme_parts_widgets(GtkThemeParts* parts)
{
    if (!parts)
        return;

    if (parts->tooltipWidget) {
        gtk_widget_destroy(parts->tooltipWidget);
        parts->tooltipWidget = NULL;
    }

    if (parts->protoWindow) {
        gtk_widget_destroy(parts->protoWindow);
        parts->protoWindow = NULL;
    }
}
