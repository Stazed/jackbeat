/* 
 *
 * Most of this code comes from gAlan 0.2.0, copyright (C) 1999
 * Tony Garnock-Jones, with modifications by Sean Bolton,
 * copyright (c) 2004.  (gtkdial.c rolls over in its grave.)
 *
 * Phatised by Loki Davison.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307, USA.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include "gui/phat/phatknob.h"
#include "gui/phat/phatrange.h"
#include "util.h"
#include "gui/dk.h"

#define SCROLL_DELAY_LENGTH     100
#define KNOB_SIZE               50

enum
{
    STATE_IDLE,
    STATE_PRESSED,
    STATE_DRAGGING,
    STATE_SCROLL,
} ;

/* properties */
enum
{
    PROP_0, /* oops, no props any more */
} ;

static void phat_knob_class_init         (PhatKnobClass *klass);
static void phat_knob_init               (PhatKnob *knob);
static void phat_knob_destroy            (GtkObject *object);
static void phat_knob_realize            (GtkWidget *widget);
static void phat_knob_size_request       (GtkWidget *widget,
                                          GtkRequisition *requisition);
static gint phat_knob_expose             (GtkWidget *widget,
                                          GdkEventExpose *event);
static gint phat_knob_button_press       (GtkWidget *widget,
                                          GdkEventButton *event);
static gint phat_knob_button_release     (GtkWidget *widget,
                                          GdkEventButton *event);
static gint phat_knob_motion_notify      (GtkWidget *widget,
                                          GdkEventMotion *event);
static gint phat_knob_scroll             (GtkWidget *widget,
                                          GdkEventScroll *event);
static void phat_knob_update_mouse       (PhatKnob *knob,
                                          gint x,
                                          gint y,
                                          gboolean absolute);


static void phat_knob_set_property      (GObject *object,
                                         guint prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec);
static void phat_knob_get_property      (GObject *object,
                                         guint prop_id,
                                         GValue *value,
                                         GParamSpec *pspec);

GError *gerror;

/* global pixbufs for less mem usage */
GdkPixbuf **pixbuf = NULL;

/* Local data */

G_DEFINE_TYPE (PhatKnob, phat_knob, PHAT_TYPE_RANGE);

static void
phat_knob_class_init (PhatKnobClass *klass)
{
    GtkObjectClass   *object_class;
    GtkWidgetClass   *widget_class;
    GObjectClass     *g_object_class;

    object_class =   GTK_OBJECT_CLASS (klass);
    widget_class =   GTK_WIDGET_CLASS (klass);
    g_object_class = G_OBJECT_CLASS (klass);

    g_object_class->set_property = phat_knob_set_property;
    g_object_class->get_property = phat_knob_get_property;

    object_class->destroy =        phat_knob_destroy;

    widget_class->realize =        phat_knob_realize;
    widget_class->expose_event =   phat_knob_expose;
    widget_class->size_request =   phat_knob_size_request;
    widget_class->button_press_event = phat_knob_button_press;
    widget_class->button_release_event = phat_knob_button_release;
    widget_class->motion_notify_event = phat_knob_motion_notify;
    widget_class->scroll_event =   phat_knob_scroll;

}

static void
phat_knob_init (PhatKnob *knob)
{
    knob->state = STATE_IDLE;
    knob->saved_x = knob->saved_y = 0;
    knob->size = KNOB_SIZE;
    knob->pixbuf = NULL;
}

/**
 * phat_knob_new:
 * @adjustment: a #GtkAdjustment or NULL
 *
 * Creates a new #PhatKnob with the supplied
 * #GtkAdjustment. if adjustment is NULL or
 * invalid, a new #GtkAdjustment will be created
 * with the default values of:
 * 0.0, 0.0, 10.0, 0.1, 0.1, 0.2
 * 
 * Returns: a newly created #PhatKnob
 * 
 */
GtkWidget *
phat_knob_new (GtkAdjustment *adjustment)
{

    return g_object_new (PHAT_TYPE_KNOB,
                         "adjustment",
                         adjustment,
                         NULL);
}

/**
 * phat_knob_new_with_range:
 * @value: the initial value the new knob should have
 * @lower: the lowest value the new knob will allow
 * @upper: the highest value the new knob will allow
 * @step: increment added or subtracted when turning
 *
 * Creates a new #PhatKnob.  The knob will create a new
 * #GtkAdjustment from @value, @lower, @upper, and @step.  If these
 * parameters represent a bogus configuration, the program will
 * terminate.
 *
 * Returns: a newly created #PhatKnob
 * 
 */
GtkWidget*
phat_knob_new_with_range (double value, double lower,
                          double upper, double step)
{
    GtkAdjustment* adj;

    adj = (GtkAdjustment*) gtk_adjustment_new (value, lower, upper, step, step, 0);

    /*adj->step_increment=step;*/

    return phat_knob_new (adj);
}

/**
 * phat_knob_set_value:
 * @knob: a #PhatKnob
 * @value: a new value for the knob
 * 
 * Sets the current value of the knob.  If the value is outside the
 * range of values allowed by @knob, it will be clamped.  The knob
 * emits the "value-changed" signal if the value changes.
 *
 */
void
phat_knob_set_value (PhatKnob* knob, double value)
{
    g_return_if_fail (PHAT_IS_KNOB (knob));

    phat_range_set_value ( PHAT_RANGE ( knob ), value );
}

/**
 * phat_knob_get_value:
 * @knob: a #PhatKnob
 *
 * Retrieves the current value of the knob.
 *
 * Returns: current value of the knob
 *
 */
double
phat_knob_get_value (PhatKnob* knob)
{
    g_return_val_if_fail (PHAT_IS_KNOB (knob), 0);

    return phat_range_get_value ( PHAT_RANGE ( knob ) );

}

#if 0

/**
 * phat_knob_set_range:
 * @knob: a #PhatKnob
 * @lower: lowest allowable value
 * @upper: highest allowable value
 * 
 * Sets the range of allowable values for the knob, and clamps the
 * knob's current value to be between @lower and @upper.
 */
void
phat_knob_set_range (PhatKnob* knob,
                     double lower, double upper)
{
    double value;

    g_return_if_fail (PHAT_IS_KNOB (knob));
    g_return_if_fail (lower <= upper);

    knob->adjustment->lower = lower;
    knob->adjustment->upper = upper;

    value = CLAMP (knob->adjustment->value,
                   knob->adjustment->lower,
                   knob->adjustment->upper);

    gtk_adjustment_changed (knob->adjustment);
    gtk_adjustment_set_value (knob->adjustment, value);
}
#endif

// XXX: this needs reimplementing when the mapper is done.

void
phat_knob_set_log (PhatKnob* knob, gboolean is_log)
{
    g_return_if_fail (PHAT_IS_KNOB (knob));

    knob->is_log = is_log;
}

gboolean
phat_knob_is_log (PhatKnob* knob)
{
    g_return_val_if_fail (PHAT_IS_KNOB (knob), 0);

    return knob->is_log;
}

static void
phat_knob_destroy (GtkObject *object)
{
    PhatKnob *knob;

    g_return_if_fail (object != NULL);
    g_return_if_fail (PHAT_IS_KNOB (object));

    knob = PHAT_KNOB (object);

    if (knob->mask)
    {
        gdk_bitmap_unref (knob->mask);
        knob->mask = NULL;
    }

    if (knob->mask_gc)
    {
        gdk_gc_unref (knob->mask_gc);
        knob->mask_gc = NULL;
    }
    if (knob->red_gc)
    {
        gdk_gc_unref (knob->red_gc);
        knob->red_gc = NULL;
    }

    if (GTK_OBJECT_CLASS (phat_knob_parent_class)->destroy)
        (*GTK_OBJECT_CLASS (phat_knob_parent_class)->destroy)(object);
}

GtkAdjustment*
phat_knob_get_adjustment (PhatKnob *knob)
{
    g_return_val_if_fail (knob != NULL, NULL);
    g_return_val_if_fail (PHAT_IS_KNOB (knob), NULL);

    return phat_range_get_adjustment ( PHAT_RANGE ( knob ) );
}

void
phat_knob_set_adjustment (PhatKnob *knob, GtkAdjustment *adjustment)
{
    g_return_if_fail (knob != NULL);
    g_return_if_fail (PHAT_IS_KNOB (knob));

    phat_range_set_adjustment ( PHAT_RANGE ( knob ), adjustment );
}

static void
phat_knob_realize (GtkWidget *widget)
{
    PhatKnob *knob;
    extern GdkPixbuf **pixbuf;
    int i = 0;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (PHAT_IS_KNOB (widget));

    if (GTK_WIDGET_CLASS (phat_knob_parent_class)->realize)
        (*GTK_WIDGET_CLASS (phat_knob_parent_class)->realize)(widget);

    knob = PHAT_KNOB (widget);

    /* FIXME keeps khagan from drawing knob */
    if (widget->allocation.height > 1)
    {
        knob->size = widget->allocation.height;
    }

    /* init first pixbuf */
    if (pixbuf == NULL)
    {
        pixbuf = g_malloc0 (sizeof (GdkPixbuf *));
    }
    /* check for fitting pixbuf or NULL */
    while (pixbuf[i] != NULL && gdk_pixbuf_get_height (pixbuf[i]) != knob->size)
    {
        i++;
    }
    /* if NULL realloc pixbuf pointer array one bigger
     * malloc new pixbuf with new size
     * set local pixbuf pointer to global
     * set last pixbuf pointer to NULL */
    if (pixbuf[i] == NULL)
    {
        pixbuf[i] = gdk_pixbuf_new_from_file_at_size (
                                                      util_pkgdata_path ("pixmaps/knob.png"),
                                                      52 * knob->size, knob->size, &gerror);
        knob->pixbuf = pixbuf[i];
        pixbuf = g_realloc (pixbuf, sizeof (GdkPixbuf *) * (i + 2));
        pixbuf[i + 1] = NULL;
    }
    else
    { /* if not NULL set fitting pixbuf */
        knob->pixbuf = pixbuf[i];
    }
}

static void
phat_knob_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
    PhatKnob *knob;

    knob = PHAT_KNOB (widget);
    requisition->width = knob->size;
    requisition->height = knob->size;
}

static void
draw_arc (GdkDrawable *drawable, dk_color_t *color, GdkGC *gc, GtkAllocation *alloc, float value, int step)
{
    if (color)
        gc = dk_make_gc (drawable, color);

    gdk_draw_arc (drawable, gc, TRUE, alloc->x + step, alloc->y + step,
                  alloc->width - 2 * step, alloc->height - 2 * step,
                  225 * 64, -270 * 64 * value);
    if (color)
        g_object_unref (G_OBJECT (gc));
}

static void
draw_knob (GdkDrawable *drawable, GdkGC *bg_gc, GtkAllocation *alloc, float value)
{
    dk_color_t from;
    GdkGCValues gc_values;
    gdk_gc_get_values (bg_gc, &gc_values);
    GdkColormap *colormap = gdk_colormap_get_system ();
    GdkColor bg_color;
    gdk_colormap_query_color (colormap, gc_values.foreground.pixel, &bg_color);
    from.red = bg_color.red;
    from.green = bg_color.green;
    from.blue = bg_color.blue;
    dk_color_t to = {0x68, 0xf3, 0xf8};
    GdkGC * gradient[5];

    dk_make_gradient (gradient, drawable, &from, &to, 5);
    gdk_draw_rectangle (drawable, bg_gc, TRUE, alloc->x, alloc->y, alloc->width, alloc->height);

    draw_arc (drawable, NULL, gradient[1], alloc, value, 3);
    draw_arc (drawable, NULL, gradient[2], alloc, value, 4);
    draw_arc (drawable, NULL, gradient[3], alloc, value, 5);
    draw_arc (drawable, NULL, gradient[4], alloc, value, 6);
    draw_arc (drawable, NULL, gradient[3], alloc, value, 8);
    draw_arc (drawable, NULL, gradient[2], alloc, value, 9);
    draw_arc (drawable, NULL, gradient[1], alloc, value, 10);
    draw_arc (drawable, NULL, bg_gc, alloc, value, 11);

    int i;
    for (i = 0; i < 5; i++)
        g_object_unref (G_OBJECT (gradient[i]));

}

static gint
phat_knob_expose (GtkWidget *widget, GdkEventExpose *event)
{
    PhatKnob *knob;
    PhatRange *range;
    int dx;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (PHAT_IS_KNOB (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    if (event->count > 0)
        return FALSE;

    knob = PHAT_KNOB (widget);
    range = PHAT_RANGE (widget);

    dx = (int) (51 * phat_range_get_internal_value (range)) * knob->size;
    gdk_pixbuf_render_to_drawable_alpha ( knob->pixbuf, widget->window,
                                         dx, 0, widget->allocation.x, widget->allocation.y,
                                         knob->size, knob->size, GDK_PIXBUF_ALPHA_FULL, 0, 0, 0, 0 );
    /*
        draw_knob (widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], 
                   &widget->allocation, phat_range_get_internal_value(range));
     */
    //    gdk_draw_pixbuf(widget->window, knob->mask_gc, knob->pixbuf,
    //                    dx, 0, 0, 0, knob->size, knob->size,GDK_RGB_DITHER_NONE,0,0);

    return FALSE;
}

static gint
phat_knob_button_press (GtkWidget *widget, GdkEventButton *event)
{
    PhatKnob *knob;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (PHAT_IS_KNOB (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    knob = PHAT_KNOB (widget);

    switch (knob->state)
    {
        case STATE_IDLE:
            switch (event->button)
            {
                case 1:
                case 2:
                    gtk_grab_add (widget);
                    knob->state = STATE_PRESSED;
                    knob->saved_x = event->x;
                    knob->saved_y = event->y;
                    break;

                default:
                    break;
            }
            break;

        default:
            break;
    }

    return FALSE;
}

static gint
phat_knob_button_release (GtkWidget *widget, GdkEventButton *event)
{
    PhatKnob *knob;
    PhatRange *range;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (PHAT_IS_KNOB (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    knob = PHAT_KNOB (widget);
    range = PHAT_RANGE (widget);

    switch (knob->state)
    {
        case STATE_PRESSED:
            gtk_grab_remove (widget);
            knob->state = STATE_IDLE;

            switch (event->button)
            {
                case 1:
                    phat_range_page_up (range);
                    break;

                case 3:
                    phat_range_page_down (range);
                    break;

                default:
                    break;
            }
            break;

        case STATE_DRAGGING:
            gtk_grab_remove (widget);
            knob->state = STATE_IDLE;

            break;

        default:
            break;
    }

    return FALSE;
}

static gint
phat_knob_motion_notify (GtkWidget *widget, GdkEventMotion *event)
{
    PhatKnob *knob;
    GdkModifierType mods;
    gint x, y;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (PHAT_IS_KNOB (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    knob = PHAT_KNOB (widget);

    x = event->x;
    y = event->y;


    if (event->is_hint || (event->window != widget->window))
        gdk_window_get_pointer (widget->window, &x, &y, &mods);

    switch (knob->state)
    {
        case STATE_PRESSED:
            knob->state = STATE_DRAGGING;
            /* fall through */

        case STATE_DRAGGING:
            if (mods & GDK_BUTTON1_MASK)
            {
                phat_knob_update_mouse (knob, x - widget->allocation.x, y - widget->allocation.y , TRUE);
                return TRUE;
            }
            else if (mods & GDK_BUTTON3_MASK)
            {
                phat_knob_update_mouse (knob, x - widget->allocation.x, y - widget->allocation.y ,  FALSE);
                return TRUE;
            }
            break;

        default:
            break;
    }

    return FALSE;
}

static gint
phat_knob_scroll (GtkWidget *widget, GdkEventScroll *event)
{
    PhatKnob *knob;
    PhatRange *range;

    knob = PHAT_KNOB (widget);
    range = PHAT_RANGE (widget);

    gtk_widget_grab_focus (widget);

    knob->state = STATE_SCROLL;

    if (event->direction == GDK_SCROLL_UP)
    {
        phat_range_step_up (range);
    }
    else if (event->direction == GDK_SCROLL_DOWN)
    {
        phat_range_step_down (range);
    }
    else if (event->direction == GDK_SCROLL_LEFT)
    {
        phat_range_step_left (range);
    }
    else
    {
        phat_range_step_right (range);
    }

    knob->state = STATE_IDLE;

    return TRUE;
}

static void
phat_knob_update_mouse (PhatKnob *knob, gint x, gint y,
                        gboolean absolute)
{
    gdouble old_value, new_value, dv, dh;
    gdouble angle;
    PhatRange *range;

    g_return_if_fail (knob != NULL);
    g_return_if_fail (PHAT_IS_KNOB (knob));

    range = PHAT_RANGE (knob);

    old_value =     phat_range_get_internal_value (range);
    ;

    angle = atan2 (-y + (knob->size >> 1), x - (knob->size >> 1));

    if (absolute)
    {

        angle /= G_PI;
        if (angle < -0.5)
            angle += 2;

        new_value = -(2.0 / 3.0) * (angle - 1.25);   /* map [1.25pi, -0.25pi] onto [0, 1] */

    }
    else
    {

        dv = knob->saved_y - y; /* inverted cartesian graphics coordinate system */
        dh = x - knob->saved_x;
        knob->saved_x = x;
        knob->saved_y = y;

        if (x >= 0 && x <= knob->size)
            dh = 0;  /* dead zone */
        else
        {
            angle = cos (angle);
            dh *= angle * angle;
        }

        new_value = old_value +
                dv * 0.1 +          /* "step" == 0.1 */
                dh / 200.0f;
    }

    phat_range_set_internal_value (range, new_value);
}

static void
phat_knob_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
    //PhatKnob *knob = PHAT_KNOB (object);

    switch (prop_id)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
phat_knob_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
    //PhatKnob *knob = PHAT_KNOB (object);

    switch (prop_id)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}
