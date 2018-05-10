/* 
 *
 * Most of this code comes from gAlan 0.2.0, copyright (C) 1999
 * Tony Garnock-Jones, with modifications by Sean Bolton,
 * copyright (c) 2004.
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

#ifndef __PHAT_KNOB_H__
#define __PHAT_KNOB_H__

#include <gdk/gdk.h>
#include <gtk/gtkrange.h>

G_BEGIN_DECLS

#define PHAT_KNOB(obj)          GTK_CHECK_CAST(obj, phat_knob_get_type(), PhatKnob)
#define PHAT_KNOB_CLASS(klass)  GTK_CHECK_CLASS_CAST(klass, phat_knob_get_type(), PhatKnobClass)
#define PHAT_IS_KNOB(obj)       GTK_CHECK_TYPE(obj, phat_knob_get_type())
#define PHAT_TYPE_KNOB          (phat_knob_get_type ( ))


typedef struct _PhatKnob        PhatKnob;
typedef struct _PhatKnobClass   PhatKnobClass;

struct _PhatKnob {
    GtkRange range;

    gboolean is_log;

    /* State of widget (to do with user interaction) */
    guint8 state;
    gint saved_x, saved_y;

    /* size of the widget */
    gint size;

    /* Pixmap for knob */
    GdkPixbuf *pixbuf;
    GdkBitmap *mask;
    GdkGC *mask_gc;
    GdkGC *red_gc;
};

struct _PhatKnobClass
{
    GtkRangeClass parent_class;
};

GType phat_knob_get_type ( );

GtkWidget* phat_knob_new (GtkAdjustment* adjustment);

GtkWidget* phat_knob_new_with_range (double value,
                                     double lower,
                                     double upper,
                                     double step);
GtkAdjustment *phat_knob_get_adjustment(PhatKnob *knob);
double phat_knob_get_value (PhatKnob* knob);
void phat_knob_set_value (PhatKnob* knob, double value);
void phat_knob_set_range (PhatKnob* knob, double lower, double upper);
void phat_knob_set_update_policy(PhatKnob *knob, GtkUpdateType  policy);
void phat_knob_set_adjustment(PhatKnob *knob, GtkAdjustment *adjustment);
void phat_knob_set_log (PhatKnob* knob, gboolean is_log);
gboolean phat_knob_is_log (PhatKnob* knob);

G_END_DECLS

#endif
