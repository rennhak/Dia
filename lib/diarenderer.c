/* Dia -- an diagram creation/manipulation program
 * Copyright (C) 1998 Alexander Larsson
 *
 * diarenderer.c - GObject based dia renderer base class
 * Copyright (C) 1998-2002 Various Dia developers
 * Copyright (C) 2002 Hans Breuer (refactoring)
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


#include "diarenderer.h"
#include "object.h"

struct _BezierApprox {
  Point *points;
  int numpoints;
  int currpoint;
};

static void dia_renderer_class_init (DiaRendererClass *klass);

static int get_width_pixels (DiaRenderer *);
static int get_height_pixels (DiaRenderer *);

static void begin_render (DiaRenderer *);
static void end_render (DiaRenderer *);

static void set_linewidth (DiaRenderer *renderer, real linewidth);
static void set_linecaps (DiaRenderer *renderer, LineCaps mode);
static void set_linejoin (DiaRenderer *renderer, LineJoin mode);
static void set_linestyle (DiaRenderer *renderer, LineStyle mode);
static void set_dashlength (DiaRenderer *renderer, real length);
static void set_fillstyle (DiaRenderer *renderer, FillStyle mode);
static void set_font (DiaRenderer *renderer, DiaFont *font, real height);

static void draw_line (DiaRenderer *renderer,
                       Point *start, Point *end,
                       Color *color);
static void fill_polygon (DiaRenderer *renderer,
                          Point *points, int num_points,
                          Color *color);
static void draw_arc (DiaRenderer *renderer,
                      Point *center,
                      real width, real height,
                      real angle1, real angle2,
                      Color *color);
static void fill_arc (DiaRenderer *renderer,
                      Point *center,
                      real width, real height,
                      real angle1, real angle2,
                      Color *color);
static void draw_ellipse (DiaRenderer *renderer,
                          Point *center,
                          real width, real height,
                          Color *color);
static void fill_ellipse (DiaRenderer *renderer,
                          Point *center,
                          real width, real height,
                          Color *color);
static void draw_bezier (DiaRenderer *renderer,
                         BezPoint *points,
                         int numpoints,
                         Color *color);
static void fill_bezier (DiaRenderer *renderer,
                         BezPoint *points,
                         int numpoints,
                         Color *color);
static void draw_string (DiaRenderer *renderer,
                         const gchar *text,
                         Point *pos,
                         Alignment alignment,
                         Color *color);
static void draw_image (DiaRenderer *renderer,
                        Point *point,
                        real width, real height,
                        DiaImage image);
static void draw_text  (DiaRenderer *renderer,
                        Text *text);

static void draw_rect (DiaRenderer *renderer,
                       Point *ul_corner, Point *lr_corner,
                       Color *color);
static void draw_polyline (DiaRenderer *renderer,
                           Point *points, int num_points,
                           Color *color);
static void draw_polygon (DiaRenderer *renderer,
                          Point *points, int num_points,
                          Color *color);

static real get_text_width (DiaRenderer *renderer,
                            const gchar *text, int length);

static void draw_rounded_rect (DiaRenderer *renderer,
                               Point *ul_corner, Point *lr_corner,
                               Color *color, real rounding);
static void fill_rounded_rect (DiaRenderer *renderer,
                               Point *ul_corner, Point *lr_corner,
                               Color *color, real rounding);
static void draw_line_with_arrows  (DiaRenderer *renderer, 
                                    Point *start, Point *end, 
                                    real line_width,
                                    Color *line_color,
                                    Arrow *start_arrow,
                                    Arrow *end_arrow);
static void draw_arc_with_arrows  (DiaRenderer *renderer, 
                                  Point *start, Point *end,
                                  Point *midpoint,
                                  real line_width,
                                  Color *color,
                                  Arrow *start_arrow,
                                  Arrow *end_arrow);
static void draw_polyline_with_arrows (DiaRenderer *renderer, 
                                       Point *points, int num_points,
                                       real line_width,
                                       Color *color,
                                       Arrow *start_arrow,
                                       Arrow *end_arrow);
static void draw_bezier_with_arrows (DiaRenderer *renderer, 
                                    BezPoint *points,
                                    int num_points,
                                    real line_width,
                                    Color *color,
                                    Arrow *start_arrow,
                                    Arrow *end_arrow);

static gpointer parent_class = NULL;

GType
dia_renderer_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (DiaRendererClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) dia_renderer_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (DiaRenderer),
        0,              /* n_preallocs */
	NULL            /* init */
      };

      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "DiaRenderer",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
draw_object (DiaRenderer *renderer,
	   Object *object) 
{
  object->ops->draw(object, renderer);
}

static void
renderer_finalize (GObject *object)
{
  DiaRenderer *renderer = DIA_RENDERER (object);

  if (renderer->font)
    dia_font_unref (renderer->font);

  if (renderer->bezier)
    {
      if (renderer->bezier->points)
        g_free (renderer->bezier->points);
      g_free (renderer->bezier);
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
dia_renderer_class_init (DiaRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  DiaRendererClass *renderer_class = DIA_RENDERER_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = renderer_finalize;

  renderer_class->get_width_pixels  = get_width_pixels;
  renderer_class->get_height_pixels = get_height_pixels;
  renderer_class->draw_object = draw_object;
  renderer_class->get_text_width = get_text_width;

  renderer_class->begin_render = begin_render;
  renderer_class->end_render   = end_render;

  renderer_class->set_linewidth  = set_linewidth;
  renderer_class->set_linecaps   = set_linecaps;
  renderer_class->set_linejoin   = set_linejoin;
  renderer_class->set_linestyle  = set_linestyle;
  renderer_class->set_dashlength = set_dashlength;
  renderer_class->set_fillstyle  = set_fillstyle;
  renderer_class->set_font       = set_font;

  renderer_class->draw_line    = draw_line;
  renderer_class->fill_polygon = fill_polygon;
  renderer_class->draw_arc     = draw_arc;
  renderer_class->fill_arc     = fill_arc;
  renderer_class->draw_ellipse = draw_ellipse;
  renderer_class->fill_ellipse = fill_ellipse;
  renderer_class->draw_string  = draw_string;
  renderer_class->draw_image   = draw_image;
  renderer_class->draw_text    = draw_text;

  /* medium level functions */
  renderer_class->draw_bezier  = draw_bezier;
  renderer_class->fill_bezier  = fill_bezier;
  renderer_class->draw_rect = draw_rect;
  renderer_class->draw_polyline  = draw_polyline;
  renderer_class->draw_polygon   = draw_polygon;

  /* highest level functions */
  renderer_class->draw_rounded_rect = draw_rounded_rect;
  renderer_class->fill_rounded_rect = fill_rounded_rect;
  renderer_class->draw_line_with_arrows = draw_line_with_arrows;
  renderer_class->draw_arc_with_arrows  = draw_arc_with_arrows;
  renderer_class->draw_polyline_with_arrows = draw_polyline_with_arrows;
  renderer_class->draw_bezier_with_arrows = draw_bezier_with_arrows;
}

static void 
begin_render (DiaRenderer *object)
{
  g_warning ("%s::begin_render not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (object)));
}

static void 
end_render (DiaRenderer *object)
{
  g_warning ("%s::end_render not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (object)));
}

static void 
set_linewidth (DiaRenderer *object, real linewidth)
{
  g_warning ("%s::set_line_width not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (object)));
}

static void 
set_linecaps (DiaRenderer *object, LineCaps mode)
{
  g_warning ("%s::set_line_caps not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (object)));
}

static void 
set_linejoin (DiaRenderer *renderer, LineJoin mode)
{
  g_warning ("%s::set_line_join not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (renderer)));
}

static void 
set_linestyle (DiaRenderer *renderer, LineStyle mode)
{
  g_warning ("%s::set_line_style not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (renderer)));
}

static void 
set_dashlength (DiaRenderer *renderer, real length)
{
  g_warning ("%s::set_dash_length not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (renderer)));
}

static void 
set_fillstyle (DiaRenderer *renderer, FillStyle mode)
{
  g_warning ("%s::set_fill_style not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (renderer)));
}

static void 
set_font (DiaRenderer *renderer, DiaFont *font, real height)
{
  if (renderer->font)
    dia_font_unref (renderer->font);
  renderer->font = dia_font_ref (font);
  renderer->font_height = height;
}


static void 
draw_line (DiaRenderer *renderer, Point *start, Point *end, Color *color)
{
  g_warning ("%s::draw_line not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (renderer)));
}

static void 
fill_polygon (DiaRenderer *renderer, Point *points, int num_points, Color *color)
{
  g_warning ("%s::fill_polygon not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (renderer)));
}

static void 
draw_arc (DiaRenderer *renderer, Point *center, 
          real width, real height, real angle1, real angle2,
          Color *color)
{
  g_warning ("%s::draw_arc not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (renderer)));
}

static void 
fill_arc (DiaRenderer *renderer, Point *center,
          real width, real height, real angle1, real angle2,
          Color *color)
{
  g_warning ("%s::fill_arc not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (renderer)));
}

static void 
draw_ellipse (DiaRenderer *renderer, Point *center,
              real width, real height, 
              Color *color)
{
  g_warning ("%s::draw_ellipse not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (renderer)));
}

static void 
fill_ellipse (DiaRenderer *renderer, Point *center,
              real width, real height, Color *color)
{
  g_warning ("%s::fill_ellipse not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (renderer)));
}

static void 
draw_string (DiaRenderer *renderer,
             const gchar *text, Point *pos, Alignment alignment,
             Color *color)
{
  g_warning ("%s::draw_string not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (renderer)));
}

static void 
draw_image (DiaRenderer *renderer,
            Point *point, real width, real height,
            DiaImage image)
{
  g_warning ("%s::draw_image not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (renderer)));
}

static void
draw_text (DiaRenderer *renderer,
           Text* text)
{
  g_warning ("%s::draw_text not implemented!", 
             G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (renderer)));
}

/*
 * medium level functions, implemented by the above
 */
#define BEZIER_SUBDIVIDE_LIMIT 0.03
#define BEZIER_SUBDIVIDE_LIMIT_SQ (BEZIER_SUBDIVIDE_LIMIT*BEZIER_SUBDIVIDE_LIMIT)

static void
bezier_add_point(BezierApprox *bezier,
                 Point *point)
{
  /* Grow if needed: */
  if (bezier->currpoint == bezier->numpoints) {
    bezier->numpoints += 40;
    bezier->points = g_realloc(bezier->points,
                               bezier->numpoints*sizeof(Point));
  }
  
  bezier->points[bezier->currpoint] = *point;
  
  bezier->currpoint++;
}

static void
bezier_add_lines(BezierApprox *bezier,
                 Point points[4])
{
  Point u, v, x, y;
  Point r[4];
  Point s[4];
  Point middle;
  coord delta;
  real v_len_sq;

  /* Check if almost flat: */
  u = points[1];
  point_sub(&u, &points[0]);
  v = points[3];
  point_sub(&v, &points[0]);
  y = v;
  v_len_sq = point_dot(&v,&v);
  if (isnan(v_len_sq)) {
    g_warning("v_len_sq is NaN while calculating bezier curve!");
    return;
  }
  if (v_len_sq < 0.000001)
    v_len_sq = 0.000001;
  point_scale(&y, point_dot(&u,&v)/v_len_sq);
  x = u;
  point_sub(&x,&y);
  delta = point_dot(&x,&x);
  if (delta < BEZIER_SUBDIVIDE_LIMIT_SQ) {
    u = points[2];
    point_sub(&u, &points[3]);
    v = points[0];
    point_sub(&v, &points[3]);
    y = v;
    v_len_sq = point_dot(&v,&v);
    if (v_len_sq < 0.000001)
      v_len_sq = 0.000001;
    point_scale(&y, point_dot(&u,&v)/v_len_sq);
    x = u;
    point_sub(&x,&y);
    delta = point_dot(&x,&x);
    if (delta < BEZIER_SUBDIVIDE_LIMIT_SQ) { /* Almost flat, draw a line */
      bezier_add_point(bezier, &points[3]);
      return;
    }
  }
  /* Subdivide into two bezier curves: */

  middle = points[1];
  point_add(&middle, &points[2]);
  point_scale(&middle, 0.5);
  
  r[0] = points[0];
  
  r[1] = points[0];
  point_add(&r[1], &points[1]);
  point_scale(&r[1], 0.5);

  r[2] = r[1];
  point_add(&r[2], &middle);
  point_scale(&r[2], 0.5);

  s[3] = points[3];

  s[2] = points[2];
  point_add(&s[2], &points[3]);
  point_scale(&s[2], 0.5);
  
  s[1] = s[2];
  point_add(&s[1], &middle);
  point_scale(&s[1], 0.5);

  r[3] = r[2];
  point_add(&r[3], &s[1]);
  point_scale(&r[3], 0.5);
  
  s[0] = r[3];

  bezier_add_lines(bezier, r);
  bezier_add_lines(bezier, s);
}

static void
bezier_add_curve(BezierApprox *bezier,
                 Point points[4])
{
  /* Is the bezier curve malformed? */
  if ( (distance_point_point(&points[0], &points[1]) < 0.00001) &&
       (distance_point_point(&points[2], &points[3]) < 0.00001) &&
       (distance_point_point(&points[0], &points[3]) < 0.00001)) {
    bezier_add_point(bezier, &points[3]);
  }
  
  bezier_add_lines(bezier, points);
}

static void
approximate_bezier (BezierApprox *bezier,
                    BezPoint *points, int numpoints)
{
  Point curve[4];
  int i;

  if (points[0].type != BEZ_MOVE_TO)
    g_warning("first BezPoint must be a BEZ_MOVE_TO");
  curve[3] = points[0].p1;
  bezier_add_point(bezier, &points[0].p1);
  for (i = 1; i < numpoints; i++)
    switch (points[i].type) {
    case BEZ_MOVE_TO:
      g_warning("only first BezPoint can be a BEZ_MOVE_TO");
      curve[3] = points[i].p1;
      break;
    case BEZ_LINE_TO:
      bezier_add_point(bezier, &points[i].p1);
      curve[3] = points[i].p1;
      break;
    case BEZ_CURVE_TO:
      curve[0] = curve[3];
      curve[1] = points[i].p1;
      curve[2] = points[i].p2;
      curve[3] = points[i].p3;
      bezier_add_curve(bezier, curve);
      break;
    }
}

/* bezier approximation with straight lines */
static void 
draw_bezier (DiaRenderer *renderer,
             BezPoint *points, int numpoints,
             Color *color)
{
  BezierApprox *bezier;

  if (renderer->bezier)
    bezier = renderer->bezier;
  else
    renderer->bezier = bezier = g_new0 (BezierApprox, 1);

  if (bezier->points == NULL) {
    bezier->numpoints = 30;
    bezier->points = g_malloc(bezier->numpoints*sizeof(Point));
  }

  bezier->currpoint = 0;
  approximate_bezier (bezier, points, numpoints);

  DIA_RENDERER_GET_CLASS (renderer)->draw_polyline (renderer,
                                                    bezier->points,
                                                    bezier->currpoint,
                                                    color);
}

static void 
fill_bezier (DiaRenderer *renderer,
             BezPoint *points, int numpoints,
             Color *color)
{
  BezierApprox *bezier;

  if (renderer->bezier)
    bezier = renderer->bezier;
  else
    renderer->bezier = bezier = g_new0 (BezierApprox, 1);

  if (bezier->points == NULL) {
    bezier->numpoints = 30;
    bezier->points = g_malloc(bezier->numpoints*sizeof(Point));
  }

  bezier->currpoint = 0;
  approximate_bezier (bezier, points, numpoints);

  DIA_RENDERER_GET_CLASS (renderer)->fill_polygon (renderer,
                                                   bezier->points,
                                                   bezier->currpoint,
                                                   color);
}

static void 
draw_rect (DiaRenderer *renderer,
           Point *ul_corner, Point *lr_corner,
           Color *color)
{
  DiaRendererClass *klass = DIA_RENDERER_GET_CLASS (renderer);
  Point ur, ll;

  ur.x = lr_corner->x;
  ur.y = ul_corner->y;
  ll.x = ul_corner->x;
  ll.y = lr_corner->y;

  klass->draw_line (renderer, ul_corner, &ur, color);
  klass->draw_line (renderer, &ur, lr_corner, color);
  klass->draw_line (renderer, lr_corner, &ll, color);
  klass->draw_line (renderer, &ll, ul_corner, color);
}

static void 
draw_polyline (DiaRenderer *renderer,
               Point *points, int num_points,
               Color *color)
{
  DiaRendererClass *klass = DIA_RENDERER_GET_CLASS (renderer);
  int i;

  for (i = 0; i < num_points - 1; i++)
    klass->draw_line (renderer, &points[i+0], &points[i+1], color);
}

static void 
draw_polygon (DiaRenderer *renderer,
              Point *points, int num_points,
              Color *color)
{
  DiaRendererClass *klass = DIA_RENDERER_GET_CLASS (renderer);
  int i;

  g_return_if_fail (1 > num_points);

  for (i = 0; i < num_points - 1; i++)
    klass->draw_line (renderer, &points[i+0], &points[i+1], color);
  /* close it in any case */
  if (   (points[0].x != points[num_points-1].x) 
      || (points[0].y != points[num_points-1].y))
    klass->draw_line (renderer, &points[num_points-1], &points[0], color);
}

static void 
draw_rounded_rect (DiaRenderer *renderer, 
                   Point *ul_corner, Point *lr_corner,
                   Color *color, real radius) 
{
  DiaRendererClass *renderer_ops = DIA_RENDERER_GET_CLASS (renderer);
  Point start, end, center;

  radius = MIN(radius, (lr_corner->x-ul_corner->x)/2);
  radius = MIN(radius, (lr_corner->y-ul_corner->y)/2);
  start.x = center.x = ul_corner->x+radius;
  end.x = lr_corner->x-radius;
  start.y = end.y = ul_corner->y;
  renderer_ops->draw_line(renderer, &start, &end, color);
  start.y = end.y = lr_corner->y;
  renderer_ops->draw_line(renderer, &start, &end, color);

  center.y = ul_corner->y+radius;
  renderer_ops->draw_arc(renderer, &center, 
			  2.0*radius, 2.0*radius,
			  90.0, 180.0, color);
  center.x = end.x;
  renderer_ops->draw_arc(renderer, &center, 
			  2.0*radius, 2.0*radius,
			  0.0, 90.0, color);

  start.y = ul_corner->y+radius;
  start.x = end.x = ul_corner->x;
  end.y = center.y = lr_corner->y-radius;
  renderer_ops->draw_line(renderer, &start, &end, color);
  start.x = end.x = lr_corner->x;
  renderer_ops->draw_line(renderer, &start, &end, color);

  center.y = lr_corner->y-radius;
  center.x = ul_corner->x+radius;
  renderer_ops->draw_arc(renderer, &center, 
			  2.0*radius, 2.0*radius,
			  180.0, 270.0, color);
  center.x = lr_corner->x-radius;
  renderer_ops->draw_arc(renderer, &center, 
			  2.0*radius, 2.0*radius,
			  270.0, 360.0, color);
}

static void 
fill_rounded_rect(DiaRenderer *renderer, 
                  Point *ul_corner, Point *lr_corner,
                  Color *color, real radius)
{
  DiaRendererClass *renderer_ops = DIA_RENDERER_GET_CLASS (renderer);
  Point start, end, center;

  radius = MIN(radius, (lr_corner->x-ul_corner->x)/2);
  radius = MIN(radius, (lr_corner->y-ul_corner->y)/2);
  start.x = center.x = ul_corner->x+radius;
  end.x = lr_corner->x-radius;
  start.y = ul_corner->y;
  end.y = lr_corner->y;
  renderer_ops->fill_rect(renderer, &start, &end, color);

  center.y = ul_corner->y+radius;
  renderer_ops->fill_arc(renderer, &center, 
			  2.0*radius, 2.0*radius,
			  90.0, 180.0, color);
  center.x = end.x;
  renderer_ops->fill_arc(renderer, &center, 
			  2.0*radius, 2.0*radius,
			  0.0, 90.0, color);


  start.x = ul_corner->x;
  start.y = ul_corner->y+radius;
  end.x = lr_corner->x;
  end.y = center.y = lr_corner->y-radius;
  renderer_ops->fill_rect(renderer, &start, &end, color);

  center.y = lr_corner->y-radius;
  center.x = ul_corner->x+radius;
  renderer_ops->fill_arc(renderer, &center, 
			  2.0*radius, 2.0*radius,
			  180.0, 270.0, color);
  center.x = lr_corner->x-radius;
  renderer_ops->fill_arc(renderer, &center, 
			  2.0*radius, 2.0*radius,
			  270.0, 360.0, color);
}

static void
draw_line_with_arrows(DiaRenderer *renderer, 
                      Point *startpoint, 
                      Point *endpoint,
                      real line_width,
                      Color *color,
                      Arrow *start_arrow,
                      Arrow *end_arrow)
{
  Point oldstart = *startpoint;
  Point oldend = *endpoint;
  if (start_arrow != NULL && start_arrow->type != ARROW_NONE) {
    Point move_arrow, move_line;
    Point arrow_head;
    calculate_arrow_point(start_arrow, startpoint, endpoint, 
			  &move_arrow, &move_line,
			  line_width);
    arrow_head = *startpoint;
    point_sub(&arrow_head, &move_arrow);
    point_sub(startpoint, &move_line);
    arrow_draw(renderer, start_arrow->type,
	       &arrow_head, endpoint,
	       start_arrow->length, start_arrow->width,
	       line_width,
	       color, &color_white);
#ifdef STEM
    Point line_start = startpoint;
    startpoint = arrow_head;
    point_normalize(&move);
    point_scale(&move, start_arrow->length);
    point_sub(startpoint, &move);
    renderer_ops->draw_line(renderer, &line_start, startpoint, color);
#endif
  }
  if (end_arrow != NULL && end_arrow->type != ARROW_NONE) {
    Point move_arrow, move_line;
    Point arrow_head;
    calculate_arrow_point(end_arrow, endpoint, startpoint,
 			  &move_arrow, &move_line,
			  line_width);
    arrow_head = *endpoint;
    point_sub(&arrow_head, &move_arrow);
    point_sub(endpoint, &move_line);
    arrow_draw(renderer, end_arrow->type,
	       &arrow_head, startpoint,
	       end_arrow->length, end_arrow->width,
	       line_width,
	       color, &color_white);
#ifdef STEM
    Point line_start = endpoint;
    endpoint = arrow_head;
    point_normalize(&move);
    point_scale(&move, end_arrow->length);
    point_sub(endpoint, &move);
    DIA_RENDERER_GET_CLASS(renderer)->draw_line(renderer, &line_start, endpoint, color);
#endif
  }
  DIA_RENDERER_GET_CLASS(renderer)->draw_line(renderer, startpoint, endpoint, color);

  *startpoint = oldstart;
  *endpoint = oldend;
}

static void
draw_polyline_with_arrows(DiaRenderer *renderer, 
                          Point *points, int num_points,
                          real line_width,
                          Color *color,
                          Arrow *start_arrow,
                          Arrow *end_arrow)
{
  /* Index of first and last point with a non-zero length segment */
  int firstline = 0;
  int lastline = num_points;
  Point oldstart = points[firstline];
  Point oldend = points[lastline-1];

  if (start_arrow != NULL && start_arrow->type != ARROW_NONE) {
    Point move_arrow, move_line;
    Point arrow_head;
    while (firstline < num_points-1 &&
	   distance_point_point(&points[firstline], 
				&points[firstline+1]) < 0.0000001)
      firstline++;
    if (firstline == num_points-1)
      firstline = 0; /* No non-zero lines, it doesn't matter. */
    oldstart = points[firstline];
    calculate_arrow_point(start_arrow, 
			  &points[firstline], &points[firstline+1], 
			  &move_arrow, &move_line,
			  line_width);
    arrow_head = points[firstline];
    point_sub(&arrow_head, &move_arrow);
    point_sub(&points[firstline], &move_line);
    arrow_draw(renderer, start_arrow->type,
	       &arrow_head, &points[firstline+1],
	       start_arrow->length, start_arrow->width,
	       line_width,
	       color, &color_white);
#ifdef STEM
    Point line_start = &points[firstline];
    &points[firstline] = arrow_head;
    point_normalize(&move);
    point_scale(&move, start_arrow->length);
    point_sub(&points[firstline], &move);
    DIA_RENDERER_GET_CLASS(renderer)->draw_line(renderer, &line_start, &points[firstline], color);
#endif
  }
  if (end_arrow != NULL && end_arrow->type != ARROW_NONE) {
    Point move_arrow, move_line;
    Point arrow_head;
    while (lastline > 0 && 
	   distance_point_point(&points[lastline-1], 
				&points[lastline-2]) < 0.0000001)
      lastline--;
    if (lastline == 0)
      firstline = num_points; /* No non-zero lines, it doesn't matter. */
    oldend = points[lastline-1];
    calculate_arrow_point(end_arrow, &points[lastline-1], 
			  &points[lastline-2],
 			  &move_arrow, &move_line,
			  line_width);
    arrow_head = points[lastline-1];
    point_sub(&arrow_head, &move_arrow);
    point_sub(&points[lastline-1], &move_line);
    arrow_draw(renderer, end_arrow->type,
	       &arrow_head, &points[lastline-2],
	       end_arrow->length, end_arrow->width,
	       line_width,
	       color, &color_white);
#ifdef STEM
    Point line_start = &points[lastline-1];
    &points[lastline-1] = arrow_head;
    point_normalize(&move);
    point_scale(&move, end_arrow->length);
    point_sub(&points[lastline-1], &move);
    DIA_RENDERER_GET_CLASS(renderer)->draw_line(renderer, &line_start, &points[lastline-1], color);
#endif
  }
  /* Don't draw degenerate line segments at end of line */
  DIA_RENDERER_GET_CLASS(renderer)->draw_polyline(renderer, &points[firstline], 
			       lastline-firstline, color);

  points[firstline] = oldstart;
  points[lastline-1] = oldend;
}

/** Figure the equation for a line given by two points.
 * Returns FALSE if the line is vertical (infinite a).
 */
static gboolean
points_to_line(real *a, real *b, Point *p1, Point *p2)
{
    if (fabs(p1->x - p2->x) < 0.000000001)
      return FALSE;
    *a = (p2->y-p1->y)/(p2->x-p1->x);
    *b = p1->y-(*a)*p1->x;
    return TRUE;
}

/** Find the intersection between two lines.
 * Returns TRUE if the lines intersect in a single point.
 */
static gboolean
intersection_line_line(Point *cross,
		       Point *p1a, Point *p1b,
		       Point *p2a, Point *p2b)
{
  real a1, b1, a2, b2;

  /* Find coefficients of lines */
  if (!(points_to_line(&a1, &b1, p1a, p1b))) {
    if (!(points_to_line(&a2, &b2, p2a, p2b))) {
      if (fabs(p1a->x-p2a->x) < 0.00000001) {
	*cross = *p1a;
	return TRUE;
      } else return FALSE;
    }
    cross->x = p1a->x;
    cross->y = a2*(p1a->x)+b2;
    return TRUE;
  }
  if (!(points_to_line(&a2, &b2, p2a, p2b))) {
    cross->x = p2a->x;
    cross->y = a1*(p2a->x)+b1;
    return TRUE;
  }
  /* Solve */
  if (fabs(a1-a2) < 0.000000001) {
    if (fabs(b1-b2) < 0.000000001) {
      *cross = *p1a;
      return TRUE;
    } else {
      return FALSE;
    }
  } else {
    /*
    a1x+b1 = a2x+b2;
    a1x = a2x+b2-b1;
    a1x-a2x = b2-b1;
    (a1-a2)x = b2-b1;
    x = (b2-b1)/(a1-a2)
    */
    cross->x = (b2-b1)/(a1-a2);
    cross->y = a1*cross->x+b1;
    return TRUE;
  }
}

/** Given three points, find the center of the circle they describe.
 * Returns FALSE if the center could not be determined (i.e. the points
 * all lie really close together).
 * The renderer should disappear once the debugging is done.
 */
static gboolean
find_center_point(Point *center, Point *p1, Point *p2, Point *p3) 
{
  Point mid1;
  Point mid2;
  Point orth1;
  Point orth2;
  real tmp;

  /* Find vector from middle between two points towards center */
  mid1 = *p1;
  point_sub(&mid1, p2);
  point_scale(&mid1, 0.5);
  orth1 = mid1;
  point_add(&mid1, p2); /* Now midpoint between p1 & p2 */
  tmp = orth1.x;
  orth1.x = orth1.y;
  orth1.y = -tmp;
  point_add(&orth1, &mid1);


  /* Again, with two other points */
  mid2 = *p2;
  point_sub(&mid2, p3);
  point_scale(&mid2, 0.5);
  orth2 = mid2;
  point_add(&mid2, p3); /* Now midpoint between p2 & p3 */
  tmp = orth2.x;
  orth2.x = orth2.y;
  orth2.y = -tmp;
  point_add(&orth2, &mid2);

  /* The intersection between these two is the center */
  if (!intersection_line_line(center, &mid1, &orth1, &mid2, &orth2)) {
    /* Degenerate circle */
    /* Either the points are really close together, or directly apart */
    printf("Degenerate circle\n");
    if (fabs((p1->x + p2->x + p3->x)/3 - p1->x) < 0.0000001 &&
	fabs((p1->y + p2->y + p3->y)/3 - p1->y) < 0.0000001)
      return FALSE;
    
    return TRUE;
  }
  return TRUE;
}

static real 
point_cross(Point *p1, Point *p2)
{
  return p1->x * p2->y - p2->x * p1->y;
}

static void
draw_arc_with_arrows (DiaRenderer *renderer, 
                      Point *startpoint, 
                      Point *endpoint,
                      Point *midpoint,
                      real line_width,
                      Color *color,
                      Arrow *start_arrow,
                      Arrow *end_arrow)
{
  Point oldstart = *startpoint;
  Point oldend = *endpoint;
  Point center;
  real width, angle1, angle2;
  Point dot1, dot2;
  gboolean righthand;
  
  if (!find_center_point(&center, startpoint, endpoint, midpoint)) {
    /* Degenerate circle -- should have been caught by the drawer? */
  }


  dot1 = *startpoint;
  point_sub(&dot1, endpoint);
  point_normalize(&dot1);
  dot2 = *midpoint;
  point_sub(&dot2, endpoint);
  point_normalize(&dot2);
  righthand = point_cross(&dot1, &dot2) > 0;
  
  if (start_arrow != NULL && start_arrow->type != ARROW_NONE) {
    Point move_arrow, move_line;
    Point arrow_head;
    Point arrow_end;
    real tmp;

    arrow_end = *startpoint;
    point_sub(&arrow_end, &center);
    tmp = arrow_end.x;
    if (righthand) {
      arrow_end.x = -arrow_end.y;
      arrow_end.y = tmp;
    } else {
      arrow_end.x = arrow_end.y;
      arrow_end.y = -tmp;
    }
    point_add(&arrow_end, startpoint);

    calculate_arrow_point(start_arrow, startpoint, &arrow_end, 
			  &move_arrow, &move_line,
			  line_width);
    arrow_head = *startpoint;
    point_sub(&arrow_head, &move_arrow);
    point_sub(startpoint, &move_line);
    arrow_draw(renderer, start_arrow->type,
	       &arrow_head, &arrow_end,
	       start_arrow->length, start_arrow->width,
	       line_width,
	       color, &color_white);
#ifdef STEM
    Point line_start = startpoint;
    startpoint = arrow_head;
    point_normalize(&move);
    point_scale(&move, start_arrow->length);
    point_sub(startpoint, &move);
    DIA_RENDERER_GET_CLASS(renderer)->draw_line(renderer, &line_start, startpoint, color);
#endif
  }
  if (end_arrow != NULL && end_arrow->type != ARROW_NONE) {
    Point move_arrow, move_line;
    Point arrow_head;
    Point arrow_end;
    real tmp;

    arrow_end = *endpoint;
    point_sub(&arrow_end, &center);
    tmp = arrow_end.x;
    if (righthand) {
      arrow_end.x = arrow_end.y;
      arrow_end.y = -tmp;
    } else {
      arrow_end.x = -arrow_end.y;
      arrow_end.y = tmp;
    }
    point_add(&arrow_end, endpoint);

    calculate_arrow_point(end_arrow, endpoint, &arrow_end,
 			  &move_arrow, &move_line,
			  line_width);
    arrow_head = *endpoint;
    point_sub(&arrow_head, &move_arrow);
    point_sub(endpoint, &move_line);
    arrow_draw(renderer, end_arrow->type,
	       &arrow_head, &arrow_end,
	       end_arrow->length, end_arrow->width,
	       line_width,
	       color, &color_white);
#ifdef STEM
    Point line_start = endpoint;
    endpoint = arrow_head;
    point_normalize(&move);
    point_scale(&move, end_arrow->length);
    point_sub(endpoint, &move);
    DIA_RENDERER_GET_CLASS(renderer)->draw_line(renderer, &line_start, endpoint, color);
#endif
  }

  if (!find_center_point(&center, startpoint, endpoint, midpoint)) {
    /* Not sure what to do here */
    *startpoint = oldstart;
    *endpoint = oldend;
    printf("Second degenerate circle\n");
    return;
  }
  width = 2*distance_point_point(&center, startpoint);
  angle1 = -atan2(startpoint->y-center.y, startpoint->x-center.x)*180.0/M_PI;
  while (angle1 < 0.0) angle1 += 360.0;
  angle2 = -atan2(endpoint->y-center.y, endpoint->x-center.x)*180.0/M_PI;
  while (angle2 < 0.0) angle2 += 360.0;
  if (righthand) {
    real tmp = angle1;
    angle1 = angle2;
    angle2 = tmp;
  }
  DIA_RENDERER_GET_CLASS(renderer)->draw_arc(renderer, &center, width, width,
			  angle1, angle2, color);

  *startpoint = oldstart;
  *endpoint = oldend;
}

static void
draw_bezier_with_arrows(DiaRenderer *renderer, 
                        BezPoint *points,
                        int num_points,
                        real line_width,
                        Color *color,
                        Arrow *start_arrow,
                        Arrow *end_arrow)
{
  Point startpoint, endpoint;

  startpoint = points[0].p1;
  endpoint = points[num_points-1].p3;

  if (start_arrow != NULL && start_arrow->type != ARROW_NONE) {
    Point move_arrow;
    Point move_line;
    Point arrow_head;
    calculate_arrow_point(start_arrow, &points[0].p1, &points[1].p1,
			  &move_arrow, &move_line,
			  line_width);
    arrow_head = points[0].p1;
    point_sub(&arrow_head, &move_arrow);
    point_sub(&points[0].p1, &move_line);
    arrow_draw(renderer, start_arrow->type,
	       &arrow_head, &points[1].p1,
	       start_arrow->length, start_arrow->width,
	       line_width,
	       color, &color_white);
    if (0) {
      Point line_start = points[0].p1;
      points[0].p1 = arrow_head;
      point_normalize(&move_arrow);
      point_scale(&move_arrow, start_arrow->length);
      point_sub(&points[0].p1, &move_arrow);
      DIA_RENDERER_GET_CLASS(renderer)->draw_line(renderer, &line_start, &points[0].p1, color);
    }
  }
  if (end_arrow != NULL && end_arrow->type != ARROW_NONE) {
    Point move_arrow;
    Point move_line;
    Point arrow_head;
    calculate_arrow_point(end_arrow,
			  &points[num_points-1].p3, &points[num_points-1].p2,
			  &move_arrow, &move_line,
			  line_width);
    arrow_head = points[num_points-1].p3;
    point_sub(&arrow_head, &move_arrow);
    point_sub(&points[num_points-1].p3, &move_line);
    arrow_draw(renderer, end_arrow->type,
	       &arrow_head, &points[num_points-1].p2,
	       end_arrow->length, end_arrow->width,
	       line_width,
	       color, &color_white);
    if (0) {
      Point line_start = points[num_points-1].p3;
      points[num_points-1].p3 = arrow_head;
      point_normalize(&move_arrow);
      point_scale(&move_arrow, end_arrow->length);
      point_sub(&points[num_points-1].p3, &move_arrow);
      DIA_RENDERER_GET_CLASS(renderer)->draw_line(renderer, &line_start, &points[num_points-1].p3, color);
    }
  }
  DIA_RENDERER_GET_CLASS(renderer)->draw_bezier(renderer, points, num_points, color);

  points[0].p1 = startpoint;
  points[num_points-1].p3 = endpoint;
  
}

/*
 * Should we really provide this ? It formerly was an 'interactive op'
 * and depends on DiaRenderer::set_font() not or properly overwritten 
 */
static real 
get_text_width (DiaRenderer *renderer,
                const gchar *text, int length)
{
  real ret = 0;

  if (renderer->font) {
    char *str = g_strndup (text, length);

    ret = dia_font_string_width (str, 
                                 renderer->font,
                                 renderer->font_height);
    g_free (str);
  }

  return ret;
}

static int 
get_width_pixels (DiaRenderer *renderer)
{
  g_return_val_if_fail (renderer->is_interactive, 0);
  return 0;
}

static int 
get_height_pixels (DiaRenderer *renderer)
{
  g_return_val_if_fail (renderer->is_interactive, 0);
  return 0;
}


/*
 * non member functions
 */
int
dia_renderer_get_width_pixels (DiaRenderer *renderer)
{
  return DIA_RENDERER_GET_CLASS(renderer)->get_width_pixels (renderer);
}

int
dia_renderer_get_height_pixels (DiaRenderer *renderer)
{
  return DIA_RENDERER_GET_CLASS(renderer)->get_height_pixels (renderer);
}
