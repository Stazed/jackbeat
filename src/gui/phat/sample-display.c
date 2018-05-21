/*
 * The Real SoundTracker - GTK+ sample display widget
 *
 * Copyright (C) 1998-2000 Michael Krause
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

#include <stdlib.h>

#include "sample-display.h"

#include <string.h>

#include <gtk/gtk.h>

#define XPOS_TO_OFFSET(x) (s->win_start + ((guint64)(x)) * s->win_length / s->width)
#define OFFSET_RANGE(l, x) (x < 0 ? 0 : (x >= l ? l - 1 : x))

static const int default_colors[] = {
    10, 20, 30,
    230, 230, 230,
    230, 200, 0,
    230, 0, 0,
    60, 60, 140,
};

enum
{
    SELECTING_NOTHING = 0,
    SELECTING_SELECTION_START,
    SELECTING_SELECTION_END,
    SELECTING_LOOP_START,
    SELECTING_LOOP_END,
    SELECTING_PAN_WINDOW,
} ;

enum
{
    SIG_SELECTION_CHANGED,
    SIG_LOOP_CHANGED,
    SIG_WINDOW_CHANGED,
    LAST_SIGNAL
} ;

#define IS_INITIALIZED(s) (s->datalen != 0)

static guint sample_display_signals[LAST_SIGNAL] = { 0 };

static int  sample_display_startoffset_to_xpos (SampleDisplay *s,
                                                int offset);
static gint sample_display_idle_draw_function  (SampleDisplay *s);
static void sample_display_draw_update (GtkWidget *widget,
                                        GdkRectangle *area,
                                        gboolean queue_draw);

static void
sample_display_idle_draw (SampleDisplay *s)
{
    if (!s->idle_handler)
    {
        s->idle_handler =  g_idle_add ((GSourceFunc) sample_display_idle_draw_function,
                                        (gpointer) s);
        g_assert (s->idle_handler != 0);
    }
}

void
sample_display_enable_zero_line (SampleDisplay *s,
                                 gboolean enable)
{
    s->display_zero_line = enable;

    if (s->datalen)
    {
        gtk_widget_queue_draw (GTK_WIDGET (s));
    }
}

void
sample_display_set_data (SampleDisplay *s,
                         void *data,
                         SampleDisplayDataType type,
                         int channels,
                         int len,
                         gboolean copy)
{
    int typesize = 0, datacopylen;

    g_return_if_fail (s != NULL);
    g_return_if_fail (IS_SAMPLE_DISPLAY (s));

    gboolean len_changed = ( s->datalen != len );
    if (!data || !len)
    {
        s->datalen = 0;
    }
    else
    {
        if (copy)
        {
            switch (type)
            {
                case SAMPLE_DISPLAY_DATA_INT8: typesize = sizeof (gint8);
                    break;
                case SAMPLE_DISPLAY_DATA_INT16: typesize = sizeof (gint16);
                    break;
                case SAMPLE_DISPLAY_DATA_FLOAT: typesize = sizeof (gfloat);
                    break;
            }

            datacopylen = len * typesize * channels;
            if (s->datacopy)
            {
                if (s->datacopylen != datacopylen)
                {
                    g_free (s->data);
                    s->data = g_malloc (datacopylen);
                    s->datacopylen = datacopylen;
                }
            }
            else
            {
                s->data = g_malloc (datacopylen);
                s->datacopylen = datacopylen;
            }
            g_assert (s->data != NULL);
            memcpy (s->data, data, datacopylen);
            s->datalen = len;
        }
        else
        {
            if (s->datacopy)
            {
                g_free (s->data);
            }
            s->data = data;
            s->datalen = len;
        }
        s->datacopy = copy;
        s->datatype = type;
        s->channels = channels;
    }

    s->old_mixerpos = -1;
    s->mixerpos = -1;

    if ( len_changed )
    {
        s->win_start = 0;
        s->win_length = len;
        g_signal_emit (s, sample_display_signals[SIG_WINDOW_CHANGED], 0);   // FIXME check detail
//        gtk_signal_emit (GTK_OBJECT (s), sample_display_signals[SIG_WINDOW_CHANGED], s->win_start, s->win_start + s->win_length);

        s->sel_start = -1;
        s->old_ss = s->old_se = -1;
        s->selecting = 0;

        s->loop_start = -1;
    }

    gtk_widget_queue_draw (GTK_WIDGET (s));
}

void
G_GNUC_DEPRECATED
sample_display_set_data_16 (SampleDisplay *s,
                            gint16 *data,
                            int len,
                            gboolean copy)
{
    sample_display_set_data (s, data, SAMPLE_DISPLAY_DATA_INT16, 1, len, copy);
}

void
G_GNUC_DEPRECATED
sample_display_set_data_8 (SampleDisplay *s,
                           gint8 *data,
                           int len,
                           gboolean copy)
{
    sample_display_set_data (s, data, SAMPLE_DISPLAY_DATA_INT8, 1, len, copy);
}

void
sample_display_set_loop (SampleDisplay *s,
                         int start,
                         int end)
{
    g_return_if_fail (s != NULL);
    g_return_if_fail (IS_SAMPLE_DISPLAY (s));

    if (!s->edit || !IS_INITIALIZED (s))
        return;

    g_return_if_fail (start >= -1 && start < s->datalen);
    g_return_if_fail (end > 0 && end <= s->datalen);
    g_return_if_fail (end > start);

    s->loop_start = start;
    s->loop_end = end;

    gtk_widget_queue_draw (GTK_WIDGET (s));
    g_signal_emit (GTK_OBJECT (s), sample_display_signals[SIG_LOOP_CHANGED], 0);    // FIXME check detail
//    gtk_signal_emit (GTK_OBJECT (s), sample_display_signals[SIG_LOOP_CHANGED], start, end);
}

void
sample_display_set_selection (SampleDisplay *s,
                              int start,
                              int end)
{
    g_return_if_fail (s != NULL);
    g_return_if_fail (IS_SAMPLE_DISPLAY (s));

    if (!s->edit || !IS_INITIALIZED (s))
        return;

    g_return_if_fail (start >= -1 && start < s->datalen);
    g_return_if_fail (end >= 1 && end <= s->datalen);
    g_return_if_fail (end > start);

    s->sel_start = start;
    s->sel_end = end;

    sample_display_idle_draw (s);
    g_signal_emit (GTK_OBJECT (s), sample_display_signals[SIG_SELECTION_CHANGED], 0);       // FIXME check detail
//    gtk_signal_emit (GTK_OBJECT (s), sample_display_signals[SIG_SELECTION_CHANGED], start, end);
}

void
sample_display_set_mixer_position (SampleDisplay *s,
                                   int offset)
{
    g_return_if_fail (s != NULL);
    g_return_if_fail (IS_SAMPLE_DISPLAY (s));

    if (!IS_INITIALIZED (s))
        return;

    if (offset != s->mixerpos)
    {
        s->mixerpos = offset;
        GdkRectangle area = { 0, 0, s->width, s->height };

        if (gtk_widget_get_mapped (GTK_WIDGET (s)))
        {
            // By passing true to the queue_draw argument, sample_display_draw_update() will call
            // gtk_widget_queue_draw_area(). This provides the best result on Gtk-Quartz (Mac OS X),
            // where sample_display_idle_draw() performed very badly (slow updates). 
            sample_display_draw_update (GTK_WIDGET (s), &area, TRUE);
        }
    }
}

void
sample_display_enable_marker_handles (SampleDisplay *s, gboolean enable)
{
    g_return_if_fail (s != NULL);
    g_return_if_fail (IS_SAMPLE_DISPLAY (s));
    s->display_marker_handles = enable;
    if (s->datalen)
    {
        gtk_widget_queue_draw (GTK_WIDGET (s));
    }
}

void
sample_display_set_window (SampleDisplay *s,
                           int start,
                           int end)
{
    g_return_if_fail (s != NULL);
    g_return_if_fail (IS_SAMPLE_DISPLAY (s));
    g_return_if_fail (start >= 0 && start < s->datalen);
    g_return_if_fail (end > 0 && end <= s->datalen);
    g_return_if_fail (end > start);

    s->win_start = start;
    s->win_length = end - start;
    g_signal_emit (GTK_OBJECT (s), sample_display_signals[SIG_WINDOW_CHANGED], 0);    // FIXME check detail
//    gtk_signal_emit (GTK_OBJECT (s), sample_display_signals[SIG_WINDOW_CHANGED], start, end);

    gtk_widget_queue_draw (GTK_WIDGET (s));
}

static void
sample_display_init_display (SampleDisplay *s,
                             int w,
                             int h)
{
    s->width = w;
    s->height = h;
}

static void
sample_display_size_allocate (GtkWidget *widget,
                              GtkAllocation *allocation)
{
    SampleDisplay *s;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (IS_SAMPLE_DISPLAY (widget));
    g_return_if_fail (allocation != NULL);

    gtk_widget_set_allocation(widget, allocation);
    
    if (gtk_widget_get_realized (widget))
    {
        s = SAMPLE_DISPLAY (widget);

        gdk_window_move_resize (gtk_widget_get_window(widget),
                                allocation->x, allocation->y,
                                allocation->width, allocation->height);

        sample_display_init_display (s, allocation->width, allocation->height);
    }
}

static void
sample_display_realize (GtkWidget *widget)
{
    GdkWindowAttr attributes;
    gint attributes_mask;
    SampleDisplay *s;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (IS_SAMPLE_DISPLAY (widget));

    gtk_widget_set_realized (widget, 1);
    s = SAMPLE_DISPLAY (widget);
    
    GtkAllocation widget_allocation;
    gtk_widget_get_allocation(GTK_WIDGET(widget), &widget_allocation); 

    attributes.x = widget_allocation.x;
    attributes.y = widget_allocation.y;
    attributes.width = widget_allocation.width;
    attributes.height = widget_allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events (widget)
            | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
            | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;

    attributes.visual = gtk_widget_get_visual (widget);
    attributes.colormap = gtk_widget_get_colormap (widget);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    gtk_widget_set_window(widget, gdk_window_new (gtk_widget_get_window(gtk_widget_get_parent(widget)), &attributes, attributes_mask));

    gtk_widget_set_style(widget, gtk_style_attach (gtk_widget_get_style(widget), gtk_widget_get_window(widget)));


    if (s->edit)    // FIXME when is this used
    {
//        s->loop_cr = gdk_cairo_create (gtk_widget_get_window(widget));
//        gdk_cairo_set_source_color (s->loop_cr, &SAMPLE_DISPLAY_CLASS (GTK_OBJECT_GET_CLASS (widget))->colors[SAMPLE_DISPLAYCOL_LOOP]);
//        s->loop_gc = gdk_gc_new (gtk_widget_get_window(widget));
//        gdk_gc_set_foreground (s->loop_gc, &SAMPLE_DISPLAY_CLASS (GTK_OBJECT_GET_CLASS (widget))->colors[SAMPLE_DISPLAYCOL_LOOP]);
    }

    sample_display_init_display (s, attributes.width, attributes.height);

    gdk_window_set_user_data (gtk_widget_get_window(widget), widget);
}

static void
sample_display_size_request (GtkWidget *widget,
                             GtkRequisition *requisition)
{
    requisition->width = 10;
    requisition->height = 32;
}

static void
sample_display_draw_data (GdkDrawable *win,
                          const SampleDisplay *s,
                          int color,
                          int x,
                          int width)
{
    int i;
    gint16 c, d;
    gfloat e, f;
    cairo_t *cr;
    int y_ofs;
    const int pad = (s->height / 20);
    const int sh = (s->height - (s->channels - 1) * pad) / s->channels;

    if (width == 0)
        return;

    g_return_if_fail (x >= 0);
    g_return_if_fail (x + width <= s->width);
    
    cairo_t *win_bg_cr = gdk_cairo_create (win);
    cairo_t *win_fg_cr = gdk_cairo_create (win);
    cairo_t *win_zeroline_cr  = gdk_cairo_create (win);           
    gdk_cairo_set_source_color (win_bg_cr, &SAMPLE_DISPLAY_CLASS (GTK_OBJECT_GET_CLASS (s))->colors[SAMPLE_DISPLAYCOL_BG]);
    gdk_cairo_set_source_color (win_fg_cr, &SAMPLE_DISPLAY_CLASS (GTK_OBJECT_GET_CLASS (s))->colors[SAMPLE_DISPLAYCOL_FG]);
    gdk_cairo_set_source_color (win_zeroline_cr, &SAMPLE_DISPLAY_CLASS (GTK_OBJECT_GET_CLASS (s))->colors[SAMPLE_DISPLAYCOL_ZERO]);

    cairo_rectangle(color ? win_fg_cr : win_bg_cr, x, 0, width, s->height);
    cairo_fill(color ? win_fg_cr : win_bg_cr);
    cairo_stroke(color ? win_fg_cr : win_bg_cr);

    cr = color ? win_bg_cr : win_fg_cr;

    for (i = 0; i < s->channels; i++)
    {
        int _width = width;
        int _x = x;
        y_ofs = i * (sh + pad);
        if (s->display_zero_line)
        {
            cairo_move_to(win_zeroline_cr, _x, y_ofs + (sh / 2));
            cairo_line_to(win_zeroline_cr, _x + _width - 1, y_ofs + (sh / 2));
            cairo_stroke(win_zeroline_cr);
        }

        if (s->datatype == SAMPLE_DISPLAY_DATA_FLOAT)
        {
            e = ((gfloat*) s->data)[OFFSET_RANGE (s->datalen * s->channels, XPOS_TO_OFFSET (_x - 1) * s->channels + i)];
            e = e > 1 ? 1 : (e < -1 ? - 1 : e);

            while (_width >= 0)
            {
                f = ((gfloat*) s->data)[OFFSET_RANGE (s->datalen * s->channels, XPOS_TO_OFFSET (_x) * s->channels + i)];
                f = f > 1 ? 1 : (f < -1 ? - 1 : f);
                
                cairo_move_to (cr, _x - 1, y_ofs + (1 - e) * sh / 2);
                cairo_line_to (cr, _x, y_ofs + (1 - f) * sh / 2 );
                cairo_stroke(cr);
                e = f;
                _x++;
                _width--;
            }
        }
        else if (s->datatype == SAMPLE_DISPLAY_DATA_INT16)
        {
            c = ((gint16*) s->data)[OFFSET_RANGE (s->datalen * s->channels, XPOS_TO_OFFSET (_x - 1) * s->channels + i)];

            while (_width >= 0)
            {
                d = ((gint16*) s->data)[OFFSET_RANGE (s->datalen * s->channels, XPOS_TO_OFFSET (_x) * s->channels + i)];
                cairo_move_to (cr, _x - 1, y_ofs + (((32768 - c) * sh) >> 16));
                cairo_line_to (cr, _x,     y_ofs + (((32768 - d) * sh) >> 16));
                cairo_stroke(cr);
                c = d;
                _x++;
                _width--;
            }
        }
        else
        {
            c = ((gint8*) s->data)[OFFSET_RANGE (s->datalen * s->channels, XPOS_TO_OFFSET (_x - 1) * s->channels + i)];

            while (_width >= 0)
            {
                d = ((gint8*) s->data)[OFFSET_RANGE (s->datalen * s->channels, XPOS_TO_OFFSET (_x) * s->channels + i)];
                cairo_move_to (cr, _x - 1, y_ofs + (((128 - c) * sh) >> 8));
                cairo_line_to (cr, _x,     y_ofs + (((128 - d) * sh) >> 8));
                cairo_stroke(cr);
                c = d;
                _x++;
                _width--;
            }
        }
    }

    cairo_destroy(win_bg_cr);
    cairo_destroy(win_fg_cr);
    cairo_destroy(win_zeroline_cr);
}

static int
sample_display_startoffset_to_xpos (SampleDisplay *s,
                                    int offset)
{
    gint64 d = offset - s->win_start;

    if (d < 0)
        return 0;
    if (d >= s->win_length)
        return s->width;

    return d * s->width / s->win_length;
}

static int
sample_display_endoffset_to_xpos (SampleDisplay *s,
                                  int offset)
{
    if (s->win_length < s->width)
    {
        return sample_display_startoffset_to_xpos (s, offset);
    }
    else
    {
        gint64 d = offset - s->win_start;
        int l = (1 - s->win_length) / s->width;

        /* you get these tests by setting the complete formula below equal to 0 or s->width, respectively,
           and then resolving towards d. */
        if (d < l)
            return 0;
        if (d > s->win_length + l)
            return s->width;

        return (d * s->width + s->win_length - 1) / s->win_length;
    }
}

static void
sample_display_do_marker_line (GdkDrawable *win,
                               SampleDisplay *s,
                               int endoffset,
                               int offset,
                               int x_min,
                               int x_max,
                               int mixerpos)
{
    cairo_t *win_cr = gdk_cairo_create (win);
    
    if(mixerpos)
        gdk_cairo_set_source_color (win_cr, &SAMPLE_DISPLAY_CLASS (GTK_OBJECT_GET_CLASS (s))->colors[SAMPLE_DISPLAYCOL_MIXERPOS]);
    else
        gdk_cairo_set_source_color (win_cr, &SAMPLE_DISPLAY_CLASS (GTK_OBJECT_GET_CLASS (s))->colors[SAMPLE_DISPLAYCOL_LOOP]);
        
    int x;

    if (offset >= s->win_start && offset <= s->win_start + s->win_length)
    {
        if (!endoffset)
            x = sample_display_startoffset_to_xpos (s, offset);
        else
            x = sample_display_endoffset_to_xpos (s, offset);
        if (x + 3 >= x_min && x - 3 < x_max)
        {
            cairo_move_to(win_cr, x, 0);
            cairo_line_to(win_cr, x, s->height);
            cairo_stroke(win_cr);

            if (s->display_marker_handles)
            {
                cairo_rectangle(win_cr, x - 3, 0, 7, 10);
                cairo_fill(win_cr);
                cairo_stroke(win_cr);
                
                cairo_rectangle(win_cr, x - 3, s->height - 10, 7, 10);
                cairo_fill(win_cr);
                cairo_stroke(win_cr);
            }
        }
    }
    cairo_destroy(win_cr);
}

static void
sample_display_draw_main (GtkWidget *widget,
                          GdkRectangle *area,
                          gboolean queue_draw)
{
    if (queue_draw)
    {
        gtk_widget_queue_draw_area (widget, area->x, area->y, area->width, area->height);
        return;
    }
    
    SampleDisplay *s = SAMPLE_DISPLAY (widget);
    int x, x2;

    g_return_if_fail (area->x >= 0);

    if (area->width == 0)
        return;

    if (area->x + area->width > s->width)
        return;

    if (!IS_INITIALIZED (s))
    {
        cairo_t *win_bg_cr = gdk_cairo_create (gtk_widget_get_window(GTK_WIDGET(widget)));
        cairo_t *win_fg_cr = gdk_cairo_create (gtk_widget_get_window(GTK_WIDGET(widget)));

        gdk_cairo_set_source_color (win_bg_cr, &SAMPLE_DISPLAY_CLASS (GTK_OBJECT_GET_CLASS (s))->colors[SAMPLE_DISPLAYCOL_BG]);
        gdk_cairo_set_source_color (win_fg_cr, &SAMPLE_DISPLAY_CLASS (GTK_OBJECT_GET_CLASS (s))->colors[SAMPLE_DISPLAYCOL_FG]);
        
        cairo_rectangle (win_bg_cr, area->x, area->y, area->width, area->height);
        cairo_stroke(win_bg_cr);
        
        cairo_move_to (win_fg_cr, area->x, s->height / 2);
        cairo_line_to (win_fg_cr, area->x + area->width - 1, s->height / 2);
        cairo_stroke(win_bg_cr);
        
        cairo_destroy(win_bg_cr);
        cairo_destroy(win_fg_cr);
    }
    else
    {
        const int x_min = area->x;
        const int x_max = area->x + area->width;

        if (s->sel_start != -1)
        {
            /* draw the part to the left of the selection */
            x = sample_display_startoffset_to_xpos (s, s->sel_start);
            x = MIN (x_max, MAX (x_min, x));
            sample_display_draw_data (gtk_widget_get_window(widget), s, 0, x_min, x - x_min);

            /* draw the selection */
            x2 = sample_display_endoffset_to_xpos (s, s->sel_end);
            x2 = MIN (x_max, MAX (x_min, x2));
            sample_display_draw_data (gtk_widget_get_window(widget), s, 1, x, x2 - x);
        }
        else
        {
            /* we don't have a selection */
            x2 = x_min;
        }

        /* draw the part to the right of the selection */
        sample_display_draw_data (gtk_widget_get_window(widget), s, 0, x2, x_max - x2);

        if (s->loop_start != -1)
        {
            sample_display_do_marker_line (gtk_widget_get_window(widget), s, 0, s->loop_start, x_min, x_max, FALSE);
            sample_display_do_marker_line (gtk_widget_get_window(widget), s, 1, s->loop_end, x_min, x_max, FALSE);
        }

        if (s->mixerpos != -1)
        {
            sample_display_do_marker_line (gtk_widget_get_window(widget), s, 0, s->mixerpos, x_min, x_max, TRUE);
            s->old_mixerpos = s->mixerpos;
        }
    }
}

static void
sample_display_draw_update (GtkWidget *widget,
                            GdkRectangle *area,
                            gboolean queue_draw)
{
    SampleDisplay *s = SAMPLE_DISPLAY (widget);
    GdkRectangle area2 = { 0, 0, 0, s->height };
    int x, i;
    const int x_min = area->x;
    const int x_max = area->x + area->width;
    gboolean special_draw = FALSE;

    if (s->mixerpos != s->old_mixerpos)
    {
        /* Redraw area of old position, redraw area of new position. */
        for (i = 0; i < 2; i++)
        {
            if (s->old_mixerpos >= s->win_start && s->old_mixerpos < s->win_start + s->win_length)
            {
                x = sample_display_startoffset_to_xpos (s, s->old_mixerpos);
                area2.x = MIN (x_max - 1, MAX (x_min, x - 3));
                area2.width = 7;
                if (area2.x + area2.width > x_max)
                {
                    area2.width = x_max - area2.x;
                }
                sample_display_draw_main (widget, &area2, queue_draw);
            }
            s->old_mixerpos = s->mixerpos;
        }
        special_draw = TRUE;
    }

    if (s->sel_start != s->old_ss || s->sel_end != s->old_se)
    {
        if (s->sel_start == -1 || s->old_ss == -1)
        {
            sample_display_draw_main (widget, area, queue_draw);
        }
        else
        {
            if (s->sel_start < s->old_ss)
            {
                /* repaint left additional side */
                x = sample_display_startoffset_to_xpos (s, s->sel_start);
                area2.x = MIN (x_max, MAX (x_min, x));
                x = sample_display_startoffset_to_xpos (s, s->old_ss);
                area2.width = MIN (x_max, MAX (x_min, x)) - area2.x;
            }
            else
            {
                /* repaint left removed side */
                x = sample_display_startoffset_to_xpos (s, s->old_ss);
                area2.x = MIN (x_max, MAX (x_min, x));
                x = sample_display_startoffset_to_xpos (s, s->sel_start);
                area2.width = MIN (x_max, MAX (x_min, x)) - area2.x;
            }
            sample_display_draw_main (widget, &area2, queue_draw);

            if (s->sel_end < s->old_se)
            {
                /* repaint right removed side */
                x = sample_display_endoffset_to_xpos (s, s->sel_end);
                area2.x = MIN (x_max, MAX (x_min, x));
                x = sample_display_endoffset_to_xpos (s, s->old_se);
                area2.width = MIN (x_max, MAX (x_min, x)) - area2.x;
            }
            else
            {
                /* repaint right additional side */
                x = sample_display_endoffset_to_xpos (s, s->old_se);
                area2.x = MIN (x_max, MAX (x_min, x));
                x = sample_display_endoffset_to_xpos (s, s->sel_end);
                area2.width = MIN (x_max, MAX (x_min, x)) - area2.x;
            }
            sample_display_draw_main (widget, &area2, queue_draw);
        }

        s->old_ss = s->sel_start;
        s->old_se = s->sel_end;
        special_draw = TRUE;
    }

    if (!special_draw)
    {
        sample_display_draw_main (widget, area, queue_draw);
    }
}

static gint
sample_display_expose (GtkWidget *widget,
                       GdkEventExpose *event)
{
    sample_display_draw_main (widget, &event->area, FALSE);
    return FALSE;
}

static gint
sample_display_idle_draw_function (SampleDisplay *s)
{
    GdkRectangle area = { 0, 0, s->width, s->height };

    if (gtk_widget_get_mapped (GTK_WIDGET (s)))
    {
        sample_display_draw_update (GTK_WIDGET (s), &area, FALSE);
    }

    g_idle_remove_by_data ( (gpointer) s);
//    gtk_idle_remove (s->idle_handler);
    s->idle_handler = 0;    // FIXME check this - we removed s by data above, so s should be gone.
    return TRUE;
}

static void
sample_display_handle_motion (SampleDisplay *s,
                              int x,
                              int y,
                              int just_clicked)
{
    int ol, or;
    int ss = s->sel_start, se = s->sel_end;
    int ls = s->loop_start, le = s->loop_end;

    if (!s->selecting)
        return;

    if (x < 0)
        x = 0;
    else if (x >= s->width)
        x = s->width - 1;

    ol = XPOS_TO_OFFSET (x);
    if (s->win_length < s->width)
    {
        or = XPOS_TO_OFFSET (x) + 1;
    }
    else
    {
        or = XPOS_TO_OFFSET (x + 1);
    }

    g_return_if_fail (ol >= 0 && ol < s->datalen);
    g_return_if_fail (or > 0 && or <= s->datalen);
    g_return_if_fail (ol < or);

    switch (s->selecting)
    {
        case SELECTING_SELECTION_START:
            if (just_clicked)
            {
                if (ss != -1 && ol < se)
                {
                    ss = ol;
                }
                else
                {
                    ss = ol;
                    se = ol + 1;
                }
            }
            else
            {
                if (ol < se)
                {
                    ss = ol;
                }
                else
                {
                    ss = se - 1;
                    se = or;
                    s->selecting = SELECTING_SELECTION_END;
                }
            }
            break;
        case SELECTING_SELECTION_END:
            if (just_clicked)
            {
                if (ss != -1 && or > ss)
                {
                    se = or;
                }
                else
                {
                    ss = or - 1;
                    se = or;
                }
            }
            else
            {
                if (or > ss)
                {
                    se = or;
                }
                else
                {
                    se = ss + 1;
                    ss = ol;
                    s->selecting = SELECTING_SELECTION_START;
                }
            }
            break;
        case SELECTING_LOOP_START:
            if (ol < s->loop_end)
                ls = ol;
            else
                ls = le - 1;
            break;
        case SELECTING_LOOP_END:
            if (or > s->loop_start)
                le = or;
            else
                le = ls + 1;
            break;
        default:
            g_assert_not_reached ();
            break;
    }

    if (s->sel_start != ss || s->sel_end != se)
    {
        s->sel_start = ss;
        s->sel_end = se;
        sample_display_idle_draw (s);
        g_signal_emit (GTK_OBJECT (s), sample_display_signals[SIG_SELECTION_CHANGED], 0);    // FIXME check detail
//        gtk_signal_emit (GTK_OBJECT (s), sample_display_signals[SIG_SELECTION_CHANGED], ss, se);
    }

    if (s->loop_start != ls || s->loop_end != le)
    {
        s->loop_start = ls;
        s->loop_end = le;
        sample_display_idle_draw (s);
        g_signal_emit (GTK_OBJECT (s), sample_display_signals[SIG_LOOP_CHANGED], 0);    // FIXME check detail
//        gtk_signal_emit (GTK_OBJECT (s), sample_display_signals[SIG_LOOP_CHANGED], ls, le);
    }
}

// Handle middle mousebutton display window panning

static void
sample_display_handle_motion_2 (SampleDisplay *s,
                                int x,
                                int y)
{
    int new_win_start = s->selecting_wins0 + (s->selecting_x0 - x) * s->win_length / s->width;

    new_win_start = CLAMP (new_win_start, 0, s->datalen - s->win_length);

    if (new_win_start != s->win_start)
    {
        sample_display_set_window (s,
                                   new_win_start,
                                   new_win_start + s->win_length);
    }
}

static gint
sample_display_button_press (GtkWidget      *widget,
                             GdkEventButton *event)
{
    SampleDisplay *s;
    int x, y;
    GdkModifierType state;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_SAMPLE_DISPLAY (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    s = SAMPLE_DISPLAY (widget);

    if (!s->edit)
        return FALSE;

    if (!IS_INITIALIZED (s))
        return TRUE;

    if (s->selecting && event->button != s->button)
    {
        s->selecting = 0;
    }
    else
    {
        s->button = event->button;
        gdk_window_get_pointer (event->window, &x, &y, &state);
        if (!(state & GDK_SHIFT_MASK))
        {
            if (s->button == 1)
            {
                s->selecting = SELECTING_SELECTION_START;
            }
            else if (s->button == 2)
            {
                s->selecting = SELECTING_PAN_WINDOW;
                gdk_window_get_pointer (event->window, &s->selecting_x0, NULL, NULL);
                s->selecting_wins0 = s->win_start;
            }
            else if (s->button == 3)
            {
                s->selecting = SELECTING_SELECTION_END;
            }
        }
        else
        {
            if (s->loop_start != -1)
            {
                if (s->button == 1)
                {
                    s->selecting = SELECTING_LOOP_START;
                }
                else if (s->button == 3)
                {
                    s->selecting = SELECTING_LOOP_END;
                }
            }
        }
        if (!s->selecting)
            return TRUE;
        if (s->selecting != SELECTING_PAN_WINDOW)
            sample_display_handle_motion (s, x, y, 1);
    }

    return TRUE;
}

static gint
sample_display_button_release (GtkWidget      *widget,
                               GdkEventButton *event)
{
    SampleDisplay *s;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_SAMPLE_DISPLAY (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    s = SAMPLE_DISPLAY (widget);

    if (!s->edit)
        return FALSE;

    if (s->selecting && event->button == s->button)
    {
        s->selecting = 0;
    }

    return TRUE;
}

static gint
sample_display_motion_notify (GtkWidget *widget,
                              GdkEventMotion *event)
{
    SampleDisplay *s;
    int x, y;
    GdkModifierType state;

    s = SAMPLE_DISPLAY (widget);

    if (!s->edit)
        return FALSE;

    if (!IS_INITIALIZED (s) || !s->selecting)
        return TRUE;

    if (event->is_hint)
    {
        gdk_window_get_pointer (event->window, &x, &y, &state);
    }
    else
    {
        x = event->x;
        y = event->y;
        state = event->state;
    }

    if (((state & GDK_BUTTON1_MASK) && s->button == 1) || ((state & GDK_BUTTON3_MASK) && s->button == 3))
    {
        sample_display_handle_motion (SAMPLE_DISPLAY (widget), x, y, 0);
    }
    else if ((state & GDK_BUTTON2_MASK) && s->button == 2)
    {
        sample_display_handle_motion_2 (SAMPLE_DISPLAY (widget), x, y);
    }
    else
    {
        s->selecting = 0;
    }

    return TRUE;
}

static void
sample_display_class_init (SampleDisplayClass *class)
{
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;
    int n;
    const int *p;
    GdkColor *c;

    object_class = (GtkObjectClass*) class;
    widget_class = (GtkWidgetClass*) class;

    widget_class->realize = sample_display_realize;
    widget_class->size_allocate = sample_display_size_allocate;
    widget_class->expose_event = sample_display_expose;
    //widget_class->draw = sample_display_draw;
    widget_class->size_request = sample_display_size_request;
    widget_class->button_press_event = sample_display_button_press;
    widget_class->button_release_event = sample_display_button_release;
    widget_class->motion_notify_event = sample_display_motion_notify;

    sample_display_signals[SIG_SELECTION_CHANGED] = g_signal_new ("selection_changed",
                                                                  G_TYPE_FROM_CLASS (object_class),
                                                                  G_SIGNAL_RUN_FIRST,
                                                                  G_STRUCT_OFFSET (SampleDisplayClass, selection_changed),
                                                                  NULL, NULL,
                                                                  g_cclosure_marshal_VOID__INT,     // FIXME MAYBE
                                                                  G_TYPE_NONE, 1,
                                                                  G_TYPE_INT);
#if 0
    sample_display_signals[SIG_SELECTION_CHANGED] = gtk_signal_new ("selection_changed",
                                                                    GTK_RUN_FIRST,
                                                                    G_TYPE_FROM_CLASS (object_class),
                                                                    G_STRUCT_OFFSET (SampleDisplayClass, selection_changed),
                                                                    gtk_marshal_NONE__INT_INT,
                                                                    GTK_TYPE_NONE, 2,
                                                                    GTK_TYPE_INT,
                                                                    GTK_TYPE_INT);
#endif
    sample_display_signals[SIG_LOOP_CHANGED] = g_signal_new ("loop_changed",
                                                             G_TYPE_FROM_CLASS (object_class),
                                                             G_SIGNAL_RUN_FIRST,
                                                             G_STRUCT_OFFSET (SampleDisplayClass, loop_changed),
                                                             NULL, NULL,
                                                             g_cclosure_marshal_VOID__INT,     // FIXME MAYBE
                                                             G_TYPE_NONE, 1,
                                                             G_TYPE_INT);
#if 0
    sample_display_signals[SIG_LOOP_CHANGED] = gtk_signal_new ("loop_changed",
                                                               GTK_RUN_FIRST,
                                                               G_TYPE_FROM_CLASS (object_class),
                                                               G_STRUCT_OFFSET (SampleDisplayClass, loop_changed),
                                                               gtk_marshal_NONE__INT_INT,
                                                               GTK_TYPE_NONE, 2,
                                                               GTK_TYPE_INT,
                                                               GTK_TYPE_INT);
#endif
    sample_display_signals[SIG_WINDOW_CHANGED] = g_signal_new ("window_changed",
                                                               G_TYPE_FROM_CLASS (object_class),
                                                               G_SIGNAL_RUN_FIRST,
                                                               G_STRUCT_OFFSET (SampleDisplayClass, window_changed),
                                                               NULL, NULL,
                                                               g_cclosure_marshal_VOID__INT,     // FIXME MAYBE
                                                               G_TYPE_NONE, 1,
                                                               G_TYPE_INT);
#if 0
    sample_display_signals[SIG_WINDOW_CHANGED] = gtk_signal_new ("window_changed",
                                                                 GTK_RUN_FIRST,
                                                                 G_TYPE_FROM_CLASS (object_class),
                                                                 G_STRUCT_OFFSET (SampleDisplayClass, window_changed),
                                                                 gtk_marshal_NONE__INT_INT,
                                                                 GTK_TYPE_NONE, 2,
                                                                 GTK_TYPE_INT,
                                                                 GTK_TYPE_INT);
#endif
    //gtk_object_class_add_signals(object_class, sample_display_signals, LAST_SIGNAL);

    class->selection_changed = NULL;
    class->loop_changed = NULL;
    class->window_changed = NULL;

    for (n = 0, p = default_colors, c = class->colors; n < SAMPLE_DISPLAYCOL_LAST; n++, c++)
    {
        c->red = *p++ * 65535 / 255;
        c->green = *p++ * 65535 / 255;
        c->blue = *p++ * 65535 / 255;
        c->pixel = (gulong) ((c->red & 0xff00)*256 + (c->green & 0xff00) + (c->blue & 0xff00) / 256);
        gdk_color_alloc (gdk_colormap_get_system (), c);
    }
}

static void
sample_display_init (SampleDisplay *s)
{
    s->idle_handler = 0;
}

GType
sample_display_get_type (void)
{
    static GType sample_display_type = 0;

    if (!sample_display_type)
    {
        GTypeInfo sample_display_info ={
            sizeof (SampleDisplayClass),
            NULL,   /* base_init */
            NULL,   /* base_finalize */
            (GClassInitFunc) sample_display_class_init,
            NULL,   /* class_finalize */
            NULL,   /* class_data */
            sizeof (SampleDisplay),
            0,
            (GInstanceInitFunc) sample_display_init
        };
        
        sample_display_type = g_type_register_static(gtk_widget_get_type (), "SampleDisplay", &sample_display_info, 0);
    }

    return sample_display_type;
}

GtkWidget*
sample_display_new (gboolean edit)
{
    SampleDisplay *s = SAMPLE_DISPLAY (g_object_new (sample_display_get_type (), NULL));

    s->edit = edit;
    s->datacopy = 0;
    s->display_zero_line = FALSE;
    s->display_marker_handles = TRUE;
    return GTK_WIDGET (s);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
