/* Dia -- an diagram creation/manipulation program
 * Copyright (C) 1998 Alexander Larsson
 *
 * List of "dynamic" (animated) objects, and their refresh rates
 * Copyright (C) 2002 Cyrille Chépélov
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
 */
#include "dynamic_refresh.h"
#include "dynamic_obj.h"
#include "diagram.h"
#include "diagramdata.h"
#include "object_ops.h"
#include "app/connectionpoint_ops.h"
#include <glib.h>

static guint timeout = 0;
static guint idle_id = 0;
static guint timer_id = 0;

static void foreach_dynobj(Object* obj, gpointer data) {
    gboolean moved = FALSE;
    GList* list = open_diagrams;

    while (list != NULL) {
        Diagram* dia;
        DiagramData* obj_ddata;
        Layer* layer;

        dia = (Diagram*)list->data;
        list = g_list_next(list);
        
        layer = dia_object_get_parent_layer(obj);
        if (!layer) continue;

        obj_ddata = layer_get_parent_diagram(layer);
        if (!obj_ddata) continue;

        if (dia->data == obj_ddata) {
            if (!moved) {
                obj->ops->move(obj,&obj->position);
                moved = TRUE;
            }
            object_add_updates(obj,dia);
            diagram_update_connections_object(dia,obj, TRUE);
        }
    }
}

static gboolean timer_handler(gpointer data) {
    GList* list;

    dynobj_list_foreach(foreach_dynobj,data);

    list = open_diagrams;
    while (list != NULL) {
        Diagram* dia = (Diagram*)list->data;
        list = g_list_next(list);
        
        diagram_flush(dia);
    }
    
    dynobj_refresh_kick();
    return TRUE;
}

static gboolean idle_handler(gpointer data) {
    guint new_timeout;
    
    new_timeout = dynobj_list_get_dynobj_rate();
    if (timeout != new_timeout) {
        if (timer_id) {
            gtk_timeout_remove(timer_id);
            timer_id = 0;
        }
        timeout = new_timeout;
        if (timeout) {
            timer_id = gtk_timeout_add(timeout,timer_handler,NULL);
        }
    }
    idle_id = 0;
    return FALSE;
}

void dynobj_refresh_kick(void) {
    if (!idle_id)
        idle_id = gtk_idle_add_priority(GTK_PRIORITY_LOW,idle_handler,NULL);
}

void dynobj_refresh_init(void) {
        /* NOP */
}

void dynobj_refresh_finish(void) {
    if (timer_id) gtk_timeout_remove(timer_id);
    if (idle_id) gtk_idle_remove(idle_id);
}