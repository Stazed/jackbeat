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
 *   SVN:$Id: track.c 193 2008-07-01 23:00:42Z olivier $
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gui/slider.h"
#include "sequence.h"
#include "util.h"

struct slider_t
{
    GtkWidget *       layout;
    GtkAdjustment *   adj;
    slider_state_t    state;
    slider_curve_t    curve;
    GtkAllocation *   allocs[2];
#if GTK_CHECK_VERSION(3,0,0)
    cairo_surface_t*  cst_surface[2];
    cairo_surface_t*  cst_gradients[2];
#else
    GdkPixmap     *   pixmaps[2];
    GdkPixmap     *   gradients[2];
#endif
    GtkWidget **      overlaps;
    GdkCursor *       empty_cursor;
    int               start_x_root;
    int               start_y_root;
    int               start_position;
} ;

static gboolean slider_expose (GtkWidget *layout, GdkEventExpose *event,
                               slider_t *slider);
static void     slider_value_changed (GtkWidget * adj, slider_t * slider);
static gboolean slider_button_event (GtkWidget *widget, GdkEventButton *event, slider_t *slider);
static gboolean slider_motion_event (GtkWidget *widget, GdkEventMotion *event, slider_t *slider);
static void     slider_destroy (GtkWidget *object, slider_t *slider);
static gboolean slider_wheel_scroll_event (GtkWidget *widget, GdkEventScroll *event, slider_t *slider);

slider_t *
slider_new (GtkWidget *layout, GtkAdjustment *adj)
{
    slider_t *slider = malloc (sizeof (slider_t));
    slider->layout = layout;
    slider->adj = adj;
    slider->state = SLIDER_FOLDED;
    slider->curve = SLIDER_LINEAR;
    slider->allocs[SLIDER_FOLDED] = calloc (1, sizeof (GtkAllocation));
    slider->allocs[SLIDER_UNFOLDED] = calloc (1, sizeof (GtkAllocation));
#if GTK_CHECK_VERSION(3,0,0)
    slider->cst_surface[SLIDER_FOLDED] = NULL;
    slider->cst_surface[SLIDER_UNFOLDED] = NULL;
    slider->cst_gradients[SLIDER_FOLDED] = NULL;
    slider->cst_gradients[SLIDER_UNFOLDED] = NULL;
#else
    slider->pixmaps[SLIDER_FOLDED] = NULL;
    slider->pixmaps[SLIDER_UNFOLDED] = NULL;
    slider->gradients[SLIDER_FOLDED] = NULL;
    slider->gradients[SLIDER_UNFOLDED] = NULL;
#endif
    slider->overlaps = malloc (sizeof (GtkWidget *));
    slider->overlaps[0] = NULL;

    gtk_widget_add_events (layout, GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK | GDK_SCROLL_MASK |
                           GDK_POINTER_MOTION_MASK);

    g_signal_connect (G_OBJECT (adj), "value_changed",
                      G_CALLBACK (slider_value_changed), (gpointer) slider);
    g_signal_connect (G_OBJECT (layout), "button-press-event",
                      G_CALLBACK (slider_button_event), slider);
    g_signal_connect (G_OBJECT (layout), "button-release-event",
                      G_CALLBACK (slider_button_event), slider);
    g_signal_connect (G_OBJECT (layout), "motion-notify-event",
                      G_CALLBACK (slider_motion_event), slider);
    g_signal_connect (G_OBJECT (layout), "expose-event",
                      G_CALLBACK (slider_expose), slider);
    g_signal_connect (G_OBJECT (layout), "destroy",
                      G_CALLBACK (slider_destroy), slider);
    g_signal_connect (G_OBJECT (layout), "scroll-event",
                      G_CALLBACK (slider_wheel_scroll_event), slider);

#if GTK_CHECK_VERSION(3,0,0)
    /* FIXME */
    cairo_surface_t *s;
    cairo_t *cr;
    GdkPixbuf *pixbuf;

    s = cairo_image_surface_create (CAIRO_FORMAT_A1, 3, 3);
    cr = cairo_create (s);
    cairo_arc (cr, 1.5, 1.5, 1.5, 0, 2 * M_PI);
    cairo_fill (cr);
    cairo_destroy (cr);

    pixbuf = gdk_pixbuf_get_from_surface (s,
                                          0, 0,
                                          3, 3);

    cairo_surface_destroy (s);

    slider->empty_cursor = gdk_cursor_new_from_pixbuf (gdk_display_get_default(), pixbuf, 0, 0);

    g_object_unref (pixbuf);    
#else
    gchar data = 0;
    GdkColor color = { 0, 0, 0, 0 };
    GdkPixmap *pixmap = gdk_bitmap_create_from_data (NULL, &data, 1, 1);
    slider->empty_cursor = gdk_cursor_new_from_pixmap (pixmap, pixmap,
                                                       &color, &color,
                                                       0, 0);
    gdk_pixmap_unref (pixmap);
#endif
    return slider;
}

static void
slider_destroy (GtkWidget *object, slider_t *slider)
{
    gdk_cursor_unref (slider->empty_cursor);
#if GTK_CHECK_VERSION(3,0,0)
    if (slider->cst_surface[SLIDER_FOLDED])
        cairo_surface_destroy (slider->cst_surface[SLIDER_FOLDED]);
    if (slider->cst_surface[SLIDER_UNFOLDED])
        cairo_surface_destroy (slider->cst_surface[SLIDER_UNFOLDED]);
    if (slider->cst_gradients[SLIDER_FOLDED])
        cairo_surface_destroy (slider->cst_gradients[SLIDER_FOLDED]);
    if (slider->cst_gradients[SLIDER_UNFOLDED])
        cairo_surface_destroy (slider->cst_gradients[SLIDER_UNFOLDED]);
#else
    if (slider->pixmaps[SLIDER_FOLDED])
        gdk_pixmap_unref (slider->pixmaps[SLIDER_FOLDED]);
    if (slider->pixmaps[SLIDER_UNFOLDED])
        gdk_pixmap_unref (slider->pixmaps[SLIDER_UNFOLDED]);
    if (slider->gradients[SLIDER_FOLDED])
        gdk_pixmap_unref (slider->gradients[SLIDER_FOLDED]);
    if (slider->gradients[SLIDER_UNFOLDED])
        gdk_pixmap_unref (slider->gradients[SLIDER_UNFOLDED]);
#endif  // GTK_CHECK_VERSION(3,0,0)
    free (slider->allocs[SLIDER_FOLDED]);
    free (slider->allocs[SLIDER_UNFOLDED]);
    free (slider->overlaps);
    free (slider);
}

void
slider_add_overlap (slider_t *slider, GtkWidget *widget)
{
    int i = 0;
    while (slider->overlaps[i++])
        ;
    slider->overlaps = realloc (slider->overlaps, (i + 1) * sizeof (GtkWidget *));
    slider->overlaps[i - 1] = widget;
    slider->overlaps[i] = NULL;
}

GtkAdjustment *
slider_get_adjustment (slider_t *slider)
{
    return slider->adj;
}

void
slider_set_curve (slider_t *slider, slider_curve_t curve)
{
    slider->curve = curve;
}

static double
slider_position_to_value (slider_t *slider, slider_state_t state, int position)
{
    double ratio = (double) position / (double) slider->allocs[state]->width;

    gdouble lower, upper;
    g_object_get (G_OBJECT (slider->adj),
                  "upper", &upper,
                  "lower", &lower,
                  NULL);

    double value = 0.0f;
    if (slider->curve == SLIDER_LINEAR)
        value = ratio * (upper - lower);
    else if (slider->curve == SLIDER_DECIBEL)
        value = SEQUENCE_SLIDE2DB (ratio);

    if (value > upper)
        value = upper;
    else if (value < lower)
        value = lower;

    return value;
}

static int
slider_value_to_position (slider_t *slider, slider_state_t state, double value)
{
    int position;
    double ratio = 0.5f;
    if (slider->curve == SLIDER_LINEAR)
    {
        gdouble lower, upper;
        g_object_get (G_OBJECT (slider->adj),
                      "upper", &upper,
                      "lower", &lower,
                      NULL);

        ratio = (value - lower) / (upper - lower);
    }
    else if (slider->curve == SLIDER_DECIBEL)
    {
        ratio = SEQUENCE_DB2SLIDE (value);
    }
    position = util_round (ratio * (double) slider->allocs[state]->width);

    if (position < 0)
        position = 0;
    else if (position >= slider->allocs[state]->width)
        position = slider->allocs[state]->width - 1;

    return position;
}

static int
slider_compute_position (slider_t *slider, slider_state_t state)
{
    return slider_value_to_position (slider, state,
                                     gtk_adjustment_get_value (slider->adj));
}

static void
slider_update_gradient (slider_t *slider, slider_state_t state)
{
#if GTK_CHECK_VERSION(3,0,0)
    if (slider->cst_gradients[state])
#else
    if (slider->gradients[state])   
#endif
    {
        dk_color_t from = {0x07, 0x45, 0x7e};
        dk_color_t to = {0xff, 0xff, 0xff};
        GtkAllocation alloc;
        alloc.x = 0;
        alloc.y = 0;
        alloc.width = slider->allocs[state]->width;
        alloc.height = slider->allocs[state]->height;
#if GTK_CHECK_VERSION(3,0,0)
        dk_draw_hgradient (slider->cst_gradients[state], &alloc, &from, &to);
#else
        dk_draw_hgradient (slider->gradients[state], &alloc, &from, &to);
#endif
    }
}

#if GTK_CHECK_VERSION(3,0,0)
static void
slider_update_pixmap (slider_t *slider, slider_state_t state)
{
    if (slider->cst_surface[state] && slider->cst_gradients[state])
    {
        cairo_t *cr = cairo_create (slider->cst_surface[state]);
        cairo_set_source_surface(cr, slider->cst_gradients[state], 0, 0);
        cairo_rectangle (cr, 0, 0, slider->allocs[state]->width, slider->allocs[state]->height);
        cairo_clip (cr);
        cairo_paint(cr);
        cairo_destroy(cr);
        
        GtkAllocation alloc;
        alloc.x = slider_compute_position (slider, state);
        alloc.y = 0;
        alloc.width = slider->allocs[state]->width - alloc.x;
        alloc.height = slider->allocs[state]->height;

        cr = cairo_create (slider->cst_surface[state]);
        dk_color_t color = {0x78, 0x78, 0x78};
        GdkColor _color = dk_set_colors(&color);

        gdk_cairo_set_source_color (cr, &_color);
        cairo_rectangle (cr, alloc.x, alloc.y, alloc.width, alloc.height);
        
        cairo_fill (cr);
        cairo_destroy (cr);
    }
}
#else
static void
slider_update_pixmap (slider_t *slider, slider_state_t state)
{
    if (slider->pixmaps[state] && slider->gradients[state])
    {
        cairo_t *cr = gdk_cairo_create (slider->pixmaps[state]);
        gdk_cairo_set_source_pixmap(cr, slider->gradients[state], 0, 0);
        cairo_rectangle (cr, 0, 0, slider->allocs[state]->width, slider->allocs[state]->height);
        cairo_clip (cr);
        cairo_paint(cr);
        cairo_destroy(cr);
        
#if 0
        gdk_draw_drawable (
                           slider->pixmaps[state],
                           gtk_widget_get_style(slider->layout)->fg_gc[gtk_widget_get_state (slider->layout)],
                           slider->gradients[state],
                           0, 0,
                           0, 0, slider->allocs[state]->width, slider->allocs[state]->height);
#endif

        GtkAllocation alloc;
        alloc.x = slider_compute_position (slider, state);
        alloc.y = 0;
        alloc.width = slider->allocs[state]->width - alloc.x;
        alloc.height = slider->allocs[state]->height;

        cairo_t *cr_slider_pixmap = gdk_cairo_create (slider->pixmaps[state]);
        dk_color_t color = {0x78, 0x78, 0x78};
        GdkColor _color = dk_set_colors(&color);

        gdk_cairo_set_source_color (cr_slider_pixmap, &_color);
        cairo_rectangle (cr_slider_pixmap, alloc.x, alloc.y, alloc.width, alloc.height);
        
        cairo_fill (cr_slider_pixmap);
        cairo_destroy (cr_slider_pixmap);
    }
}
#endif  // GTK_CHECK_VERSION(3,0,0)

static void
slider_queue_draw (slider_t *slider)
{
    GtkAllocation *alloc = slider->allocs[slider->state];
    gtk_widget_queue_draw_area (slider->layout,
                                alloc->x, alloc->y, alloc->width, alloc->height);
}

static void
slider_value_changed (GtkWidget * adj, slider_t * slider)
{
    slider_update_pixmap (slider, slider->state);
    slider_queue_draw (slider);
}

static gboolean
slider_button_event (GtkWidget *widget, GdkEventButton *event, slider_t *slider)
{
    gboolean handled = FALSE;
    if (slider->state == SLIDER_FOLDED)
    {
        GtkAllocation *alloc = slider->allocs[SLIDER_FOLDED];
        if ((event->type == GDK_BUTTON_PRESS) && (event->button == 1) &&
            (event->x >= alloc->x) && (event->x < alloc->width + alloc->x) &&
            (event->y >= alloc->y) && (event->y < alloc->height + alloc->y))
        {
            int i = 0;
            while (slider->overlaps[i])
                gtk_widget_hide (slider->overlaps[i++]);

            gtk_main_iteration_do (FALSE);
            slider->state = SLIDER_UNFOLDED;
            slider_update_pixmap (slider, slider->state);
            slider_queue_draw (slider);
            gdk_window_set_cursor (gtk_widget_get_window(slider->layout), slider->empty_cursor);
            slider->start_x_root = event->x_root;
            slider->start_y_root = event->y_root;
            slider->start_position = slider_compute_position (slider, SLIDER_UNFOLDED);
            handled = TRUE;
        }
    }
    else
    {
        if ((event->type == GDK_BUTTON_RELEASE) && (event->button == 1))
        {
            slider_queue_draw (slider);
            slider->state = SLIDER_FOLDED;
            slider_update_pixmap (slider, slider->state);
            gtk_main_iteration_do (FALSE);
            int i = 0;
            while (slider->overlaps[i])
                gtk_widget_show (slider->overlaps[i++]);
            gdk_window_set_cursor (gtk_widget_get_window(slider->layout), NULL);
            GdkDisplay *display = gdk_display_get_default ();
            GdkScreen *screen = gdk_display_get_default_screen (display);
            gdk_display_warp_pointer (display, screen, slider->start_x_root, slider->start_y_root);
            slider_queue_draw (slider);
            handled = TRUE;
        }
    }
    return handled;
}

static gboolean
slider_motion_event (GtkWidget *widget, GdkEventMotion *event, slider_t *slider)
{
    gboolean handled = FALSE;
    if (slider->state == SLIDER_UNFOLDED)
    {
        double position_delta = event->x_root - slider->start_x_root;
        double position = slider->start_position + position_delta;
        double newvalue = slider_position_to_value (slider, SLIDER_UNFOLDED, position);
        double value = gtk_adjustment_get_value (slider->adj);

        if (newvalue != value)
        {
            gtk_adjustment_set_value (slider->adj, newvalue);
            gtk_adjustment_value_changed (slider->adj);
        }

        handled = TRUE;
    }
    return handled;
}

static gboolean
slider_wheel_scroll_event (GtkWidget *widget, GdkEventScroll *event, slider_t *slider)
{
    gboolean handled = FALSE;
    if (slider->state == SLIDER_FOLDED)
    {
        GtkAllocation *alloc = slider->allocs[SLIDER_FOLDED];
        int x, y;
        // event coordinates are not reliable on osx here
        gdk_window_get_pointer (gtk_widget_get_window(widget), &x, &y, NULL);
        if ((x >= alloc->x) && (x < alloc->width + alloc->x) &&
            (y >= alloc->y) && (y < alloc->height + alloc->y))
        {
            gdouble lower, upper, step, value;

            g_object_get (G_OBJECT (slider->adj),
                          "step-increment", &step,
                          "value", &value,
                          "lower", &lower,
                          "upper", &upper,
                          NULL);

            if (event->direction == GDK_SCROLL_UP)
                value += step;
            else if (event->direction == GDK_SCROLL_DOWN)
                value -= step;

            if (value < lower)
                value = lower;
            else if (value > upper)
                value = upper;

            if (value != gtk_adjustment_get_value (slider->adj))
            {
                gtk_adjustment_set_value (slider->adj, value);
                gtk_adjustment_value_changed (slider->adj);
            }
            handled = TRUE;
        }
    }
    return handled;
}

#if GTK_CHECK_VERSION(3,0,0)
void
slider_allocate (slider_t *slider, slider_state_t state, GtkAllocation * alloc)
{
    if (!slider->cst_surface[state] || (alloc->width != slider->allocs[state]->width)
        || (alloc->height != slider->allocs[state]->height))
    {
        if (slider->cst_surface[state])
            cairo_surface_destroy (slider->cst_surface[state]);

        if (slider->cst_gradients[state])
            cairo_surface_destroy (slider->cst_gradients[state]);

        memcpy (slider->allocs[state], alloc, sizeof (GtkAllocation));

        if (gtk_layout_get_bin_window(GTK_LAYOUT (slider->layout)))
        { 
            slider->cst_surface[state] = gdk_window_create_similar_surface (gtk_layout_get_bin_window(GTK_LAYOUT (slider->layout)),
                                             CAIRO_CONTENT_COLOR_ALPHA, alloc->width, alloc->height);
            slider->cst_gradients[state] = gdk_window_create_similar_surface (gtk_layout_get_bin_window(GTK_LAYOUT (slider->layout)),
                                             CAIRO_CONTENT_COLOR_ALPHA, alloc->width, alloc->height);

            slider_update_gradient (slider, state);
            slider_update_pixmap (slider, state);
        }
    }
}
#else
void
slider_allocate (slider_t *slider, slider_state_t state, GtkAllocation * alloc)
{
    if (!slider->pixmaps[state] || (alloc->width != slider->allocs[state]->width)
        || (alloc->height != slider->allocs[state]->height))
    {
        if (slider->pixmaps[state])
            g_object_unref (G_OBJECT (slider->pixmaps[state]));

        if (slider->gradients[state])
            g_object_unref (G_OBJECT (slider->gradients[state]));

        memcpy (slider->allocs[state], alloc, sizeof (GtkAllocation));

        if (gtk_layout_get_bin_window(GTK_LAYOUT (slider->layout)))
        {
            slider->pixmaps[state] = gdk_pixmap_new (gtk_layout_get_bin_window(GTK_LAYOUT (slider->layout)),
                                                     alloc->width, alloc->height, -1);
            slider->gradients[state] = gdk_pixmap_new (gtk_layout_get_bin_window(GTK_LAYOUT (slider->layout)),
                                                       alloc->width, alloc->height, -1);

            slider_update_gradient (slider, state);
            slider_update_pixmap (slider, state);
        }
    }
}
#endif  // GTK_CHECK_VERSION(3,0,0)

void
slider_get_allocation (slider_t *slider, slider_state_t state, GtkAllocation * alloc)
{
    memcpy (alloc, slider->allocs[state], sizeof (GtkAllocation));
}

slider_state_t
slider_get_state (slider_t *slider)
{
    return slider->state;
}

#if GTK_CHECK_VERSION(3,0,0)
static gboolean
slider_expose (GtkWidget *layout, GdkEventExpose *event, slider_t *slider)
{
    GtkAllocation *alloc = slider->allocs[slider->state];
    cairo_surface_t *surface = slider->cst_surface[slider->state];

    if (surface && gtk_layout_get_bin_window(GTK_LAYOUT (layout)))
    {
        GtkAllocation target;
        if (gdk_rectangle_intersect (&event->area, alloc, &target))
        {
            int srcx = target.x - alloc->x;
            int srcy = target.y - alloc->y;

            cairo_t *cr = gdk_cairo_create (gtk_layout_get_bin_window(GTK_LAYOUT (layout)));
            cairo_set_source_surface(cr, surface, target.x - srcx, target.y - srcy);
            cairo_rectangle (cr, target.x, target.y, target.width, target.height);
            cairo_clip (cr);
            cairo_paint(cr);
            cairo_destroy(cr);
   
            gtk_paint_shadow (gtk_widget_get_style(layout),
                              gtk_layout_get_bin_window(GTK_LAYOUT (layout)),
                              GTK_STATE_NORMAL,
                              GTK_SHADOW_IN,
                              &event->area, layout, NULL,
                              alloc->x, alloc->y,
                              alloc->width, alloc->height);
        }
    }
    return FALSE;
}
#else
static gboolean
slider_expose (GtkWidget *layout, GdkEventExpose *event, slider_t *slider)
{
    GtkAllocation *alloc = slider->allocs[slider->state];
    GdkPixmap *pixmap = slider->pixmaps[slider->state];

    if (pixmap && gtk_layout_get_bin_window(GTK_LAYOUT (layout)))
    {
        GtkAllocation target;
        if (gdk_rectangle_intersect (&event->area, alloc, &target))
        {
            int srcx = target.x - alloc->x;
            int srcy = target.y - alloc->y;

            cairo_t *cr = gdk_cairo_create (gtk_layout_get_bin_window(GTK_LAYOUT (layout)));
            gdk_cairo_set_source_pixmap(cr, pixmap, target.x - srcx, target.y - srcy);
            cairo_rectangle (cr, target.x, target.y, target.width, target.height);
            cairo_clip (cr);
            cairo_paint(cr);
            cairo_destroy(cr);

#if 0
            gdk_draw_drawable (
                               gtk_layout_get_bin_window(GTK_LAYOUT (layout)),
                               gtk_widget_get_style(layout)->fg_gc[gtk_widget_get_state (layout)],
                               pixmap,
                               srcx, srcy,
                               target.x, target.y, target.width, target.height);
#endif
            
            gtk_paint_shadow (gtk_widget_get_style(layout),
                              gtk_layout_get_bin_window(GTK_LAYOUT (layout)),
                              GTK_STATE_NORMAL,
                              GTK_SHADOW_IN,
                              &event->area, layout, NULL,
                              alloc->x, alloc->y,
                              alloc->width, alloc->height);
        }
    }
    return FALSE;
}
#endif  // GTK_CHECK_VERSION(3,0,0)