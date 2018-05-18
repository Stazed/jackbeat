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
 *   SVN:$Id: sequenceeditor.c 643 2009-11-23 19:38:59Z olivier $
 */

#include <stdlib.h>
#include "config.h"
#include "grid.h"
#include "util.h"
#include "gui/sequenceeditor.h"
#include "gui/misc.h"
#include "gui/toggle.h"
#include "gui/slider.h"
#include "gui/toggle.h"

#define TRACK_HPADDING 6
#define TRACK_VPADDING 2
#define LAYOUT_PADDING 1
#define ANIMATION_INTERVAL 34 // miliseconds

#define DEBUG(M, ...) ({ printf("GSE  %s(): ", __func__); printf(M, ## __VA_ARGS__); printf("\n"); })

typedef struct gui_sequence_editor_control_t gui_sequence_editor_control_t;

struct gui_sequence_editor_t
{
    sequence_t *          sequence;
    GtkWidget *           layout;
    GtkWidget *           hscrollbar;
    GtkWidget *           vscrollbar;
    GtkWidget *           volume_label;
    GtkWidget *           controls_viewport;
    GtkWidget *           controls_layout;
    grid_t *              grid;

    gui_sequence_editor_control_t * controls;
    int                   last_name_width;
    int                   last_control_height;
    int                   active_track;
    int                   active_track_drawn;
    int                   top_padding;
    GdkPixmap *           bg;
    GdkPixmap *           bg_inactive;
    GdkPixmap *           bg_active;
    gint                  animation_tag;
    int                   updating;
} ;

struct gui_sequence_editor_control_t
{
    GtkWidget * name;
    GtkWidget * menu;
    toggle_t  * mute;
    toggle_t  * solo;
    int         track;
    slider_t *  volume;
    gui_sequence_editor_t *editor;
} ;

static void gui_sequence_editor_update (gui_sequence_editor_t *self);
static void gui_sequence_editor_control_update_bg (gui_sequence_editor_t *self,
                                                   int active_track, int width,
                                                   int track_height);
static void gui_sequence_editor_on_mute_changed (event_t *event);
static void gui_sequence_editor_on_solo_changed (event_t *event);
static void gui_sequence_editor_on_volume_changed (event_t *event);
static void gui_sequence_editor_on_destroy (event_t *event);

static void
gui_sequence_editor_load_sample (GtkWidget * widget, gui_sequence_editor_control_t * ctl)
{
    gui_sequence_editor_t *self = ctl->editor;
    sequence_position_t *pos = malloc (sizeof (sequence_position_t));
    pos->beat = -1;
    pos->track = ctl->track;
    event_fire (self, "load-sample-request", pos, free);

}

static void
gui_sequence_editor_rename_track (GtkWidget * menu_item, gui_sequence_editor_control_t * ctl)
{
    gui_sequence_editor_t *self = ctl->editor;
    sequence_position_t *pos = malloc (sizeof (sequence_position_t));
    pos->beat = -1;
    pos->track = ctl->track;
    event_fire (self, "track-rename-request", pos, free);
}

static void
gui_sequence_editor_on_track_name_changed (event_t *event)
{
    gui_sequence_editor_t *self = event->self;
    gui_sequence_editor_update (self);
}

static void
gui_sequence_editor_remove_track (GtkWidget * menu_item, gui_sequence_editor_control_t * ctl)
{
    gui_sequence_editor_t *self = ctl->editor;
    sequence_remove_track (self->sequence, ctl->track);
}

static void
gui_sequence_editor_move_track_up (GtkWidget * menu_item, gui_sequence_editor_control_t * ctl)
{
    gui_sequence_editor_t *self = ctl->editor;
    sequence_swap_tracks (self->sequence, ctl->track, ctl->track - 1);
}

static void
gui_sequence_editor_move_track_down (GtkWidget * menu_item, gui_sequence_editor_control_t * ctl)
{
    gui_sequence_editor_t *self = ctl->editor;
    sequence_swap_tracks (self->sequence, ctl->track, ctl->track + 1);
}

static void
gui_sequence_editor_set_active_track (gui_sequence_editor_t *self, int track, int redraw)
{
    GtkAllocation allocation; 
    gtk_widget_get_allocation(GTK_WIDGET(self->controls_layout), &allocation); 
    
    int track_height = self->last_control_height + TRACK_VPADDING;
    gui_sequence_editor_control_update_bg (self, track,
                                           allocation.width,
                                           track_height);
    if (track != self->active_track)
    {
        self->active_track = track;
        if (redraw)
            gtk_widget_queue_draw (self->controls_layout);

        event_fire (self, "track-activated", NULL, NULL);
    }
}

static void
gui_sequence_editor_on_mute_toggled (event_t *event)
{
    gui_sequence_editor_control_t *ctl = event->self;
    gui_sequence_editor_t *self = ctl->editor;

    if (!self->updating)
    {
        int active = toggle_get_active (ctl->mute);
        sequence_mute_track (self->sequence, active, ctl->track);
        gui_sequence_editor_set_active_track (self, ctl->track, 1);
    }
}

static void
gui_sequence_editor_on_solo_toggled (event_t *event)
{
    gui_sequence_editor_control_t *ctl = event->self;
    gui_sequence_editor_t *self = ctl->editor;

    if (!self->updating)
    {
        int active = toggle_get_active (ctl->solo);
        sequence_solo_track (self->sequence, active, ctl->track);
        gui_sequence_editor_set_active_track (self, ctl->track, 1);
    }
}

static void
gui_sequence_editor_volume_adj_changed (GtkAdjustment * adj, gui_sequence_editor_control_t * ctl)
{
    gui_sequence_editor_t *self = ctl->editor;
    double value;
    if (!self->updating)
    {
        value = gtk_adjustment_get_value (adj);
        sequence_set_volume_db (self->sequence, ctl->track, value);
    }
}

static GtkWidget *
gui_sequence_editor_make_menu (gui_sequence_editor_t *self, int track)
{
    GtkWidget *menu;
    GtkWidget *item;
    gui_sequence_editor_control_t *ctl;
    int tracks_num = sequence_get_tracks_num (self->sequence);

    ctl = self->controls + track;

    menu = gtk_menu_new ();

    gui_misc_add_menu_item (menu, "#item", "Load sample...", G_CALLBACK (gui_sequence_editor_load_sample),
                            (gpointer) ctl, NULL);

    gui_misc_add_menu_item (menu, "#item", "Rename track...", G_CALLBACK (gui_sequence_editor_rename_track),
                            (gpointer) ctl, NULL);

    gui_misc_add_menu_item (menu, "#separator", NULL, NULL, NULL, NULL);

    item = gui_misc_add_menu_item (menu, "#item", "Move up", G_CALLBACK (gui_sequence_editor_move_track_up),
                                   (gpointer) ctl, NULL);
    if (track == 0)
        gtk_widget_set_sensitive (item, FALSE);

    item = gui_misc_add_menu_item (menu, "#item", "Move down", G_CALLBACK (gui_sequence_editor_move_track_down),
                                   (gpointer) ctl, NULL);

    if (track == tracks_num - 1)
        gtk_widget_set_sensitive (item, FALSE);

    gui_misc_add_menu_item (menu, "#separator", NULL, NULL, NULL, NULL);

    item = gui_misc_add_menu_item (menu, "#stock", GTK_STOCK_REMOVE, G_CALLBACK
                                   (gui_sequence_editor_remove_track), (gpointer) ctl, NULL);

    if (tracks_num == 1)
        gtk_widget_set_sensitive (item, FALSE);

    return menu;
}

static void
gui_sequence_editor_control_update_bg_item (GdkWindow *window, GdkPixmap **pixmap,
                                            int active, int width, int height)
{
    int update = 0;
    if (!*pixmap)
    {
        update = 1;
    }
    else
    {
        int cur_width, cur_height;
        gdk_drawable_get_size (GDK_DRAWABLE (*pixmap), &cur_width, &cur_height);
        if ((cur_width != width) || (cur_height != height))
        {
            gdk_pixmap_unref (*pixmap);
            update = 1;
        }
    }

    if (update)
    {
        *pixmap = gdk_pixmap_new (window, width, height, -1);
        GtkAllocation alloc = {0, 0, width, height};
        dk_draw_track_bg (GDK_DRAWABLE (*pixmap), &alloc, active, NULL, NULL);
    }
}

static void
gui_sequence_editor_control_update_bg (gui_sequence_editor_t *self, int active_track,
                                       int width, int track_height)
{
    int ntracks = sequence_get_tracks_num (self->sequence);
    int height = track_height * ntracks + self->top_padding;
    int update = 0;

    GdkPixmap *pixmap = self->bg;
    if (!pixmap)
    {
        update = 1;
    }
    else
    {
        int cur_width, cur_height;
        gdk_drawable_get_size (GDK_DRAWABLE (pixmap), &cur_width, &cur_height);
        if ((active_track != self->active_track_drawn)
            || (cur_width != width) || (cur_height != height))
        {
            gdk_pixmap_unref (pixmap);
            update = 1;
        }
    }
    GtkWidget *layout = self->controls_layout;
    GdkWindow *window = gtk_layout_get_bin_window (GTK_LAYOUT (layout));
    if (update && window)
    {
        gui_sequence_editor_control_update_bg_item (window, &self->bg_inactive, 0, width, track_height);
        gui_sequence_editor_control_update_bg_item (window, &self->bg_active, 1, width, track_height);

        self->bg = gdk_pixmap_new (window, width, height, -1);
        int i;
        for (i = 0; i < ntracks; i++)
        {
            GdkPixmap *item = (i == active_track)
                    ? self->bg_active : self->bg_inactive;

            gtk_paint_flat_box (gtk_widget_get_style(layout), GDK_DRAWABLE (self->bg),
                                GTK_STATE_NORMAL, GTK_SHADOW_NONE, NULL, NULL, NULL,
                                0, 0, width, self->top_padding);
            gdk_draw_drawable (
                               self->bg,
                               gtk_widget_get_style(layout)->fg_gc[ gtk_widget_get_state (layout)],
                               item,
                               0, 0,
                               0, i * track_height + self->top_padding, width, track_height);
        }
        self->active_track_drawn = active_track;
    }
}

static void
gui_sequence_editor_draw_control (gui_sequence_editor_t *self, int track)
{
    gui_sequence_editor_control_t *ctl = self->controls + track;

    ctl->editor = self;
    ctl->track = track;

    // Drawing track menu and name
    ctl->menu = gui_sequence_editor_make_menu (self, track);
    ctl->name = gtk_label_new (NULL);
    char *name = sequence_get_track_name (self->sequence, track);
    
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("Drawing track %s", name);
#endif
    
    char *markup;
    markup = g_markup_printf_escaped ("<span weight=\"bold\" foreground=\"white\" size=\"smaller\">%s</span>", name);
    gtk_label_set_markup (GTK_LABEL (ctl->name), markup);
    gtk_misc_set_alignment (GTK_MISC (ctl->name), 0, 0.5);
    g_free (markup);
    free (name);
    gtk_layout_put (GTK_LAYOUT (self->controls_layout), ctl->name, 0, 0);

    /* Drawing Mute button */
    ctl->mute = toggle_new (self->controls_layout, "M");
    dk_hsv_t shift = {56, 0, 0};
    toggle_set_hsv_shift (ctl->mute, &shift);
    toggle_set_active (ctl->mute, sequence_track_is_muted (self->sequence, track));
    event_subscribe (ctl->mute, "toggled", ctl, gui_sequence_editor_on_mute_toggled);

    /* Drawing Solo button */
    ctl->solo = toggle_new (self->controls_layout, "S");
    //  dk_hsv_t shift2 = {-169, 15, 7};
    dk_hsv_t shift2 = {-112, 0, 0};
    //  dk_hsv_t shift2 = {-26, 2, -42};
    toggle_set_hsv_shift (ctl->solo, &shift2);
    toggle_set_active (ctl->solo, sequence_track_is_solo (self->sequence, track));
    event_subscribe (ctl->solo, "toggled", ctl, gui_sequence_editor_on_solo_toggled);

    /* Drawing volume slider */
    GtkAdjustment * adj = (GtkAdjustment *) gtk_adjustment_new (
                                                                sequence_get_volume_db (self->sequence, track),
                                                                SEQUENCE_DBMIN, SEQUENCE_DBMAX, 1, 3, 0);
    g_signal_connect (G_OBJECT (adj), "value_changed",
                      G_CALLBACK (gui_sequence_editor_volume_adj_changed), (gpointer) ctl);
    ctl->volume = slider_new (self->controls_layout, adj);
    slider_set_curve (ctl->volume, SLIDER_DECIBEL);
    slider_add_overlap (ctl->volume, ctl->name);
}

static void
gui_sequence_editor_widget_get_size (GtkWidget *widget, int *height, int *width)
{
    GtkRequisition req;
    gtk_widget_size_request (widget, &req);
    if (req.width > *width)
        *width = req.width;
    if (req.height > *height)
        *height = req.height;
}

static void
gui_sequence_editor_allocate_child (gui_sequence_editor_t *self, GtkWidget *child,
                                    int x, int y, int width, int height)
{
    GtkAllocation allocation;
    allocation.x = x;
    allocation.y = y;
    allocation.width = width;
    allocation.height = height;
    
    GtkAllocation child_allocation;
    gtk_widget_get_allocation(GTK_WIDGET(child), &child_allocation); 

    if ((x != child_allocation.x) || (y != child_allocation.y))
        gtk_layout_move (GTK_LAYOUT (gtk_widget_get_parent(child)), child, x, y);

    gtk_widget_size_allocate (child, &allocation);
}

static void
gui_sequence_editor_control_get_sizes (gui_sequence_editor_t *self, int *name_width,
                                       int *toggle_width, int *volume_width, int *height)
{
    *name_width = 0;
    GtkWidget *layout = self->controls_layout;
    *volume_width = dk_em (layout, 4);
    *toggle_width = 0;
    *height = dk_em (layout, 1.6);
    int i, w;
    gui_sequence_editor_control_t *ctl;
    int ntracks = sequence_get_tracks_num (self->sequence);
    for (i = 0; i < ntracks; i++)
    {
        ctl = self->controls + i;
        if (slider_get_state (ctl->volume) == SLIDER_UNFOLDED)
        {
            *name_width = self->last_name_width;
            *height = self->last_control_height;
            break;
        }

        gui_sequence_editor_widget_get_size (ctl->name, height, name_width);
        w = toggle_get_width (ctl->mute);
        if (w > *toggle_width)
            *toggle_width = w;
        w = toggle_get_width (ctl->solo);
        if (w > *toggle_width)
            *toggle_width = w;
    }


    *name_width += dk_em (layout, 0.8);
    self->last_name_width = *name_width;
    self->last_control_height = *height;
}

static void
gui_sequence_editor_control_realize (GtkWidget *widget, gui_sequence_editor_t *self)
{
    self->top_padding = TRACK_VPADDING * 2;
    gtk_widget_queue_resize (widget);
}

static void
gui_sequence_editor_control_size_request_event (GtkWidget *widget, GtkRequisition *requisition,
                                                gui_sequence_editor_t *self)
{
    int hpad = TRACK_HPADDING;
    int vpad = TRACK_VPADDING;
    int volume_width = 0;
    int name_width, toggle_width, height;
    int top_pad = self->top_padding;

    gui_sequence_editor_control_get_sizes (self, &name_width, &toggle_width, &volume_width, &height);

    int ntracks = sequence_get_tracks_num (self->sequence);
    requisition->width = hpad + name_width + hpad + (toggle_width + hpad) * 2 + volume_width + hpad;
    requisition->height = (height + vpad) * ntracks + top_pad;
    
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("width: %d, height: %d", requisition->width, requisition->height);
#endif
    /*
    if (ntracks > 0)
      requisition->height -= pad;
     */
}

static gboolean
gui_sequence_editor_control_size_allocate_event (GtkWidget *widget, GtkAllocation *event,
                                                 gui_sequence_editor_t *self)
{
    int hpad = TRACK_HPADDING;
    int vpad = TRACK_VPADDING;
    int name_width = 0;
    int volume_width = 0;
    int toggle_width = 0;
    int height = 0;
    gui_sequence_editor_control_t *ctl;
    int i;
    GtkAllocation alloc = {0, 0, 0, 0};
    int top_pad = self->top_padding;

    gui_sequence_editor_control_get_sizes (self, &name_width, &toggle_width, &volume_width, &height);

    int ntracks = sequence_get_tracks_num (self->sequence);

    for (i = 0; i < ntracks; i++)
    {
        ctl = self->controls + i;
        gui_sequence_editor_allocate_child (self, ctl->name, hpad, top_pad + i * (height + vpad),
                                            name_width, height + vpad);
        alloc.width = toggle_width;
        alloc.height = height + vpad;
        alloc.x = hpad + name_width + hpad + volume_width + hpad;
        alloc.y = i * (height + vpad) + top_pad;
        toggle_allocate (ctl->mute, &alloc);

        alloc.width = toggle_width;
        alloc.height = height + vpad;
        alloc.x = hpad + name_width + hpad + volume_width + hpad + toggle_width + hpad;
        alloc.y = i * (height + vpad) + top_pad;
        toggle_allocate (ctl->solo, &alloc);

        alloc.width = volume_width;
        alloc.height = dk_em (widget, 0.9);
        alloc.x = hpad + name_width + hpad;
        alloc.y = i * (height + vpad) + ((height + vpad - alloc.height) / 2) + top_pad;
        slider_allocate (ctl->volume, SLIDER_FOLDED, &alloc);

        alloc.x = 0;
        alloc.y = i * (height + vpad) + top_pad;
        alloc.height = height;
        alloc.width = hpad + name_width + hpad + (toggle_width + hpad) * 2 + volume_width + hpad;
        slider_allocate (ctl->volume, SLIDER_UNFOLDED, &alloc);
    }

    int cell_vspacing = util_round ((double) height  * 1 / 10);
    cell_vspacing += cell_vspacing % 2;

    int cell_height = height - cell_vspacing;
    int cell_width = util_round ((double) cell_height * 3 / 4);
    int grid_vpadding = cell_vspacing / 2 + self->top_padding;
    cell_vspacing += vpad;
    int cell_hspacing = util_round ((double) cell_vspacing * 2 / 3);

    grid_set_cell_size (self->grid, cell_width, 1000, cell_height, cell_height);
    grid_set_spacing (self->grid, cell_hspacing, cell_vspacing);
    grid_set_vpadding (self->grid, grid_vpadding);

    int active_track = self->active_track;
    if (active_track >= ntracks)
        active_track = ntracks - 1;

    gui_sequence_editor_set_active_track (self, active_track, 0);

    return TRUE;
}

static gboolean
gui_sequence_editor_control_expose_event (GtkWidget *widget, GdkEventExpose *event,
                                          gui_sequence_editor_t *self)
{
    if (self->bg && gtk_layout_get_bin_window(GTK_LAYOUT (widget)))
    {
        gdk_draw_drawable (
                           gtk_layout_get_bin_window(GTK_LAYOUT (widget)),
                           gtk_widget_get_style(widget)->fg_gc[gtk_widget_get_state (widget)],
                           self->bg,
                           event->area.x, event->area.y,
                           event->area.x, event->area.y,
                           event->area.width, event->area.height);
    }
    return FALSE;
}

static gint
gui_sequence_editor_control_click_event (GtkWidget *widget, GdkEventButton *event,
                                         gui_sequence_editor_t *self)
{
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    int handled = FALSE;
    if (event->type == GDK_BUTTON_PRESS)
    {
        int track_height = self->last_control_height + TRACK_VPADDING;
        int ntracks = sequence_get_tracks_num (self->sequence);
        int track = (event->y - self->top_padding) / track_height;
        if ((track >= 0) && (track < ntracks))
        {
            switch (event->button)
            {
                case 1:
                    if (event->x < self->last_name_width + TRACK_HPADDING)
                    {
                        gui_sequence_editor_set_active_track (self, track, 1);
                        handled = TRUE;
                    }
                    break;
                case 3:
                    gtk_menu_popup (GTK_MENU (self->controls[track].menu),
                                    NULL, NULL, NULL, NULL,
                                    event->button, event->time);
                    handled = TRUE;
                    break;
            }
        }
    }
    return handled;
}

static void
gui_sequence_editor_size_request_event (GtkWidget *widget, GtkRequisition *requisition, gui_sequence_editor_t *self)
{
    int pad = LAYOUT_PADDING;
    int header_height = 0;
    int hscrollbar_width = 0, hscrollbar_height = 0;
    int vscrollbar_width = 0, vscrollbar_height = 0;
    int controls_width = 0;
    int junk = 0;

    header_height = dk_em (widget, 1);
    gui_sequence_editor_widget_get_size (self->vscrollbar, &vscrollbar_height, &vscrollbar_width);
    gui_sequence_editor_widget_get_size (self->hscrollbar, &hscrollbar_height, &hscrollbar_width);
    gui_sequence_editor_widget_get_size (self->controls_layout, &junk, &controls_width);

    requisition->width = pad + controls_width + pad + hscrollbar_width + pad + vscrollbar_width + pad;
    requisition->height = pad + header_height + pad + vscrollbar_height + pad + hscrollbar_height + pad;
}

static gboolean
gui_sequence_editor_configure_event (GtkWidget *widget, GtkAllocation *event, gui_sequence_editor_t *self)
{
    int pad = LAYOUT_PADDING;
    int header_height = 0;
    int hscrollbar_width = 0, hscrollbar_height = 0;
    int vscrollbar_width = 0, vscrollbar_height = 0;
    int controls_width = 0;
    int junk = 0;

    header_height = dk_em (widget, 0.7);
    gui_sequence_editor_widget_get_size (self->vscrollbar, &vscrollbar_height, &vscrollbar_width);
    gui_sequence_editor_widget_get_size (self->hscrollbar, &hscrollbar_height, &hscrollbar_width);
    gui_sequence_editor_widget_get_size (self->controls_layout, &junk, &controls_width);

    int grid_width = event->width - pad - controls_width - pad - pad - vscrollbar_width - pad;
    int grid_height = event->height - pad - pad - hscrollbar_height - pad;
    int controls_height = grid_height - header_height - pad;

    grid_set_header_height_no_redraw (self->grid, pad + header_height + pad);

    gui_sequence_editor_allocate_child (self, self->controls_viewport,
                                        pad, pad + header_height + pad,
                                        controls_width,
                                        controls_height);
    gui_sequence_editor_allocate_child (self, self->hscrollbar,
                                        pad + controls_width + pad, pad + grid_height + pad,
                                        grid_width,
                                        hscrollbar_height);
    gui_sequence_editor_allocate_child (self, self->vscrollbar,
                                        pad + controls_width + pad + grid_width + pad,
                                        pad,
                                        vscrollbar_width,
                                        grid_height);
    gui_sequence_editor_allocate_child (self, grid_get_widget (self->grid),
                                        pad + controls_width + pad,
                                        pad,
                                        grid_width,
                                        grid_height);

    GtkAllocation *size_delta = malloc (sizeof (GtkAllocation));
    size_delta->width = grid_get_minimum_width (self->grid) - grid_width + 5;
    size_delta->height = grid_get_minimum_height (self->grid) - grid_height + 5;
    event_fire (self, "size-delta-request", size_delta, free);

    return TRUE;
}

static void
gui_sequence_editor_grid_modified (event_t *event)
{
    gui_sequence_editor_t *self = (gui_sequence_editor_t *) event->self;
    grid_cell_state_t *state    = (grid_cell_state_t *) event->data;
    sequence_set_beat (self->sequence, state->row, state->col, state->value);
}

static void
gui_sequence_editor_grid_mask_modified (event_t *event)
{
    gui_sequence_editor_t *self = (gui_sequence_editor_t *) event->self;
    grid_cell_state_t *state    = (grid_cell_state_t *) event->data;
    sequence_set_mask_beat (self->sequence, state->row, state->col, state->value);
}

static void
gui_sequence_editor_grid_pointer_keymoved (event_t *event)
{
    gui_sequence_editor_t *self = (gui_sequence_editor_t *) event->self;
    grid_cell_state_t *state    = (grid_cell_state_t *) event->data;
    gui_sequence_editor_set_active_track (self, state->row, 1);
}

static gboolean
gui_sequence_editor_animate (gpointer data)
{
    gui_sequence_editor_t *self = (gui_sequence_editor_t *) data;
    int i;
    int tracks_num = sequence_get_tracks_num (self->sequence);
    event_process_queue (self);
    if (sequence_is_playing (self->sequence) && self->layout && gtk_widget_get_visible (self->layout))
    {
        for (i = 0; i < tracks_num; i++)
        {
            grid_highlight_cell (self->grid,
                                 sequence_get_active_beat (self->sequence, i),
                                 i,
                                 sequence_get_level (self->sequence, i));

        }
    }
    else
    {
        for (i = 0; i < tracks_num; i++)
        {
            grid_highlight_cell (self->grid, -1, i, 0);
        }
    }
    return TRUE;
}

static void
gui_sequence_editor_on_beat_changed (event_t *event)
{
    gui_sequence_editor_t *self = (gui_sequence_editor_t *) event->self;
    sequence_position_t *pos = (sequence_position_t *) event->data;
    grid_set_value (self->grid, pos->beat, pos->track,
                    sequence_get_beat (self->sequence, pos->track, pos->beat));
    grid_set_mask (self->grid, pos->beat, pos->track,
                   sequence_get_mask_beat (self->sequence, pos->track, pos->beat));
}

static void
gui_sequence_editor_on_resize (event_t *event)
{
    gui_sequence_editor_t *self = (gui_sequence_editor_t *) event->self;
    gui_sequence_editor_update (self);
}

gui_sequence_editor_t *
gui_sequence_editor_new (sequence_t *sequence)
{
    gui_sequence_editor_t *self = malloc (sizeof (gui_sequence_editor_t));
    self->sequence = sequence;
    self->layout = NULL;
    self->controls_layout = NULL;
    self->controls = NULL;
    self->last_name_width = 0;
    self->last_control_height = 0;
    self->top_padding = 0;
    self->active_track = 0;
    self->active_track_drawn = -1;
    self->bg = NULL;
    self->bg_inactive = NULL;
    self->bg_active = NULL;
    self->grid = grid_new ();
    self->updating = 0;
    event_subscribe (self->grid, "value-changed", self, gui_sequence_editor_grid_modified);
    event_subscribe (self->grid, "mask-changed", self, gui_sequence_editor_grid_mask_modified);
    event_subscribe (self->grid, "pointer-keymoved", self, gui_sequence_editor_grid_pointer_keymoved);
    self->animation_tag = g_timeout_add (ANIMATION_INTERVAL, gui_sequence_editor_animate, (gpointer) self);

    event_enable_queue (self);

    event_register (self, "track-activated");
    event_register (self, "load-sample-request");
    event_register (self, "track-rename-request");
    event_register (self, "size-delta-request");

    event_subscribe (self->sequence, "beat-changed", self, gui_sequence_editor_on_beat_changed);
    event_subscribe (self->sequence, "resized", self, gui_sequence_editor_on_resize);
    event_subscribe (self->sequence, "reordered", self, gui_sequence_editor_on_resize);
    event_subscribe (self->sequence, "track-mute-changed", self, gui_sequence_editor_on_mute_changed);
    event_subscribe (self->sequence, "track-solo-changed", self, gui_sequence_editor_on_solo_changed);
    event_subscribe (self->sequence, "track-volume-changed", self, gui_sequence_editor_on_volume_changed);
    event_subscribe (self->sequence, "track-name-changed", self, gui_sequence_editor_on_track_name_changed);
    event_subscribe (self->sequence, "destroy", self, gui_sequence_editor_on_destroy);

    gui_sequence_editor_update (self);

    return self;
}

GtkWidget *
gui_sequence_editor_get_widget (gui_sequence_editor_t *self)
{
    return self->layout;
}

static void
gui_sequence_editor_destroy (GtkObject *object, gui_sequence_editor_t *self)
{
    //FIXME: destroy menus?
    g_source_remove (self->animation_tag);
    grid_destroy (self->grid);
    event_remove_source (self);
    free (self->controls);
    free (self);
}

static void
gui_sequence_editor_on_destroy (event_t *event)
{
    gui_sequence_editor_destroy (NULL, (gui_sequence_editor_t *) event->self);
}

int
gui_sequence_editor_get_active_track (gui_sequence_editor_t *self)
{
    return self->active_track;
}

static void
gui_sequence_editor_create_layout (gui_sequence_editor_t *self)
{
    self->layout = gtk_layout_new (NULL, NULL);
    // The sequence_editor and its children can be styled in an rc file using:
    // widget "*sequence_editor*" style "some-style"
    gtk_widget_set_name (self->layout, "sequence_editor");
    g_signal_connect (G_OBJECT (self->layout), "size-request",
                      G_CALLBACK (gui_sequence_editor_size_request_event), self);
    g_signal_connect (G_OBJECT (self->layout), "size-allocate",
                      G_CALLBACK (gui_sequence_editor_configure_event), self);
    g_signal_connect (G_OBJECT (self->layout), "destroy",
                      G_CALLBACK (gui_sequence_editor_destroy), self);

    self->hscrollbar = gtk_hscrollbar_new (NULL);
    gtk_layout_put (GTK_LAYOUT (self->layout), self->hscrollbar, 0, 0);

    self->vscrollbar = gtk_vscrollbar_new (NULL);
    gtk_layout_put (GTK_LAYOUT (self->layout), self->vscrollbar, 0, 0);

    gtk_layout_put (GTK_LAYOUT (self->layout), grid_get_widget (self->grid), 0, 0);

    gtk_widget_show_all (self->layout);
}

static void
gui_sequence_editor_update (gui_sequence_editor_t *self)
{
    int i, j;
    
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("Drawing sequence");
#endif
    
    self->updating = 1;
    /* Fetching dimensions */
    int ntracks = sequence_get_tracks_num (self->sequence);
    int nbeats = sequence_get_beats_num (self->sequence);
    int measure_len = sequence_get_measure_len (self->sequence);

    /* (Re)allocating main widgets and callback data */
    if (!self->layout)
        gui_sequence_editor_create_layout (self);

    if (self->controls_layout)
        gtk_widget_destroy (self->controls_layout);

    if (self->controls)
        free (self->controls);

    //FIXME: destroy menus?
    self->controls = calloc (ntracks, sizeof (gui_sequence_editor_control_t));

    GtkAdjustment *hadj = gtk_range_get_adjustment (GTK_RANGE (self->hscrollbar));
    GtkAdjustment *vadj = gtk_range_get_adjustment (GTK_RANGE (self->vscrollbar));

    /* Setting up grid */
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("Setting up grid");
#endif
    
    grid_resize (self->grid, nbeats, ntracks);
    grid_set_column_group_size (self->grid, measure_len);
    grid_set_scroll_adjustments (self->grid, hadj, vadj);
    for (j = 0; j < ntracks; j++)
    {
        int handle_mask = sequence_is_enabled_mask (self->sequence, j);
        for (i = 0; i < nbeats; i++)
        {
            grid_set_value (self->grid, i, j, sequence_get_beat (self->sequence, j, i));
            if (handle_mask)
                grid_set_mask (self->grid, i, j, sequence_get_mask_beat (self->sequence, j, i));
        }
    }

    GtkWidget *vbox = gtk_vbox_new (FALSE, 0);

    /* Drawing track controls */
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("Drawing track controls");
#endif

    self->controls_layout = gtk_layout_new (NULL, NULL);
    g_signal_connect (G_OBJECT (self->controls_layout), "size-request",
                      G_CALLBACK (gui_sequence_editor_control_size_request_event), self);
    g_signal_connect (G_OBJECT (self->controls_layout), "size-allocate",
                      G_CALLBACK (gui_sequence_editor_control_size_allocate_event), self);
    g_signal_connect (G_OBJECT (self->controls_layout), "expose-event",
                      G_CALLBACK (gui_sequence_editor_control_expose_event), self);
    g_signal_connect (G_OBJECT (self->controls_layout), "button-press-event",
                      G_CALLBACK (gui_sequence_editor_control_click_event), self);
    g_signal_connect_after (G_OBJECT (self->controls_layout), "realize",
                            G_CALLBACK (gui_sequence_editor_control_realize), self);

    gtk_box_pack_start (GTK_BOX (vbox), self->controls_layout, FALSE, FALSE, 0);
    GtkWidget *viewport = self->controls_viewport = gtk_viewport_new (NULL, vadj);
    gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
    gtk_widget_set_size_request (viewport, -1, 0);
    gtk_container_add (GTK_CONTAINER (viewport), vbox);
    gtk_layout_put (GTK_LAYOUT (self->layout), viewport, 0, 0);

    for (i = 0; i < ntracks; i++)
        gui_sequence_editor_draw_control (self, i);

    /* Finalizing */

    gtk_widget_show_all (viewport);
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("Finished drawing sequence");
#endif
    self->updating = 0;
}

static void
gui_sequence_editor_on_mute_changed (event_t *event)
{
    gui_sequence_editor_t *self = (gui_sequence_editor_t *) event->self;
    sequence_position_t *pos = (sequence_position_t *) event->data;
    gui_sequence_editor_control_t *ctl = self->controls + pos->track;
    toggle_set_active (ctl->mute, sequence_track_is_muted (self->sequence, pos->track));
}

static void
gui_sequence_editor_on_solo_changed (event_t *event)
{
    gui_sequence_editor_t *self = (gui_sequence_editor_t *) event->self;
    sequence_position_t *pos = (sequence_position_t *) event->data;
    gui_sequence_editor_control_t *ctl = self->controls + pos->track;
    toggle_set_active (ctl->solo, sequence_track_is_solo (self->sequence, pos->track));
}

static void
gui_sequence_editor_on_volume_changed (event_t *event)
{
    gui_sequence_editor_t *self = (gui_sequence_editor_t *) event->self;
    sequence_position_t *pos = (sequence_position_t *) event->data;
    gui_sequence_editor_control_t *ctl = self->controls + pos->track;
    double volume = sequence_get_volume_db (self->sequence, pos->track);
    
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("new volume: %f dB", volume);
#endif
    
    GtkAdjustment *adj = slider_get_adjustment (ctl->volume);
    if (slider_get_state (ctl->volume) != SLIDER_UNFOLDED
        && gtk_adjustment_get_value (adj) != volume)
    {
        gtk_adjustment_set_value (adj, volume);
        gtk_adjustment_value_changed (adj);
    }
}

