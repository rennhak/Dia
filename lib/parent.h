/* Dia -- an diagram creation/manipulation program
 * Copyright (C) 2003 Vadim Berezniker
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

//#include "diagram.h"
#include <glib.h>
#include "geometry.h"

GList *parent_list_affected(GList *obj_list);
Rectangle *parent_handle_extents(Object *obj);
Point parent_move_child_delta_out(Rectangle *p_ext, Rectangle *c_ext, const Point *delta);
Point parent_move_child_delta(Rectangle *p_ext, Rectangle *c_text, Point *delta);
Rectangle *parent_point_extents(Point *point);
gboolean parent_list_expand(GList *obj_list);
GList *parent_list_affected_hierarchy(GList *obj_list);
gboolean parent_handle_move_out_check(Object *object, Point *to);
gboolean parent_handle_move_in_check(Object *object, Point *to, Point *start_at);
