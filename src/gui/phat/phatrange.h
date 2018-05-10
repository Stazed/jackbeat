/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __PHAT_RANGE_H__
#define __PHAT_RANGE_H__


#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>


G_BEGIN_DECLS


#define PHAT_TYPE_RANGE            (phat_range_get_type ())
#define PHAT_RANGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PHAT_TYPE_RANGE, PhatRange))
#define PHAT_RANGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PHAT_TYPE_RANGE, PhatRangeClass))
#define PHAT_IS_RANGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PHAT_TYPE_RANGE))
#define PHAT_IS_RANGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PHAT_TYPE_RANGE))
#define PHAT_RANGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PHAT_TYPE_RANGE, PhatRangeClass))

/* These two are private/opaque types, ignore */
typedef struct _PhatRangeLayout    PhatRangeLayout;

typedef struct _PhatRange        PhatRange;
typedef struct _PhatRangeClass   PhatRangeClass;

struct _PhatRange
{
    GtkWidget widget;
    GObject *value_mapper; /* slot for the as yet unwritten value_mapper */
    GtkAdjustment *adjustment;

    /*< private >*/
    gdouble internal_value;     /* adjsutment value converted to 0..1 range and possibly mapped too */
    gdouble internal_page;      /* adjsutment page_increment converted to 0..1 range */
    gdouble internal_step;      /* adjsutment step_increment converted to 0..1 range */
    gdouble internal_bigstep;   /* like internal_step but used for left/right scroll events */

    GdkWindow *event_window;
};

struct _PhatRangeClass
{
    GtkWidgetClass parent_class;
    
    void (* value_changed)    (PhatRange     *range);
};


GType              phat_range_get_type                      (void) G_GNUC_CONST;

void               phat_range_set_adjustment                (PhatRange      *range,
                                                            GtkAdjustment *adjustment);
GtkAdjustment*     phat_range_get_adjustment                (PhatRange      *range);

void               phat_range_set_increments                (PhatRange      *range,
                                                            gdouble        step,
                                                            gdouble        page);
void               phat_range_set_range                     (PhatRange      *range,
                                                            gdouble        min,
                                                            gdouble        max);
void               phat_range_set_value                     (PhatRange      *range,
                                                            gdouble        value);
gdouble            phat_range_get_value                     (PhatRange      *range);

gdouble phat_range_get_internal_value(PhatRange *range_ptr);
void phat_range_set_internal_value(PhatRange *range_ptr, gdouble value);

void phat_range_page_up(PhatRange *range_ptr);
void phat_range_page_down(PhatRange *range_ptr);

void phat_range_step_up(PhatRange *range_ptr);
void phat_range_step_down(PhatRange *range_ptr);
void phat_range_step_left(PhatRange *range_ptr);
void phat_range_step_right(PhatRange *range_ptr);

G_END_DECLS


#endif /* __PHAT_RANGE_H__ */
