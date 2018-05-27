/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2001 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2004.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */



/* porting notes. delete these when the port to phat is completed
 *
 * i've implemented the intl macros as simple pass thoughs. (P_ and I_)
 * this is because we don't use intl at the moment. although we probably
 * should now that property information is visible in glade. if we add
 * intl at a later date, we only need to delete the macro definitions
 * below and it should work
 *
 * i replaced GTK_PARAM_READWRITE and GTK_PARAM_READABLE with G_*
 * but they aren't equivilent. the G*_STATIC_* varients included in
 * the GTK_* definitions in gtkprivate.h require gobject >= 2.8 so
 * we'd need to check for that to use them. which is probably what i'll do
 *
 * we need to sort out the credits above too. pete.
 */

#include <config.h>
#include <stdio.h>
#include <math.h>
#include <gtk/gtk.h>

#include "util.h"
#include "phatrange.h"

#define P_(string) (string) /* override intl macros for now */
#define I_(string) (string) /* don't want to remove them only to add them again */

enum
{
    PROP_0,
    PROP_ADJUSTMENT,
    PROP_VALUE_MAPPER,
} ;

enum
{
    VALUE_CHANGED,
    LAST_SIGNAL
} ;



static void phat_range_set_property   (GObject          *object,
                                       guint             prop_id,
                                       const GValue     *value,
                                       GParamSpec       *pspec);
static void phat_range_get_property   (GObject          *object,
                                       guint             prop_id,
                                       GValue           *value,
                                       GParamSpec       *pspec);
#if GTK_CHECK_VERSION(3,0,0)
static void phat_range_destroy        (GtkWidget        *object);
#else
static void phat_range_destroy        (GtkObject        *object);
#endif
static void phat_range_realize        (GtkWidget        *widget);
static void phat_range_unrealize      (GtkWidget        *widget);
static void phat_range_map            (GtkWidget        *widget);
static void phat_range_unmap          (GtkWidget        *widget);
static void phat_range_size_allocate  (GtkWidget        *widget,
                                       GtkAllocation *allocation);


/* Internals */

static void          phat_range_adjustment_changed (GtkAdjustment *adjustment,
                                                    gpointer       data);
static void          phat_range_adjustment_value_changed (GtkAdjustment *adjustment,
                                                          gpointer       data);

static void
phat_range_update_internal_value (PhatRange *range_ptr);

static void
phat_range_update_internals (PhatRange *range_ptr);

static void
phat_range_queue_redraw (PhatRange *range_ptr);

static guint signals[LAST_SIGNAL];

G_DEFINE_ABSTRACT_TYPE (PhatRange, phat_range, GTK_TYPE_WIDGET)

static void
phat_range_class_init (PhatRangeClass *class)
{
    GObjectClass   *gobject_class;
#if GTK_CHECK_VERSION(3,0,0)
    GtkWidgetClass *object_class;
#else
    GtkObjectClass *object_class;
#endif
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS (class);
#if GTK_CHECK_VERSION(3,0,0)
    object_class = (GtkWidgetClass*) class;
#else
    object_class = (GtkObjectClass*) class;
#endif
    widget_class = (GtkWidgetClass*) class;

    gobject_class->set_property = phat_range_set_property;
    gobject_class->get_property = phat_range_get_property;

    object_class->destroy = phat_range_destroy;
    widget_class->realize = phat_range_realize;
    widget_class->unrealize = phat_range_unrealize;
    widget_class->map = phat_range_map;
    widget_class->unmap = phat_range_unmap;
    widget_class->size_allocate = phat_range_size_allocate;

    signals[VALUE_CHANGED] =
            g_signal_new (I_ ("value_changed"),
                          G_TYPE_FROM_CLASS (gobject_class),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (PhatRangeClass, value_changed),
                          NULL, NULL,
                          g_cclosure_marshal_VOID__VOID,
                          G_TYPE_NONE, 0);

    g_object_class_install_property (gobject_class,
                                     PROP_ADJUSTMENT,
                                     g_param_spec_object ("adjustment",
                                                          P_ ("Adjustment"),
                                                          P_ ("The GtkAdjustment that contains the current value of this range object"),
                                                          GTK_TYPE_ADJUSTMENT,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT));


    g_object_class_install_property (gobject_class,
                                     PROP_VALUE_MAPPER,
                                     g_param_spec_object ("value-mapper",
                                                          P_ ("Value Mapper"),
                                                          P_ ("The object that converts between relative knob position and desired value"),
                                                          G_TYPE_OBJECT,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
phat_range_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
    PhatRange *range;

    range = PHAT_RANGE (object);

    switch (prop_id)
    {
        case PROP_ADJUSTMENT:
            phat_range_set_adjustment (range, g_value_get_object (value));
            break;
        case PROP_VALUE_MAPPER:
            //nothing yet
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
phat_range_get_property (GObject      *object,
                         guint         prop_id,
                         GValue       *value,
                         GParamSpec   *pspec)
{
    PhatRange *range;

    range = PHAT_RANGE (object);

    switch (prop_id)
    {
        case PROP_ADJUSTMENT:
            g_value_set_object (value, range->adjustment);
            break;
        case PROP_VALUE_MAPPER:
            //nothing yet
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
phat_range_init (PhatRange *range)
{
    gtk_widget_set_has_window (GTK_WIDGET(range), 0);

    range->adjustment = NULL;
    range->value_mapper = NULL;
    range->internal_value = 0.0;
    range->internal_page = 0.0;
    range->internal_step = 0.0;
    range->internal_bigstep = 0.0;
}

/**
 * phat_range_get_adjustment:
 * @range: a #PhatRange
 * 
 * Get the #GtkAdjustment which is the "model" object for #PhatRange.
 * See phat_range_set_adjustment() for details.
 * The return value does not have a reference added, so should not
 * be unreferenced.
 * 
 * Return value: a #GtkAdjustment
 **/
GtkAdjustment*
phat_range_get_adjustment (PhatRange *range)
{
    g_return_val_if_fail (PHAT_IS_RANGE (range), NULL);

    if (!range->adjustment)
        phat_range_set_adjustment (range, NULL);

    return range->adjustment;
}

/**
 * phat_range_set_adjustment:
 * @range: a #PhatRange
 * @adjustment: a #GtkAdjustment
 *
 * Sets the adjustment to be used as the "model" object for this range
 * widget. The adjustment indicates the current range value, the
 * minimum and maximum range values, the step/page increments used
 * for keybindings and scrolling, and the page size. The page size
 * is normally 0 for #GtkScale and nonzero for #GtkScrollbar, and
 * indicates the size of the visible area of the widget being scrolled.
 * The page size affects the size of the scrollbar slider.
 * 
 **/
void
phat_range_set_adjustment (PhatRange      *range,
                           GtkAdjustment *adjustment)
{
    g_return_if_fail (PHAT_IS_RANGE (range));

    if (!adjustment)
        adjustment = (GtkAdjustment*) gtk_adjustment_new (0.0, 0.0, 10.0, 1.0, 1.0, 1.0);
    else
        g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

    if (range->adjustment != adjustment)
    {
        if (range->adjustment)
        {
            g_signal_handlers_disconnect_by_func (range->adjustment,
                                                  phat_range_adjustment_changed,
                                                  range);
            g_signal_handlers_disconnect_by_func (range->adjustment,
                                                  phat_range_adjustment_value_changed,
                                                  range);
            g_object_unref (range->adjustment);
        }

        range->adjustment = adjustment;
        phat_range_update_internals (range);

        g_object_ref_sink (adjustment);

        g_signal_connect (adjustment, "changed",
                          G_CALLBACK (phat_range_adjustment_changed),
                          range);
        g_signal_connect (adjustment, "value_changed",
                          G_CALLBACK (phat_range_adjustment_value_changed),
                          range);

        phat_range_adjustment_changed (adjustment, range);
        g_object_notify (G_OBJECT (range), "adjustment");
    }
}

/**
 * phat_range_set_increments:
 * @range: a #PhatRange
 * @step: step size
 * @page: page size
 *
 * Sets the step and page sizes for the range.
 * The step size is used when the user clicks the #GtkScrollbar
 * arrows or moves #GtkScale via arrow keys. The page size
 * is used for example when moving via Page Up or Page Down keys.
 * 
 **/
void
phat_range_set_increments (PhatRange *range,
                           gdouble   step,
                           gdouble   page)
{
    g_return_if_fail (PHAT_IS_RANGE (range));

    gtk_adjustment_set_step_increment(range->adjustment, step);
    gtk_adjustment_set_page_increment(range->adjustment, page);

    gtk_adjustment_changed (range->adjustment);
}

/**
 * phat_range_set_range:
 * @range: a #PhatRange
 * @min: minimum range value
 * @max: maximum range value
 * 
 * Sets the allowable values in the #PhatRange, and clamps the range
 * value to be between @min and @max. (If the range has a non-zero
 * page size, it is clamped between @min and @max - page-size.)
 **/
void
phat_range_set_range (PhatRange *range,
                      gdouble   min,
                      gdouble   max)
{
    gdouble value;

    g_return_if_fail (PHAT_IS_RANGE (range));
    g_return_if_fail (min < max);

    gtk_adjustment_set_lower(range->adjustment, min);
    gtk_adjustment_set_upper(range->adjustment, max);

    value = CLAMP (gtk_adjustment_get_value(range->adjustment),
                   gtk_adjustment_get_lower(range->adjustment),
                   (gtk_adjustment_get_upper(range->adjustment) - gtk_adjustment_get_page_size(range->adjustment)));

    gtk_adjustment_set_value (range->adjustment, value);
    gtk_adjustment_changed (range->adjustment);
}

/**
 * phat_range_set_value:
 * @range: a #PhatRange
 * @value: new value of the range
 *
 * Sets the current value of the range; if the value is outside the
 * minimum or maximum range values, it will be clamped to fit inside
 * them. The range emits the "value_changed" signal if the value
 * changes.
 * 
 **/
void
phat_range_set_value (PhatRange *range,
                      gdouble   value)
{
    g_return_if_fail (PHAT_IS_RANGE (range));

    value = CLAMP (value, gtk_adjustment_get_lower(range->adjustment),
                   (gtk_adjustment_get_upper(range->adjustment)));

    gtk_adjustment_set_value (range->adjustment, value);
}

/**
 * phat_range_get_value:
 * @range: a #PhatRange
 * 
 * Gets the current value of the range.
 * 
 * Return value: current value of the range.
 **/
gdouble
phat_range_get_value (PhatRange *range)
{
    g_return_val_if_fail (PHAT_IS_RANGE (range), 0.0);

    return gtk_adjustment_get_value(range->adjustment);
}

gdouble
phat_range_get_internal_value (PhatRange *range_ptr)
{
    return range_ptr->internal_value;
}

void
phat_range_set_internal_value (PhatRange *range_ptr, gdouble value)
{
    value = CLAMP (value, 0.0, 1.0);

    if (range_ptr->internal_value == value)
    {
        return;
    }
    /*
        range_ptr->internal_value = value;

        range_ptr->adjustment->value = range_ptr->adjustment->lower + range_ptr->internal_value * (range_ptr->adjustment->upper - range_ptr->adjustment->lower);
     */
    /*
    printf("value: %f, step: %f, div+round: %f, lower: %f, upper: %f\n", value, range_ptr->adjustment->step_increment, 
                            util_round (
                                      (range_ptr->adjustment->lower + value * (range_ptr->adjustment->upper - range_ptr->adjustment->lower) 
                                          / range_ptr->adjustment->step_increment), )
    );
     */
    gtk_adjustment_set_value(range_ptr->adjustment, util_round (
                                                (gtk_adjustment_get_lower(range_ptr->adjustment) + value * 
                                                (gtk_adjustment_get_upper(range_ptr->adjustment) - gtk_adjustment_get_lower(range_ptr->adjustment)))
                                                / gtk_adjustment_get_step_increment(range_ptr->adjustment))
                                                * gtk_adjustment_get_step_increment(range_ptr->adjustment));

    phat_range_update_internal_value (range_ptr);

    phat_range_queue_redraw (range_ptr);
}

static void
#if GTK_CHECK_VERSION(3,0,0)
phat_range_destroy (GtkWidget *object)
#else
phat_range_destroy (GtkObject *object)
#endif
{
    PhatRange *range = PHAT_RANGE (object);

    if (range->adjustment)
    {
        g_signal_handlers_disconnect_by_func (range->adjustment,
                                              phat_range_adjustment_changed,
                                              range);
        g_signal_handlers_disconnect_by_func (range->adjustment,
                                              phat_range_adjustment_value_changed,
                                              range);
        g_object_unref (range->adjustment);
        range->adjustment = NULL;
    }

    if (range->value_mapper)
    {
        /* WARNING remember to flesh this out by disconnecting
           any signals when the mapper is implemented */
        g_object_unref (range->value_mapper);
        range->value_mapper = NULL;
    }
#if GTK_CHECK_VERSION(3,0,0)
    (* GTK_WIDGET_CLASS (phat_range_parent_class)->destroy) (object);
#else
    (* GTK_OBJECT_CLASS (phat_range_parent_class)->destroy) (object);
#endif
}

static void
phat_range_realize (GtkWidget *widget)
{
    PhatRange *range;
    GdkWindowAttr attributes;
    gint attributes_mask;

    range = PHAT_RANGE (widget);

    gtk_widget_set_realized (widget, 1);

    gtk_widget_set_window(widget, gtk_widget_get_parent_window (widget));
    /* make use of parent window optional ? */
    g_object_ref (gtk_widget_get_window(widget));
    
    GtkAllocation widget_allocation;
    gtk_widget_get_allocation(GTK_WIDGET(widget), &widget_allocation); 

    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = widget_allocation.x;
    attributes.y = widget_allocation.y;
    attributes.width = widget_allocation.width;
    attributes.height = widget_allocation.height;
    attributes.wclass = GDK_INPUT_ONLY;
    attributes.event_mask = gtk_widget_get_events (widget);
    attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                              GDK_BUTTON_RELEASE_MASK |
                              GDK_ENTER_NOTIFY_MASK |
                              GDK_LEAVE_NOTIFY_MASK |
                              GDK_POINTER_MOTION_MASK |
                              GDK_POINTER_MOTION_HINT_MASK);

    attributes_mask = GDK_WA_X | GDK_WA_Y;

    range->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                          &attributes, attributes_mask);
    gdk_window_set_user_data (range->event_window, range);

    gtk_widget_set_style(widget, gtk_style_attach (gtk_widget_get_style(widget), gtk_widget_get_window(widget)));
}

static void
phat_range_unrealize (GtkWidget *widget)
{
    PhatRange *range = PHAT_RANGE (widget);

    gdk_window_set_user_data (range->event_window, NULL);
    gdk_window_destroy (range->event_window);
    range->event_window = NULL;

    if (GTK_WIDGET_CLASS (phat_range_parent_class)->unrealize)
        (* GTK_WIDGET_CLASS (phat_range_parent_class)->unrealize) (widget);
}

static void
phat_range_map (GtkWidget *widget)
{
    PhatRange *range = PHAT_RANGE (widget);

    gdk_window_show (range->event_window);

    GTK_WIDGET_CLASS (phat_range_parent_class)->map (widget);
}

static void
phat_range_unmap (GtkWidget *widget)
{
    PhatRange *range = PHAT_RANGE (widget);

    gdk_window_hide (range->event_window);

    GTK_WIDGET_CLASS (phat_range_parent_class)->unmap (widget);
}

static void
phat_range_adjustment_value_changed (GtkAdjustment *adjustment,
                                     gpointer       data)
{
    PhatRange *range = PHAT_RANGE (data);

    phat_range_update_internal_value (range);

    phat_range_queue_redraw (range);
}

static void
phat_range_queue_redraw (PhatRange *range_ptr)
{
    gtk_widget_queue_draw (GTK_WIDGET (range_ptr));

    /* This is so we don't lag the widget being scrolled. */
    if (gtk_widget_get_realized (GTK_WIDGET (range_ptr)))
        gdk_window_process_updates (gtk_widget_get_window(GTK_WIDGET (range_ptr)), FALSE);

    g_signal_emit (range_ptr, signals[VALUE_CHANGED], 0);
}

static void
phat_range_adjustment_changed (GtkAdjustment *adjustment,
                               gpointer       data)
{
    PhatRange *range = PHAT_RANGE (data);

    phat_range_update_internals (range);

    gtk_widget_queue_draw (GTK_WIDGET (range));
}

static void
phat_range_size_allocate (GtkWidget     *widget,
                          GtkAllocation *allocation)
{
    PhatRange *range;

    range = PHAT_RANGE (widget);
    gtk_widget_set_allocation(widget, allocation);
    
    GtkAllocation widget_allocation;
    gtk_widget_get_allocation(GTK_WIDGET(widget), &widget_allocation); 

    if (gtk_widget_get_realized (GTK_WIDGET(range)))
        gdk_window_move_resize (range->event_window,
                                widget_allocation.x,
                                widget_allocation.y,
                                widget_allocation.width,
                                widget_allocation.height);
}

/* Updates internal value from adjustment */
static
void
phat_range_update_internal_value (PhatRange *range_ptr)
{
    range_ptr->internal_value = (gtk_adjustment_get_value(range_ptr->adjustment) - 
            gtk_adjustment_get_lower(range_ptr->adjustment)) /
            (gtk_adjustment_get_upper(range_ptr->adjustment) -
            gtk_adjustment_get_lower(range_ptr->adjustment));
}

/* Update internals from adjustment */
static
void
phat_range_update_internals (PhatRange *range_ptr)
{
    range_ptr->internal_page = gtk_adjustment_get_page_increment(range_ptr->adjustment) /
            (gtk_adjustment_get_upper(range_ptr->adjustment) - 
            gtk_adjustment_get_lower(range_ptr->adjustment));
    
    range_ptr->internal_step = gtk_adjustment_get_step_increment(range_ptr->adjustment) /
            (gtk_adjustment_get_upper(range_ptr->adjustment) -
            gtk_adjustment_get_lower(range_ptr->adjustment));
    
    range_ptr->internal_bigstep = range_ptr->internal_step * 3; /* from phatknob, should be configurable in future */
    phat_range_update_internal_value (range_ptr);
}

void
phat_range_page_up (PhatRange *range_ptr)
{
    phat_range_set_internal_value (range_ptr, range_ptr->internal_value + range_ptr->internal_page);
}

void
phat_range_page_down (PhatRange *range_ptr)
{
    phat_range_set_internal_value (range_ptr, range_ptr->internal_value - range_ptr->internal_page);
}

void
phat_range_step_up (PhatRange *range_ptr)
{
    phat_range_set_internal_value (range_ptr, range_ptr->internal_value + range_ptr->internal_step);
}

void
phat_range_step_down (PhatRange *range_ptr)
{
    phat_range_set_internal_value (range_ptr, range_ptr->internal_value - range_ptr->internal_step);
}

void
phat_range_step_left (PhatRange *range_ptr)
{
    phat_range_set_internal_value (range_ptr, range_ptr->internal_value + range_ptr->internal_bigstep);
}

void
phat_range_step_right (PhatRange *range_ptr)
{
    phat_range_set_internal_value (range_ptr, range_ptr->internal_value - range_ptr->internal_bigstep);
}

#define __PHAT_RANGE_C__
