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
#include "gui/toggle.h"
#include "sequence.h"
#include "util.h"
#include "core/event.h"

struct toggle_t
{
    GtkWidget *       layout;
    char *            text;
    GtkAdjustment *   adj;
    int               active;
    int               hover;
    GtkAllocation *   alloc;
#if GTK_CHECK_VERSION(3,0,0)
    cairo_surface_t * cst_surface;
    cairo_surface_t * cst_gradients[2];
#else
    GdkPixmap     *   pixmap;
    GdkPixmap     *   gradients[2];
#endif
    dk_hsv_t          hsv_shift;
} ;

static gboolean toggle_expose (GtkWidget *layout, GdkEventExpose *event,
                               toggle_t *toggle);
static gboolean toggle_button_event (GtkWidget *widget, GdkEventButton *event, toggle_t *toggle);
static gboolean toggle_motion_event (GtkWidget *widget, GdkEventMotion *event, toggle_t *toggle);
static gboolean toggle_leave_event (GtkWidget *widget, GdkEventCrossing *event, toggle_t *toggle);
static void     toggle_destroy (GtkWidget *object, toggle_t *toggle);

toggle_t *
toggle_new (GtkWidget *layout, const char *text)
{
    toggle_t *toggle      = calloc (1, sizeof (toggle_t));
    toggle->layout        = layout;
    toggle->alloc         = calloc (1, sizeof (GtkAllocation));
#if GTK_CHECK_VERSION(3,0,0)
    toggle->cst_surface      = NULL;
    toggle->cst_gradients[0] = NULL;
    toggle->cst_gradients[1] = NULL;
#else
    toggle->pixmap        = NULL;
    toggle->gradients[0]  = NULL;
    toggle->gradients[1]  = NULL;
#endif
    toggle->text          = strdup (text);

    gtk_widget_add_events (layout, GDK_BUTTON_PRESS_MASK | GDK_LEAVE_NOTIFY_MASK);

    g_signal_connect (G_OBJECT (layout), "button-press-event",
                      G_CALLBACK (toggle_button_event), toggle);
    g_signal_connect (G_OBJECT (layout), "motion-notify-event",
                      G_CALLBACK (toggle_motion_event), toggle);
    g_signal_connect (G_OBJECT (layout), "leave-notify-event",
                      G_CALLBACK (toggle_leave_event), toggle);
    g_signal_connect (G_OBJECT (layout), "expose-event",
                      G_CALLBACK (toggle_expose), toggle);
    g_signal_connect (G_OBJECT (layout), "destroy",
                      G_CALLBACK (toggle_destroy), toggle);

    event_register (toggle, "toggled");

    return toggle;
}

static void
toggle_destroy (GtkWidget *object, toggle_t *toggle)
{
#if GTK_CHECK_VERSION(3,0,0)
    if (toggle->cst_gradients[0])
        cairo_surface_destroy (toggle->cst_gradients[0]);
    if (toggle->cst_gradients[1])
        cairo_surface_destroy (toggle->cst_gradients[1]);
    if (toggle->cst_surface)
        cairo_surface_destroy (toggle->cst_surface);
#else
    if (toggle->gradients[0])
        gdk_pixmap_unref (toggle->gradients[0]);
    if (toggle->gradients[1])
        gdk_pixmap_unref (toggle->gradients[1]);
    if (toggle->pixmap)
        gdk_pixmap_unref (toggle->pixmap);
#endif
    event_remove_source (toggle);

    free (toggle->text);
    free (toggle->alloc);
    free (toggle);
}

static void
toggle_update_gradient (toggle_t *toggle, int active)
{
#if GTK_CHECK_VERSION(3,0,0)
    if (toggle->cst_gradients[active])
    {
#else
    if (toggle->gradients[active])
    {
#endif
        GtkAllocation alloc = {0, 0, toggle->alloc->width, toggle->alloc->height};
        dk_color_t border = {0x78, 0x78, 0x78};

#if GTK_CHECK_VERSION(3,0,0)
        dk_draw_track_bg (toggle->cst_gradients[active], &alloc, active,
                          &toggle->hsv_shift, &border);
#else
        dk_draw_track_bg (GDK_DRAWABLE (toggle->gradients[active]), &alloc, active,
                          &toggle->hsv_shift, &border);
#endif
    }
}

static PangoLayout *
toggle_create_pango_layout (toggle_t *toggle, int hover)
{
    PangoContext *context = gtk_widget_get_pango_context (toggle->layout);
    PangoLayout *layout = pango_layout_new (context);
    char markup[256];
    if (hover)
        sprintf (markup, "<span size=\"smaller\" underline=\"single\" foreground=\"white\"><small>%s</small></span>",
                 toggle->text);
    else
        sprintf (markup,
                 "<span size=\"smaller\" foreground=\"white\"><small>%s</small></span>", toggle->text);

    pango_layout_set_markup (layout, markup, -1);
    return layout;
}

int
toggle_get_width (toggle_t *toggle)
{
    PangoLayout *layout = toggle_create_pango_layout (toggle, 0);
    int width, lwidth, height;
    pango_layout_get_pixel_size (layout, &lwidth, &height);
    width = lwidth * 2;
    width += ((width - lwidth) / 2) % 2; // Make sure width is even, for centering purpose
    g_object_unref (layout);
    return width;
}

static void
toggle_update_pixmap (toggle_t *toggle)
{
#if GTK_CHECK_VERSION(3,0,0)
    if (toggle->cst_surface && toggle->cst_gradients[toggle->active])
    {
        cairo_t *cr = cairo_create (toggle->cst_surface);
        cairo_set_source_surface (cr, toggle->cst_gradients[toggle->active], 0, 0);
#else
    if (toggle->pixmap && toggle->gradients[toggle->active])
    {
        cairo_t *cr = gdk_cairo_create (toggle->pixmap);
        gdk_cairo_set_source_pixmap(cr, toggle->gradients[toggle->active], 0, 0);
#endif
        cairo_rectangle (cr, 0, 0, toggle->alloc->width, toggle->alloc->height);
        cairo_clip (cr);
        cairo_paint(cr);
        cairo_destroy(cr);

#if 0        
        gdk_draw_drawable (
                           toggle->pixmap,
                           gtk_widget_get_style(toggle->layout)->fg_gc[gtk_widget_get_state (toggle->layout)],
                           toggle->gradients[toggle->active],
                           0, 0,
                           0, 0, toggle->alloc->width, toggle->alloc->height);
#endif
        
        PangoLayout *layout = toggle_create_pango_layout (toggle, toggle->hover);
        int lwidth, lheight;
        pango_layout_get_pixel_size (layout, &lwidth, &lheight);
     
#if GTK_CHECK_VERSION(3,0,0)
        cairo_t *toggle_label_cr = cairo_create (toggle->cst_surface);
        cairo_move_to (toggle_label_cr, (toggle->alloc->width - lwidth) / 2, (toggle->alloc->height - lheight) / 2);
        pango_cairo_show_layout (toggle_label_cr, layout);
        cairo_destroy(toggle_label_cr);
#else    
        gdk_draw_layout (toggle->pixmap, gtk_widget_get_style(toggle->layout)->fg_gc[gtk_widget_get_state (toggle->layout)],
                         (toggle->alloc->width - lwidth) / 2,
                         (toggle->alloc->height - lheight) / 2, layout);
#endif
        g_object_unref (layout);
    }
}

static void
toggle_queue_draw (toggle_t *toggle)
{
    GtkAllocation *alloc = toggle->alloc;
    gtk_widget_queue_draw_area (toggle->layout,
                                alloc->x, alloc->y, alloc->width, alloc->height);
}

int
toggle_get_active (toggle_t *toggle)
{
    return toggle->active;
}

void
toggle_set_active (toggle_t *toggle, int active)
{
    if (active != toggle->active)
    {
        toggle->active = active;
        toggle_update_pixmap (toggle);
        toggle_queue_draw (toggle);
        event_fire (toggle, "toggled", NULL, NULL);
    }
}

static gboolean
toggle_button_event (GtkWidget *widget, GdkEventButton *event, toggle_t *toggle)
{
    gboolean handled = FALSE;
    GtkAllocation *alloc = toggle->alloc;
    if ((event->type == GDK_BUTTON_PRESS) && (event->button == 1) &&
        (event->x >= alloc->x) && (event->x < alloc->width + alloc->x) &&
        (event->y >= alloc->y) && (event->y < alloc->height + alloc->y))
    {
        toggle_set_active (toggle, !toggle->active);
        handled = TRUE;
    }

    return handled;
}

static gboolean
toggle_motion_event (GtkWidget *widget, GdkEventMotion *event, toggle_t *toggle)
{
    GtkAllocation area = {event->x, event->y, 1, 1};
    int hover = gdk_rectangle_intersect (&area, toggle->alloc, NULL);
    if (hover != toggle->hover)
    {
        toggle->hover = hover;
        toggle_update_pixmap (toggle);
        toggle_queue_draw (toggle);
    }
    return FALSE;
}

static gboolean
toggle_leave_event (GtkWidget *widget, GdkEventCrossing *event, toggle_t *toggle)
{
    if (toggle->hover != 0)
    {
        toggle->hover = 0;
        toggle_update_pixmap (toggle);
        toggle_queue_draw (toggle);
    }
    return FALSE;
}

void
toggle_allocate (toggle_t *toggle, GtkAllocation * alloc)
{
#if GTK_CHECK_VERSION(3,0,0)
    if (!toggle->cst_surface || (alloc->width != toggle->alloc->width)
#else
    if (!toggle->pixmap || (alloc->width != toggle->alloc->width)
#endif
        || (alloc->height != toggle->alloc->height))
    {
#if GTK_CHECK_VERSION(3,0,0)
        if (toggle->cst_surface)
            cairo_surface_destroy (toggle->cst_surface);
        if (toggle->cst_gradients[0])
            cairo_surface_destroy (toggle->cst_gradients[0]);

        if (toggle->cst_gradients[1])
            cairo_surface_destroy (toggle->cst_gradients[1]);
#else
        if (toggle->pixmap)
            g_object_unref (G_OBJECT (toggle->pixmap));
        if (toggle->gradients[0])
            g_object_unref (G_OBJECT (toggle->gradients[0]));

        if (toggle->gradients[1])
            g_object_unref (G_OBJECT (toggle->gradients[1]));
#endif
        memcpy (toggle->alloc, alloc, sizeof (GtkAllocation));

        if (gtk_layout_get_bin_window (GTK_LAYOUT (toggle->layout)))
        {
#if GTK_CHECK_VERSION(3,0,0)
            toggle->cst_surface = gdk_window_create_similar_surface (gtk_layout_get_bin_window (GTK_LAYOUT (toggle->layout)),
                                             CAIRO_CONTENT_COLOR_ALPHA, alloc->width, alloc->height);
            toggle->cst_gradients[0] = gdk_window_create_similar_surface (gtk_layout_get_bin_window (GTK_LAYOUT (toggle->layout)),
                                             CAIRO_CONTENT_COLOR_ALPHA, alloc->width, alloc->height);
            toggle->cst_gradients[1] = gdk_window_create_similar_surface (gtk_layout_get_bin_window (GTK_LAYOUT (toggle->layout)),
                                             CAIRO_CONTENT_COLOR_ALPHA, alloc->width, alloc->height);
#else
            toggle->pixmap = gdk_pixmap_new (gtk_layout_get_bin_window (GTK_LAYOUT (toggle->layout)),
                                             alloc->width, alloc->height, -1);
            toggle->gradients[0] = gdk_pixmap_new (gtk_layout_get_bin_window (GTK_LAYOUT (toggle->layout)),
                                                   alloc->width, alloc->height, -1);
            toggle->gradients[1] = gdk_pixmap_new (gtk_layout_get_bin_window (GTK_LAYOUT (toggle->layout)),
                                                   alloc->width, alloc->height, -1);
#endif
            toggle_update_gradient (toggle, 0);
            toggle_update_gradient (toggle, 1);
            toggle_update_pixmap (toggle);
        }
    }
}

void
toggle_get_allocation (toggle_t *toggle, GtkAllocation * alloc)
{
    memcpy (alloc, toggle->alloc, sizeof (GtkAllocation));
}

static gboolean
toggle_expose (GtkWidget *layout, GdkEventExpose *event, toggle_t *toggle)
{
    GtkAllocation *alloc = toggle->alloc;

#if GTK_CHECK_VERSION(3,0,0)
    if (toggle->cst_surface && gtk_layout_get_bin_window (GTK_LAYOUT (layout)))
    {
#else
    if (toggle->pixmap && gtk_layout_get_bin_window (GTK_LAYOUT (layout)))
    {
#endif
        GtkAllocation target;
        if (gdk_rectangle_intersect (&event->area, alloc, &target))
        {
            int srcx = target.x - alloc->x;
            int srcy = target.y - alloc->y;
            
            cairo_t *cr = gdk_cairo_create (gtk_layout_get_bin_window(GTK_LAYOUT (layout)));
#if GTK_CHECK_VERSION(3,0,0)
            cairo_set_source_surface (cr, toggle->cst_surface, target.x - srcx, target.y - srcy);
#else
            gdk_cairo_set_source_pixmap(cr, toggle->pixmap, target.x - srcx, target.y - srcy);
#endif
            cairo_rectangle (cr, target.x, target.y, target.width, target.height);
            cairo_clip (cr);
            cairo_paint(cr);
            cairo_destroy(cr);
            
#if 0
            gdk_draw_drawable (
                               gtk_layout_get_bin_window (GTK_LAYOUT (layout)),
                               gtk_widget_get_style(layout)->fg_gc[gtk_widget_get_state (layout)],
                               toggle->pixmap,
                               srcx, srcy,
                               target.x, target.y, target.width, target.height);
#endif
        }
    }
    return FALSE;
}

void
toggle_set_hsv_shift (toggle_t *toggle, dk_hsv_t *shift)
{
    toggle->hsv_shift = *shift;
    //FIXME: redraw
}
