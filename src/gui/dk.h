/*
 *   Jackbeat - JACK sequencer
 *    
 *   Copyright (c) 2004-2008 Olivier Guilyardi <olivier {at} samalyse {dot} com>
 *    
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   SVN:$Id: grid.c 191 2008-06-30 19:23:58Z olivier $
 */

#ifndef JACKBEAT_DK_H
#define JACKBEAT_DK_H

#include <gdk/gdk.h>
#include <gtk/gtk.h>

typedef struct dk_color_t {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
} dk_color_t;

typedef struct dk_hsv_t {
    float hue;
    float saturation;
    float value;
} dk_hsv_t;

#define DK_COLOR_SET(color, r, g, b) color.red = r; color.green = g; color.blue = b

#if GTK_CHECK_VERSION(3,0,0)
cairo_t * dk_make_cr(cairo_surface_t *surface, dk_color_t *color);
void dk_make_gradient(cairo_t *colors[], cairo_surface_t *surface,
        dk_color_t *from, dk_color_t *to, int steps);
void dk_draw_hgradient(cairo_surface_t *surface, GtkAllocation *alloc,
        dk_color_t *from, dk_color_t *to);
void dk_draw_vgradient(cairo_surface_t *surface, GtkAllocation *alloc,
        dk_color_t *from, dk_color_t *to);
void dk_draw_line(cairo_surface_t *surface, dk_color_t *color, int x1, int y1, int x2, int y2);
void dk_draw_track_bg(cairo_surface_t *surface, GtkAllocation *alloc, int active,
        dk_hsv_t *hsv_shift, dk_color_t *hborder);
#else
cairo_t * dk_make_cr(GdkWindow *drawable, dk_color_t *color);
void dk_make_gradient(cairo_t *colors[], GdkWindow *drawable,
        dk_color_t *from, dk_color_t *to, int steps);
void dk_draw_hgradient(GdkWindow *drawable, GtkAllocation *alloc,
        dk_color_t *from, dk_color_t *to);
void dk_draw_vgradient(GdkWindow *drawable, GtkAllocation *alloc,
        dk_color_t *from, dk_color_t *to);
void dk_draw_line(GdkWindow *drawable, dk_color_t *color, int x1, int y1, int x2, int y2);
void dk_draw_track_bg(GdkWindow *drawable, GtkAllocation *alloc, int active,
        dk_hsv_t *hsv_shift, dk_color_t *hborder);
#endif

GdkColor dk_set_colors(dk_color_t *color);
void dk_make_single_gradient (dk_color_t *color, dk_color_t from, dk_color_t to, int single, int steps);
int dk_em(GtkWidget *widget, float em_size);
void dk_transform_hsv(dk_color_t *color, dk_hsv_t *hsv_shift);
#endif
