/* Dia -- a diagram creation/manipulation program
 * Copyright (C) 1998 Alexander Larsson
 *
 * sheets.c : a sheets and objects dialog
 * Copyright (C) 2002 M.C. Nelson
 *  
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *  
 */

#ifdef GNOME
#include <gnome.h>
#else
#include <gtk/gtk.h>
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gmodule.h>

#include "../lib/plug-ins.h"
#include "../lib/sheet.h"
#include "../lib/message.h"

#include "interface.h"
#include "sheets.h"
#include "sheets_dialog.h"
#include "gtkhwrapbox.h"

GtkWidget *sheets_dialog = NULL;
GSList *sheets_mods_list = NULL;
GtkTooltips *sheets_dialog_tooltips = NULL;
static gpointer custom_type_symbol;

/* Given a SheetObject and a SheetMod, create a new SheetObjectMod
   and hook it into the 'objects' list in the SheetMod->sheet
   
   NOTE: that the objects structure points to SheetObjectMod's and
        *not* SheetObject's */

SheetObjectMod *
sheets_append_sheet_object_mod(SheetObject *so, SheetMod *sm)
{
  SheetObjectMod *sheet_object_mod;
  ObjectType *ot;

  sheet_object_mod = g_new(SheetObjectMod, 1);
  sheet_object_mod->sheet_object = *so;
  sheet_object_mod->mod = SHEET_OBJECT_MOD_NONE;

  ot = object_get_type(so->object_type);
  g_assert(ot);
  if (ot->ops == ((ObjectType *)(custom_type_symbol))->ops)
    sheet_object_mod->type = OBJECT_TYPE_SVG;
  else
    sheet_object_mod->type = OBJECT_TYPE_PROGRAMMED;

  sm->sheet.objects = g_slist_append(sm->sheet.objects, sheet_object_mod);

  return sheet_object_mod;
}

/* Given a Sheet, create a SheetMod wrapper for a list of SheetObjectMod's */

SheetMod *
sheets_append_sheet_mods(Sheet *sheet)
{
  SheetMod *sheet_mod;
  GSList *sheet_objects_list;

  sheet_mod = g_new(SheetMod, 1);
  sheet_mod->sheet = *sheet;
  sheet_mod->type = SHEETMOD_TYPE_NORMAL;
  sheet_mod->mod = SHEETMOD_MOD_NONE;
  sheet_mod->sheet.objects = NULL;

  for (sheet_objects_list = sheet->objects; sheet_objects_list;
       sheet_objects_list = g_slist_next(sheet_objects_list))
    sheets_append_sheet_object_mod(sheet_objects_list->data, sheet_mod);
    
  sheets_mods_list = g_slist_append(sheets_mods_list, sheet_mod);

  return sheet_mod;
}

static gint
menu_item_compare_labels(gconstpointer a, gconstpointer b)
{
  GList *a_list;
  gchar *label;

  a_list = gtk_container_children(GTK_CONTAINER(GTK_MENU_ITEM(a)));
  g_assert(g_list_length(a_list) == 1);

  gtk_label_get(GTK_LABEL(a_list->data), &label);
  g_list_free(a_list);

  if (!strcmp(label, (gchar *)b))
    return 0;
  else
    return 1;
}

void
sheets_optionmenu_create(GtkWidget *option_menu, GtkWidget *wrapbox,
                         gchar *sheet_name)
{
  GtkWidget *optionmenu_menu;
  GSList *sheets_list;
  GList *menu_item_list;

  /* Delete the contents, if any, of this optionemnu first */

  optionmenu_menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(option_menu));
  gtk_container_foreach(GTK_CONTAINER(optionmenu_menu),
                        (GtkCallback)gtk_widget_destroy, NULL);

  if (!sheets_dialog_tooltips)
    sheets_dialog_tooltips = gtk_tooltips_new();

  for (sheets_list = sheets_mods_list; sheets_list;
       sheets_list = g_slist_next(sheets_list))
  {
    SheetMod *sheet_mod;
    GtkWidget *menu_item;
    gchar *tip;

    sheet_mod = sheets_list->data;

    /* We don't display sheets which have been deleted prior to Apply */

    if (sheet_mod->mod == SHEETMOD_MOD_DELETED)
      continue;

    menu_item = gtk_menu_item_new_with_label(sheet_mod->sheet.name);
    gtk_menu_append(GTK_MENU(optionmenu_menu), menu_item);

    tip = g_strdup_printf("%s\n%s", sheet_mod->sheet.description,
                              sheet_mod->sheet.scope == SHEET_SCOPE_SYSTEM
                              ? "System sheet" : "User sheet");
    gtk_tooltips_set_tip(GTK_TOOLTIPS(sheets_dialog_tooltips), menu_item, tip,
                         NULL);
    g_free(tip);

    gtk_widget_show(menu_item);

    gtk_object_set_data(GTK_OBJECT(menu_item), "wrapbox", wrapbox);

    gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
                       GTK_SIGNAL_FUNC(on_sheets_dialog_optionmenu_activate),
                       (gpointer)sheet_mod);
  }
 
  menu_item_list = gtk_container_children(GTK_CONTAINER(optionmenu_menu));

  /* If we were passed a sheet_name, then make the optionmenu point to that
     name after creation */

  if (sheet_name)
  {
    gint index;
    GList *list;

    list = g_list_find_custom(menu_item_list, sheet_name,
                              menu_item_compare_labels);
    g_assert(list);

    index = g_list_position(menu_item_list, list);
    gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), index);
    gtk_menu_item_activate(GTK_MENU_ITEM(g_list_nth_data(menu_item_list,
                                                         index)));
  }
  else
  {
    gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), 0);
    gtk_menu_item_activate(GTK_MENU_ITEM(menu_item_list->data));
  }

  g_list_free(menu_item_list);
}

gboolean
sheets_dialog_create(void)
{
  GList *plugin_list;
  GSList *sheets_list;
  GtkWidget *option_menu;
  GtkWidget *sw;
  GtkWidget *wrapbox;
  gchar *sheet_left;
  gchar *sheet_right;

  if (sheets_mods_list)
    {
      /* FIXME: not sure if I understood the data structure
         but simply leaking isn't acceptable ... --hb
       */
      g_slist_foreach(sheets_mods_list, (GFunc)g_free, NULL);
      g_slist_free(sheets_mods_list);
    }
  sheets_mods_list = NULL;

  if (!GTK_IS_WIDGET(sheets_dialog))
  {
    sheets_dialog = create_sheets_main_dialog();

    /* This little bit identifies a custom object symbol so we can tell the
       difference later between a SVG shape and a Programmed shape */

    custom_type_symbol = NULL;
    for (plugin_list = dia_list_plugins(); plugin_list != NULL;
         plugin_list = g_list_next(plugin_list))
    {
       GModule *module = ((PluginInfo *)(plugin_list->data))->module;

       if (module == NULL)
         continue;

       if (g_module_symbol(module, "custom_type", &custom_type_symbol) == TRUE)
         break;
    }

    sheet_left = NULL;
    sheet_right = NULL;
  }
  else
  {
    option_menu = lookup_widget(sheets_dialog, "optionmenu_left");
    sheet_left = gtk_object_get_data(GTK_OBJECT(option_menu),
                                     "active_sheet_name");

    option_menu = lookup_widget(sheets_dialog, "optionmenu_right");
    sheet_right = gtk_object_get_data(GTK_OBJECT(option_menu),
                                      "active_sheet_name");

    wrapbox = lookup_widget(sheets_dialog, "wrapbox_left");
    gtk_widget_destroy(wrapbox);

    wrapbox = lookup_widget(sheets_dialog, "wrapbox_right");
    gtk_widget_destroy(wrapbox);
  }

  if (!custom_type_symbol)
  {
    message_warning ("Can't get symbol 'custom_type' from any module.\n"
                     "Editing shapes is disabled.");
    return FALSE;
  }

  for (sheets_list = get_sheets_list(); sheets_list;
       sheets_list = g_slist_next(sheets_list))
    sheets_append_sheet_mods(sheets_list->data);
    
  sw = lookup_widget(sheets_dialog, "scrolledwindow_right");
  wrapbox = gtk_hwrap_box_new(FALSE);
  gtk_widget_ref(wrapbox);
  gtk_object_set_data(GTK_OBJECT(sheets_dialog), "wrapbox_right", wrapbox);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), wrapbox);
  gtk_wrap_box_set_justify(GTK_WRAP_BOX(wrapbox), GTK_JUSTIFY_TOP);
  gtk_wrap_box_set_line_justify(GTK_WRAP_BOX(wrapbox), GTK_JUSTIFY_LEFT);
  gtk_widget_show(wrapbox);
  gtk_object_set_data(GTK_OBJECT(wrapbox), "is_left", FALSE);
  option_menu = lookup_widget(sheets_dialog, "optionmenu_right");
  sheets_optionmenu_create(option_menu, wrapbox, sheet_right);

  sw = lookup_widget(sheets_dialog, "scrolledwindow_left");
  wrapbox = gtk_hwrap_box_new(FALSE);
  gtk_widget_ref(wrapbox);
  gtk_object_set_data(GTK_OBJECT(sheets_dialog), "wrapbox_left", wrapbox);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), wrapbox);
  gtk_wrap_box_set_justify(GTK_WRAP_BOX(wrapbox), GTK_JUSTIFY_TOP);
  gtk_wrap_box_set_line_justify(GTK_WRAP_BOX(wrapbox), GTK_JUSTIFY_LEFT);
  gtk_widget_show(wrapbox);
  gtk_object_set_data(GTK_OBJECT(wrapbox), "is_left", (gpointer)TRUE);
  option_menu = lookup_widget(sheets_dialog, "optionmenu_left");
  sheets_optionmenu_create(option_menu, wrapbox, sheet_left);

  return TRUE;
}

void
create_object_pixmap(SheetObject *so, GtkWidget *parent,
                     GdkPixmap **pixmap, GdkBitmap **mask)
{
  GtkStyle *style;

  g_assert(so);
  g_assert(pixmap);
  g_assert(mask);
  
  style = gtk_widget_get_style(parent);
  
  if (so->pixmap != NULL)
  {
    *pixmap =
      gdk_pixmap_colormap_create_from_xpm_d(NULL,
                                            gtk_widget_get_colormap(parent),
                                            mask, 
                                            &style->bg[GTK_STATE_NORMAL],
                                            so->pixmap);
  }
  else
  {
    if (so->pixmap_file != NULL)
    {
      GdkPixbuf *pixbuf;

      pixbuf = gdk_pixbuf_new_from_file(so->pixmap_file);
      if (pixbuf != NULL)
      {
        gdk_pixbuf_render_pixmap_and_mask(pixbuf, pixmap, mask, 1.0);
        gdk_pixbuf_unref(pixbuf);
      }
    }
    else
    {
      *pixmap = NULL;
      *mask = NULL;
    }
  }
}

GtkWidget*
lookup_widget                          (GtkWidget       *widget,
                                        const gchar     *widget_name)
{
  GtkWidget *parent, *found_widget;

  for (;;)
    {
      if (GTK_IS_MENU (widget))
        parent = gtk_menu_get_attach_widget (GTK_MENU (widget));
      else
        parent = widget->parent;
      if (parent == NULL)
        break;
      widget = parent;
    }

  found_widget = (GtkWidget*) gtk_object_get_data (GTK_OBJECT (widget),
                                                   widget_name);
  if (!found_widget)
    g_warning ("Widget not found: %s", widget_name);
  return found_widget;
}

#include "pixmaps/n_a.xpm"
#include "pixmaps/line_break.xpm"

/* This is only called by the code written by glade and at the moment
   it is only used to create the static pixmap of the current sheet object. */

GtkWidget *
create_pixmap(GtkWidget *dialog, gchar *filename, gboolean arg3)
{
  GdkPixmap *pixmap, *mask;
  GtkWidget *button;
  GtkWidget *wrapbox;
  GList *button_list;
  SheetObjectMod *som;

  button = sheets_dialog_get_active_button(&wrapbox, &button_list);
  som = gtk_object_get_data(GTK_OBJECT(button), "sheet_object_mod");

  if (som)
    create_object_pixmap(&som->sheet_object, wrapbox, &pixmap, &mask);
  else
  {
    GtkStyle *style;
    gchar **icon;

    style = gtk_widget_get_style(wrapbox);
    
    if ((gboolean)gtk_object_get_data(GTK_OBJECT(button), "is_hidden_button")
        == TRUE)
      icon = n_a_xpm;
    else
      icon = line_break_xpm;

    pixmap =
         gdk_pixmap_colormap_create_from_xpm_d(NULL,
                                               gtk_widget_get_colormap(wrapbox),
                                               &mask,
                                               &style->bg[GTK_STATE_NORMAL],
                                               icon);
  }
    
  return gtk_pixmap_new(pixmap, mask);
}

/* The menu calls us here, after we've been instantiated */

void
sheets_dialog_show_callback(gpointer data, guint action, GtkWidget *widget)
{
  GtkWidget *wrapbox;
  GtkWidget *option_menu;
  static gboolean sheets_dialog_created = FALSE;

  if (!sheets_dialog_created)
    sheets_dialog_created = sheets_dialog_create();
  if (!sheets_dialog_created)
    return;

  wrapbox = gtk_object_get_data(GTK_OBJECT(sheets_dialog), "wrapbox_left");
  option_menu = lookup_widget(sheets_dialog, "optionmenu_left");
  sheets_optionmenu_create(option_menu, wrapbox, interface_current_sheet_name);

  g_assert(GTK_IS_WIDGET(sheets_dialog));
  gtk_widget_show(sheets_dialog);
}

gchar *
sheet_object_mod_get_type_string(SheetObjectMod *som)
{
  switch (som->type)
  {
  case OBJECT_TYPE_SVG:
    return "SVG Shape";
  case OBJECT_TYPE_PROGRAMMED:
    return "Programmed Object";
  default:
    g_assert_not_reached();
    return FALSE;           /* shut gcc up */
  }
}
