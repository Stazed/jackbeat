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
 *   SVN:$Id: grid.c 551 2009-04-30 10:38:56Z olivier $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include "config.h"
#include "grid.h"
#include "gui/dk.h"
#include "core/event.h"

#define GRID_ANIM_INTERVAL 34 
#define GRID_ANIM_LEVEL_TIMEOUT 40
#define GRID_ANIM_LEVEL_FADEOUT 300
#define GRID_ANIM_LEVEL_MAGNIFY 1

#define GRID_CELL_MASK 0x100
#define GRID_GRADIENT_STEPS 64

#define DEBUG(M, ...) {}
/*
#define DEBUG(M, ...) { \
  printf("GRD %s(): ", __func__); \
  printf(M, ## __VA_ARGS__); printf("\n"); \
}
 */

#define GRID_IS_VALID_POS(grid, col, row) \
        ((col >= 0) && (col < grid->col_num) && (row >= 0) && (row < grid->row_num))


typedef struct grid_theme_t
{
    dk_color_t  level_min;
    dk_color_t  level_max;
    dk_color_t  beat_on;
    dk_color_t  beat_off;
    dk_color_t  background;
    dk_color_t  pointer_border;
    dk_color_t  mask_border;
    dk_color_t  mask_background;
    dk_color_t  ruler_label;

} grid_theme_t;

#define GRID_DEFAULT_THEME {  \
  {0xcc,0xcc,0xcc},  \
  {0xff,0xc6,0x00},  \
  {0xcc,0xcc,0xcc},  \
  {0xed,0xed,0xed},  \
  {0xff,0xff,0xff},  \
  {0xa0,0x96,0xea},  \
  {0xa0,0x96,0xea},  \
  {0xff,0xff,0xff},  \
  {0x00,0x00,0x00},  \
}

typedef struct grid_cell_t
{
    int value;
    int mask;
    float level;
    float next_level;
    float timecount;
} grid_cell_t;

struct grid_t
{
    /* Parameters */
    int       min_cell_width;
    int       min_cell_height;
    int       max_cell_width;
    int       max_cell_height;
    int       min_col_spacing;
    int       min_group_spacing;
    int       row_spacing;
    int       y_padding;
    int       col_num;
    int       row_num;
    int       group_col_num;
    int       header_height;
    int       header_label_y;

    /* Allocated objects and generated cells */
    GtkWidget *area;
    GdkPixmap *pixmap;
    int       pixmap_width;
    int       pixmap_height;
    grid_cell_t **cells;
    int       cell_width;
    int       cell_height;
    int       col_spacing;
    int       group_spacing;
    int       last_row;
    int       last_col;
    int       last_value;
    int       last_mask;
    int       pointed_col;
    int       pointed_row;
    int       mask_pointer;
    int       animation_tag;
    GtkAdjustment *hadj;
    GtkAdjustment *vadj;

    grid_theme_t *theme;
} ;

static gboolean grid_configure_event (GtkWidget *widget, GdkEventConfigure *event, grid_t *grid);
static gboolean grid_expose_event (GtkWidget *widget, GdkEventExpose *event, grid_t *grid);
static void     grid_size_request_event (GtkWidget *widget, GtkRequisition *requisition, grid_t *grid);
static gboolean grid_button_press_event (GtkWidget *widget, GdkEventButton *event, grid_t *grid);
static gboolean grid_motion_notify_event (GtkWidget *widget, GdkEventMotion *event, grid_t *grid);
static gboolean grid_key_action_event (GtkWidget * widget, GdkEventKey *event, grid_t * grid);
static void     grid_area_create (grid_t *grid);
gboolean        grid_animation_cb (gpointer data);
static void     grid_draw_cell (grid_t *grid, int col, int row, int Xnotify, int direct);
static void     grid_draw_cell_pointer (grid_t *grid, int col, int row, int is_mask);
static void     grid_draw_all (grid_t *grid);
static void     grid_scroll_event (GtkAdjustment *adj, grid_t * grid);
static gboolean grid_wheel_scroll_event (GtkWidget *widget, GdkEventScroll *event, grid_t *grid);
static void     grid_pointer_moved (grid_t *grid, int x, int y, GdkModifierType state);
static gboolean grid_focus_changed_event (GtkWidget *widget, GdkEventFocus *event, grid_t *grid);

grid_t *
grid_new ()
{
    grid_t *grid = malloc (sizeof (grid_t));
    grid->col_num = 0;
    grid->row_num = 0;
    grid->group_col_num = 4;
    grid->min_group_spacing = 8;
    grid->header_height = 10;
    grid->header_label_y = 0;
    grid->min_cell_width = 10;
    grid->min_cell_height = 10;
    grid->max_cell_width = 1000;
    grid->max_cell_height = 1000;
    grid->min_col_spacing = 4;
    grid->row_spacing = 6;
    grid->y_padding = 0;
    grid->cells = NULL;
    grid->pixmap = NULL;
    grid->last_row = -1;
    grid->last_col = -1;
    grid->last_value = -1;
    grid->last_mask = -1;
    grid->pointed_row = -1;
    grid->pointed_col = -1;
    grid->area = NULL;
    grid->animation_tag = g_timeout_add (GRID_ANIM_INTERVAL, grid_animation_cb, (gpointer) grid);
    grid->hadj = grid->vadj = NULL;

    event_register (grid, "value-changed");
    event_register (grid, "mask-changed");
    event_register (grid, "pointer-keymoved");

    return grid;
}

void
grid_destroy (grid_t *grid)
{
    int i;
    g_source_remove (grid->animation_tag);
    if (grid->cells)
    {
        for (i = 0; i < grid->row_num; i++)
            free (grid->cells[i]);
        free (grid->cells);
    }
    
    if (grid->pixmap)
        g_object_unref (grid->pixmap);

    if (grid->area)
        gtk_widget_destroy (grid->area);

    event_remove_source (grid);

    free (grid);
}

void
grid_resize (grid_t *grid, int col_num, int row_num)
{
    int row;

    if (grid->area != NULL)
    {
        DEBUG ("grid->area is set");
    }
    else
    {
        DEBUG ("grid->area is NOT set");
    }

    grid->cells = realloc (grid->cells, row_num * sizeof (grid_cell_t*));

    int col;
    int common_col_num = col_num < grid->col_num ? col_num : grid->col_num;
    int common_row_num = row_num < grid->row_num ? row_num : grid->row_num;
    grid_cell_t *row_ptr;
    for (row = 0; row < common_row_num; row++)
    {
        row_ptr = grid->cells[row];
        grid->cells[row] = calloc (col_num, sizeof (grid_cell_t));
        memcpy (grid->cells[row], row_ptr, common_col_num * sizeof (grid_cell_t));
        for (col = grid->col_num; col < col_num; col++)
        {
            grid->cells[row][col].level = -1;
            grid->cells[row][col].next_level = -1;
            grid->cells[row][col].mask = 1;
            grid->cells[row][col].value = 0;
            grid->cells[row][col].timecount = 0;
        }
        // free row_ptr ?
    }

    for (row = grid->row_num; row < row_num; row++)
    {
        grid->cells[row] = calloc (col_num, sizeof (grid_cell_t));
        for (col = 0; col < col_num; col++)
        {
            grid->cells[row][col].level = -1;
            grid->cells[row][col].next_level = -1;
            grid->cells[row][col].mask = 1;
            grid->cells[row][col].value = 0;
            grid->cells[row][col].timecount = 0;
        }
    }

    grid->col_num = col_num;
    grid->row_num = row_num;


}

void
grid_set_value (grid_t *grid, int col, int row, int value)
{
    grid->cells[row][col].value = value;
    grid_draw_cell (grid, col, row, 1, 0);
    if ((col == grid->pointed_col) && (row == grid->pointed_row))
        grid_draw_cell_pointer (grid, grid->pointed_col, grid->pointed_row, grid->mask_pointer);
}

void
grid_set_mask (grid_t *grid, int col, int row, int mask)
{
    grid->cells[row][col].mask = mask;
    grid_draw_cell (grid, col, row, 1, 0);
    if ((col == grid->pointed_col) && (row == grid->pointed_row))
        grid_draw_cell_pointer (grid, grid->pointed_col, grid->pointed_row, grid->mask_pointer);
}

GtkWidget *
grid_get_widget (grid_t *grid)
{
    if (!grid->area) grid_area_create (grid);
    return grid->area;
}

void
grid_highlight_cell (grid_t *grid, int col, int row, float level)
{
    if (!(((col == -1) && GRID_IS_VALID_POS (grid, 1, row)) || GRID_IS_VALID_POS (grid, col, row)))
        return;

    if ((level > 1) || (level < 0))
    {
        DEBUG ("Warning - Bad level: %f", level);
    }
    level = level * GRID_ANIM_LEVEL_MAGNIFY;
    if (level > 1) level = 1;
    if (col != -1)
    {
        if (1) // level > grid->cells[row][col].level)
        {
            int x = level * (GRID_GRADIENT_STEPS - 1);
            int y = grid->cells[row][col].level * level * (GRID_GRADIENT_STEPS - 1);
            if (x != y)
            {
                grid->cells[row][col].level = level;
                grid->cells[row][col].next_level = -1;
#ifdef HAVE_GTK_QUARTZ
                grid_draw_cell (grid, col, row, 1, 0);
#else
                grid_draw_cell (grid, col, row, 0, 1);
#endif
            }
        }
        else if (level < grid->cells[row][col].level)
        {
            grid->cells[row][col].next_level = level;
        }
        grid->cells[row][col].timecount = 0;
    }
    else
    {
        int i;
        for (i = 0; i < grid->col_num; i++) grid->cells[row][i].next_level = -1;
    }
}

void
grid_set_cell_size (grid_t *grid, int min_width, int max_width, int min_height, int max_height)
{
    if ((grid->min_cell_width != min_width) || (grid->max_cell_width != max_width) ||
        (grid->min_cell_height != min_height) || (grid->max_cell_height != max_height))
    {
#ifdef PRINT_EXTRA_DEBUG
        DEBUG ("Setting cell size : width (min/max): %d/%d, height (min/max): %d/%d",
               min_width, max_width, min_height, max_height);
#endif
        grid->min_cell_width = min_width;
        grid->max_cell_width = max_width;
        grid->min_cell_height = min_height;
        grid->max_cell_height = max_height;

        grid_draw_all (grid);
        if (grid->area) gtk_widget_queue_resize (grid->area);
    }
}

void
grid_set_spacing (grid_t *grid, int min_col_spacing, int row_spacing)
{
    if (min_col_spacing == -1)
        min_col_spacing = grid->min_col_spacing;

    if (row_spacing == -1)
        row_spacing = grid->row_spacing;

    int min_group_spacing = min_col_spacing * 2;

    if ((grid->min_col_spacing != min_col_spacing) || (grid->row_spacing != row_spacing)
        || (grid->min_group_spacing != min_group_spacing))
    {
#ifdef PRINT_EXTRA_DEBUG
        DEBUG ("Setting min spacing: col: %d, row: %d", col_spacing, row_spacing);
#endif
        grid->min_col_spacing   = min_col_spacing;
        grid->min_group_spacing = min_group_spacing;
        grid->row_spacing       = row_spacing;

        grid_draw_all (grid);
        if (grid->area) gtk_widget_queue_resize (grid->area);
    }
}

void
grid_set_column_group_size (grid_t *grid, int column_num)
{
    grid->group_col_num = column_num;
    grid_draw_all (grid);
    if (grid->area) gtk_widget_queue_resize (grid->area);
}

void
grid_set_header_height (grid_t *grid, int height)
{
    grid->header_height = height;
    grid_draw_all (grid);
    if (grid->area) gtk_widget_queue_resize (grid->area);
}

void
grid_set_header_height_no_redraw (grid_t *grid, int height)
{
    grid->header_height = height;
}

void
grid_set_vpadding (grid_t *grid, int padding)
{
    grid->y_padding = padding;
}

void
grid_set_header_label_ypos (grid_t *grid, int y)
{
    grid->header_label_y = y;
}

void
grid_set_scroll_adjustments (grid_t* grid, GtkAdjustment *hadj, GtkAdjustment *vadj)
{
    grid->hadj = hadj;
    grid->vadj = vadj;

    g_signal_connect (G_OBJECT (hadj), "value-changed",
                      G_CALLBACK (grid_scroll_event), grid);
    g_signal_connect (G_OBJECT (vadj), "value-changed",
                      G_CALLBACK (grid_scroll_event), grid);
}

/* PRIVATE */

static void
grid_area_finalized_cb (void *data, GObject *junk)
{
    grid_t *grid = (grid_t *) data;
    grid->area = NULL;
}

static void
grid_area_create (grid_t *grid)
{
    grid->area = gtk_drawing_area_new ();
    g_object_weak_ref (G_OBJECT (grid->area), grid_area_finalized_cb, grid);

    g_signal_connect (G_OBJECT (grid->area), "expose-event",
                      G_CALLBACK (grid_expose_event), grid);
    g_signal_connect (G_OBJECT (grid->area), "configure-event",
                      G_CALLBACK (grid_configure_event), grid);
    g_signal_connect (G_OBJECT (grid->area), "size-request",
                      G_CALLBACK (grid_size_request_event), grid);
    g_signal_connect (G_OBJECT (grid->area), "button-press-event",
                      G_CALLBACK (grid_button_press_event), grid);
    g_signal_connect (G_OBJECT (grid->area), "motion-notify-event",
                      G_CALLBACK (grid_motion_notify_event), grid);
    gtk_widget_set_can_focus(grid->area, 1);
    g_signal_connect (G_OBJECT (grid->area), "key_press_event",
                      G_CALLBACK (grid_key_action_event), grid);
    g_signal_connect (G_OBJECT (grid->area), "key_release_event",
                      G_CALLBACK (grid_key_action_event), grid);
    g_signal_connect (G_OBJECT (grid->area), "scroll-event",
                      G_CALLBACK (grid_wheel_scroll_event), grid);
    g_signal_connect (G_OBJECT (grid->area), "focus-in-event",
                      G_CALLBACK (grid_focus_changed_event), grid);
    g_signal_connect (G_OBJECT (grid->area), "focus-out-event",
                      G_CALLBACK (grid_focus_changed_event), grid);

    gtk_widget_set_events (grid->area, GDK_EXPOSURE_MASK
                           // | GDK_LEAVE_NOTIFY_MASK
                           | GDK_KEY_PRESS_MASK
                           | GDK_BUTTON_PRESS_MASK
                           | GDK_POINTER_MOTION_MASK
                           | GDK_POINTER_MOTION_HINT_MASK
                           | GDK_FOCUS_CHANGE_MASK);
}

static int
grid_col2x (grid_t *grid, int col)
{
    return grid->col_spacing + col * (grid->col_spacing + grid->cell_width)
            + col / grid->group_col_num * (grid->group_spacing - grid->col_spacing);
}

static int
grid_row2y (grid_t *grid, int row)
{
    return grid->header_height + grid->y_padding + row * (grid->row_spacing + grid->cell_height);
}

int
grid_x2col (grid_t *grid, int x)
{
    int col;
    int group_width = grid->group_col_num * (grid->cell_width + grid->col_spacing) - grid->col_spacing;
    int group = (x - grid->col_spacing) / (group_width + grid->group_spacing);
    x -= group * (group_width + grid->group_spacing);
    if (x >= group_width)
        col = -1;
    else
    {
        col = x / (grid->cell_width + grid->col_spacing);
        if (x >= col * (grid->cell_width + grid->col_spacing) + grid->cell_width) col = -1;
        else col += group * grid->group_col_num;
    }

    return col;
}

int
grid_y2row (grid_t *grid, int y)
{
    y -= grid->header_height;
    int row = y / (grid->cell_height + grid->row_spacing);
    if (y < row * (grid->cell_height + grid->row_spacing) + grid->y_padding) row = -1;
    return row;
}

static void
grid_draw_mask_icon (grid_t *grid, int col, int row, const char icon[8][8])
{
    int i, j;

    int x = grid_col2x (grid, col) + (grid->cell_width - 8) / 2;
    int y = grid_row2y (grid, row) + (grid->cell_height - 8) / 2;

    cairo_t *mask_border_cr;
    mask_border_cr = dk_make_cr (grid->pixmap, &grid->theme->mask_border);
    cairo_set_line_cap(mask_border_cr, CAIRO_LINE_CAP_ROUND); 
    cairo_set_line_width(mask_border_cr, 3.0);
    
    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if (icon[j][i])
            {
                cairo_move_to (mask_border_cr, x + i, y + j);
                cairo_close_path (mask_border_cr);
            }
    cairo_stroke (mask_border_cr);
    cairo_destroy(mask_border_cr);
}

static void
grid_draw_mask (grid_t *grid, int col, int row)
{
    const char icon[8][8] ={
        {1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1}
    };

    grid_draw_mask_icon (grid, col, row, icon);
}

static void
grid_draw_mask_pointer (grid_t *grid, int col, int row)
{
    const char icon[8][8] ={
        {1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1}
    };

    grid_draw_mask_icon (grid, col, row, icon);
}

static int
grid_get_window_xpos (grid_t *grid)
{
    gdouble pos;
    if (grid->hadj)
    {
        g_object_get (G_OBJECT (grid->hadj), "value", &pos, NULL);
        return pos;
    }
    return 0;
}

static int
grid_get_window_ypos (grid_t *grid)
{
    gdouble pos;
    if (grid->vadj)
    {
        g_object_get (G_OBJECT (grid->vadj), "value", &pos, NULL);
#ifdef PRINT_EXTRA_DEBUG
        DEBUG ("ypos: %f", pos);
#endif
        return pos;
    }
    return 0;
}

static int
grid_from_window_rect (grid_t *grid, GdkRectangle *src, GdkRectangle *dest)
{
    GdkRectangle full, _src;
    memcpy (&_src, src, sizeof (GdkRectangle));
    full.x = 0;
    full.y = 0;
    full.width = grid->pixmap_width;
    full.height = grid->pixmap_height;
    _src.x += grid_get_window_xpos (grid);
    _src.y += grid_get_window_ypos (grid);
    return gdk_rectangle_intersect (&_src, &full, dest);
}

static int
grid_to_window_rect (grid_t *grid, GdkRectangle *src, GdkRectangle *dest)
{
    GdkRectangle visible;
    GtkAllocation grid_area_allocation;
    gtk_widget_get_allocation(GTK_WIDGET(grid->area), &grid_area_allocation); 
    
    visible.x = grid_get_window_xpos (grid);
    visible.y = grid_get_window_ypos (grid);
    visible.width = grid_area_allocation.width;
    visible.height = grid_area_allocation.height;
    int is_visible = gdk_rectangle_intersect (src, &visible, dest);
    dest->x -= visible.x;
    dest->y -= visible.y;
    return is_visible;
}

static void
grid_display (grid_t *grid, int x, int y, int width, int height, int direct, int is_dest)
{
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("x=%d, y=%d, width=%d, height=%d, direct=%d, is_dest=%d",
           x, y, width, height, direct, is_dest);
#endif
    int is_visible = 0;
    if (grid->pixmap && grid->area)
    {
        GdkRectangle dest, src;
        if (is_dest)
        {
            dest.x = x;
            dest.y = y;
            dest.width = width;
            dest.height = height;
            is_visible = grid_from_window_rect (grid, &dest, &src);
            dest.width = src.width;
            dest.height = src.height;
        }
        else
        {
            src.x = x;
            src.y = y;
            src.width = width;
            src.height = height;
            is_visible = grid_to_window_rect (grid, &src, &dest);
        }

#ifdef PRINT_EXTRA_DEBUG
        DEBUG ("visible=%d, src=(%d, %d, %d, %d), dest=(%d, %d, %d, %d)",
               is_visible,
               src.x, src.y, src.width, src.height,
               dest.x, dest.y, dest.width, dest.height);
#endif
        if (is_visible)
        {
            if (direct)
            {
                GdkRectangle visible, header, pattern;
                GtkAllocation grid_area_allocation;
                gtk_widget_get_allocation(GTK_WIDGET(grid->area), &grid_area_allocation); 

                visible.x = 0;
                visible.y = 0;
                visible.width = grid_area_allocation.width;
                visible.height = grid->header_height;

                if (gdk_rectangle_intersect (&visible, &dest, &header))
                {
                    gdk_draw_drawable (
                                       gtk_widget_get_window(grid->area),
                                       gtk_widget_get_style(grid->area)->fg_gc[gtk_widget_get_state (grid->area)],
                                       grid->pixmap,
                                       header.x + grid_get_window_xpos (grid),
                                       header.y, header.x, header.y, header.width, header.height);
                }

                visible.x = 0;
                visible.y = grid->header_height;
                visible.width = grid_area_allocation.width;
                visible.height = grid_area_allocation.height - grid->header_height;

                if (gdk_rectangle_intersect (&visible, &dest, &pattern))
                {
                    gdk_draw_drawable (
                                       gtk_widget_get_window(grid->area),
                                       gtk_widget_get_style(grid->area)->fg_gc[gtk_widget_get_state (grid->area)],
                                       grid->pixmap,
                                       src.x, pattern.y + grid_get_window_ypos (grid),
                                       pattern.x, pattern.y, pattern.width, pattern.height);
                }
            }
            else
            {
                gtk_widget_queue_draw_area (grid->area,
                                            dest.x, dest.y, dest.width, dest.height);
            }
        }
    }
}

static void
grid_update_adjustments (grid_t *grid)
{
    GtkAllocation grid_area_allocation;
    gtk_widget_get_allocation(GTK_WIDGET(grid->area), &grid_area_allocation); 
    
    if (grid->hadj)
    {
#ifdef PRINT_EXTRA_DEBUG
        DEBUG ("hadj: type=%s, upper=%d, step=%d, page/size=%d", G_OBJECT_TYPE_NAME (grid->hadj),
               grid->pixmap_width,
               grid->cell_width, grid->area->allocation.width);
#endif
        g_object_set (G_OBJECT (grid->hadj),
                      "lower", (gdouble) 0,
                      "upper", (gdouble) grid->pixmap_width,
                      "step-increment", (gdouble) grid->cell_width + grid->col_spacing,
                      "page-increment", (gdouble) grid_area_allocation.width,
                      "page-size", (gdouble) grid_area_allocation.width,
                      NULL);

        gtk_adjustment_changed (grid->hadj);
    }
    if (grid->vadj)
    {

        g_object_set (G_OBJECT (grid->vadj),
                      "lower", (gdouble) 0,
                      "upper", (gdouble) grid->pixmap_height - grid->header_height,
                      "step-increment", (gdouble) grid->cell_height + grid->row_spacing,
                      "page-increment", (gdouble) grid_area_allocation.height - grid->header_height,
                      "page-size", (gdouble) grid_area_allocation.height - grid->header_height,
                      NULL);

        gtk_adjustment_changed (grid->vadj);
    }
}

static void
grid_scroll_event (GtkAdjustment *adj, grid_t * grid)
{
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("scrolling");
#endif
    grid_display (grid, 0, 0, grid->pixmap_width, grid->pixmap_height, 0, 0);
}

static gboolean
grid_wheel_scroll_event (GtkWidget *widget, GdkEventScroll *event, grid_t *grid)
{
    gdouble step, value, size, upper;
    int dir;
    GtkAdjustment *adj;

    if ((event->direction == GDK_SCROLL_UP) || (event->direction == GDK_SCROLL_DOWN))
    {
        adj = grid->vadj;
        dir = (event->direction == GDK_SCROLL_UP) ? -1 : 1;
    }
    else
    {
        adj = grid->hadj;
        dir = (event->direction == GDK_SCROLL_LEFT) ? -1 : 1;
    }

    g_object_get (G_OBJECT (adj),
                  "step-increment", &step,
                  "value", &value,
                  "page-size", &size,
                  "upper", &upper,
                  NULL);

    value += step * dir;
    if (value < 0)
        value = 0;
    else if (value > upper - size)
        value = upper - size;

    gtk_adjustment_set_value (adj, value);

    int x, y;
    GdkModifierType state;
    gdk_window_get_pointer (event->window, &x, &y, &state);
    grid_pointer_moved (grid, x, y, state);

    return FALSE;
}

static void
grid_bring_into_view (grid_t *grid, int col, int row)
{
    int x      = grid_col2x (grid, col);
    int y      = grid_row2y (grid, row) - grid->header_height;
    int left   = x - grid->col_spacing;
    int right  = x + grid->cell_width + grid->col_spacing;
    int top    = y - grid->row_spacing;
    int bottom = y + grid->cell_height + grid->row_spacing;
    int winx   = grid_get_window_xpos (grid);
    int winy   = grid_get_window_ypos (grid);
    
    GtkAllocation grid_area_allocation;
    gtk_widget_get_allocation(GTK_WIDGET(grid->area), &grid_area_allocation); 
    
    int height = grid_area_allocation.height - grid->header_height;
    int width  = grid_area_allocation.width;

    if (left < winx)
    {
        gtk_adjustment_set_value (grid->hadj, left);
    }
    else if (right > winx + width)
    {
        gtk_adjustment_set_value (grid->hadj, right - width);
    }

    if (top < winy)
    {
        gtk_adjustment_set_value (grid->vadj, top);
    }
    else if (bottom > winy + height)
    {
        gtk_adjustment_set_value (grid->vadj, bottom - height);
    }
}

static void
grid_draw_cell (grid_t *grid, int col, int row, int Xnotify, int direct)
{
    if ((col >= grid->col_num) || (row >= grid->row_num))
        DEBUG ("OUT OF BOUNDS: col/col_num: %d/%d ; row/row_num: %d/%d", col, grid->col_num, row, grid->row_num);

    if (grid->pixmap)
    {
        int int_level;
        if (0) // histogram
        {
            cairo_t *cr = grid->cells[row][col].value ? dk_make_cr (grid->pixmap, &grid->theme->beat_on) :
                dk_make_cr (grid->pixmap, &grid->theme->beat_off);
            
            cairo_rectangle(cr, grid_col2x (grid, col),
                                grid_row2y (grid, row),
                                grid->cell_width,
                                grid->cell_height);
            
            cairo_fill(cr);
            cairo_stroke(cr);
            cairo_destroy (cr);
            
            if (grid->cells[row][col].level != -1)
            {
                /* FIXME - there has got to be a better way!!!!*/
                cairo_t     *values_cr[GRID_GRADIENT_STEPS]; 
                dk_make_gradient (values_cr, grid->pixmap, &grid->theme->level_min,
                      &grid->theme->level_max, GRID_GRADIENT_STEPS);
                
                cairo_t *cr = values_cr[GRID_GRADIENT_STEPS - 1];
                int_level = grid->cells[row][col].level * ((float) grid->cell_height);

                cairo_rectangle(cr, grid_col2x (grid, col),
                                    grid_row2y (grid, row) + grid->cell_height - int_level,
                                    grid->cell_width,
                                    int_level);

                cairo_fill(cr);
                cairo_stroke(cr);
                
                /* FIXME - same as above */
                for (int i = 0; i < GRID_GRADIENT_STEPS; i++)
                    cairo_destroy (values_cr[i]);
            }
        }
        else // scale
        {
            cairo_t *cr;
            if (grid->cells[row][col].level == -1)
                cr = grid->cells[row][col].value ? dk_make_cr (grid->pixmap, &grid->theme->beat_on) :
                    dk_make_cr (grid->pixmap, &grid->theme->beat_off);
            else
            {
                int_level = grid->cells[row][col].level * ((float) GRID_GRADIENT_STEPS - 1.0f);

                /* FIXME - there has got to be a better way!!!!*/
                cairo_t     *values_cr[GRID_GRADIENT_STEPS];
                dk_make_gradient (values_cr, grid->pixmap, &grid->theme->level_min,
                      &grid->theme->level_max, GRID_GRADIENT_STEPS);
                
                cr = values_cr[int_level];
                
                /* FIXME - same as above */
                for (int i = 0; i < GRID_GRADIENT_STEPS; i++)
                {
                    if(i != int_level)
                        cairo_destroy (values_cr[i]);
                }
            }
            
            cairo_rectangle(cr, grid_col2x (grid, col),
                                grid_row2y (grid, row),
                                grid->cell_width,
                                grid->cell_height);
            
            cairo_fill(cr);
            cairo_stroke(cr);
            cairo_destroy (cr);  
        }

        if (!grid->cells[row][col].mask) grid_draw_mask (grid, col, row);

        if (Xnotify || direct)
        {
            grid_display (grid,
                          grid_col2x (grid, col),
                          grid_row2y (grid, row),
                          grid->cell_width,
                          grid->cell_height,
                          direct, 0);
        }
    }
}

static void
grid_set_default_theme (grid_t *grid)
{
    static grid_theme_t theme = GRID_DEFAULT_THEME;
    grid->theme = &theme;
}

static void
grid_draw_cell_pointer (grid_t *grid, int col, int row, int is_mask)
{
    if (grid->pixmap)
    {
        if (is_mask)
        {
            grid_draw_mask_pointer (grid, col, row);
        }
        else
        {
            cairo_t *cr = dk_make_cr (grid->pixmap, &grid->theme->pointer_border);
            cairo_rectangle(cr, grid_col2x (grid, col)+2,
                                grid_row2y (grid, row)+2,
                                grid->cell_width - 3,
                                grid->cell_height - 3);
            
            cairo_stroke(cr);
            cairo_destroy(cr); 
        }

        grid_display (grid,
                      grid_col2x (grid, col),
                      grid_row2y (grid, row),
                      grid->cell_width,
                      grid->cell_height,
                      0, 0);       
    }
}

static void
grid_draw_ruler (grid_t *grid)
{
    int col;
    char str[128];
    if (GTK_IS_WIDGET (grid->area))
    {
        GtkStyle *style = gtk_widget_get_style (grid->area);
        gtk_paint_flat_box (style, GDK_DRAWABLE (grid->pixmap),
                            GTK_STATE_NORMAL, GTK_SHADOW_NONE, NULL, NULL, NULL,
                            0, 0, grid->pixmap_width, grid->header_height);
        gtk_paint_hline (style, GDK_DRAWABLE (grid->pixmap), GTK_STATE_NORMAL, NULL,
                         NULL, NULL, 0, grid->pixmap_width, grid->header_height - 1);

        int subcol;
        for (col = 0; col < grid->col_num; col += grid->group_col_num)
        {
            if ((grid->group_col_num > 1) || (col % 2 == 0))
            {
                int x = grid_col2x (grid, col);
                if (col != 0)
                {
                    gtk_paint_vline (style, GDK_DRAWABLE (grid->pixmap), GTK_STATE_NORMAL,
                                     NULL, NULL, NULL, grid->header_height - grid->header_height * 2 / 3,
                                     grid->header_height - 2, x);
                }
                for (subcol = 1; subcol < grid->group_col_num; subcol++)
                {
                    gtk_paint_vline (style, GDK_DRAWABLE (grid->pixmap),
                                     GTK_STATE_NORMAL, NULL, NULL, NULL, grid->header_height - grid->header_height * 1 / 3,
                                     grid->header_height - 2, grid_col2x (grid, col + subcol));
                }

                PangoContext *context = gtk_widget_get_pango_context (grid->area);
                PangoLayout *layout = pango_layout_new (context);
                snprintf (str, 128, "<small><small>%d</small></small>", col / grid->group_col_num + 1);
                pango_layout_set_markup (layout, str, -1);
                
                cairo_t *ruler_label_cr = dk_make_cr (grid->pixmap, &grid->theme->ruler_label);
                cairo_move_to (ruler_label_cr, x + 3, grid->header_label_y);
                pango_cairo_show_layout (ruler_label_cr, layout);
                g_object_unref (layout);
                cairo_destroy(ruler_label_cr);
            }
        }
    }
}

static void
grid_draw_all (grid_t *grid)
{
    if (grid->pixmap)
    {
#ifdef PRINT_EXTRA_DEBUG
        DEBUG ("row spacing: %d", grid->row_spacing);
        DEBUG ("Drawing background");
#endif
        /* Draw grid background */
        cairo_t *background_cr  = dk_make_cr (grid->pixmap, &grid->theme->background);
        cairo_rectangle(background_cr, 0, 0,
                            grid->pixmap_width, grid->pixmap_height);

        cairo_fill(background_cr);
        cairo_stroke(background_cr);
        cairo_destroy(background_cr);
        
#ifdef PRINT_EXTRA_DEBUG
        DEBUG ("Drawing cells");
#endif
        int row, col;
        for (row = 0; row < grid->row_num; row++)
            for (col = 0; col < grid->col_num; col++)
                grid_draw_cell (grid, col, row, 0, 0);
#ifdef PRINT_EXTRA_DEBUG
        DEBUG ("Drawing ruler");
#endif
        grid_draw_ruler (grid);

        if (GTK_IS_WIDGET (grid->area))
        {
            GtkStyle *style = gtk_widget_get_style (grid->area);
            gtk_paint_vline (style, GDK_DRAWABLE (grid->pixmap),
                             GTK_STATE_NORMAL, NULL, NULL, "vseparator",
                             0, grid->pixmap_height, 0);
        }

        grid_display (grid, 0, 0, grid->pixmap_width, grid->pixmap_height, 0, 0);
    }
}

int
grid_get_minimum_width (grid_t *grid)
{
    return grid->col_num * grid->min_cell_width + (grid->col_num + 1) * grid->min_col_spacing
            + (grid->col_num / grid->group_col_num - 1) * (grid->min_group_spacing - grid->min_col_spacing);
}

int
grid_get_minimum_height (grid_t *grid)
{
    return grid->row_num * grid->min_cell_height + (grid->row_num - 1) * grid->row_spacing
            + grid->y_padding * 2 + grid->header_height;
}

static gboolean
grid_configure_event (GtkWidget *widget, GdkEventConfigure *event, grid_t *grid)
{
    if (grid->pixmap) g_object_unref (grid->pixmap);
    
    GtkAllocation widget_allocation;
    gtk_widget_get_allocation(GTK_WIDGET(widget), &widget_allocation); 

    int minimum_width = grid_get_minimum_width (grid);
    grid->pixmap_width = (minimum_width > widget_allocation.width)
            ?  minimum_width : widget_allocation.width;

    float hratio = (float) grid->pixmap_width / minimum_width;
    if (grid->min_cell_width * hratio > grid->max_cell_width)
    {
        hratio = (float) grid->max_cell_width / grid->min_cell_width;
    }

    grid->cell_width    = grid->min_cell_width * hratio;
    grid->col_spacing   = grid->min_col_spacing * hratio;
    grid->group_spacing = grid->min_group_spacing * hratio;
    /*
      grid->cell_width = (grid->pixmap_width - (grid->col_num + 1) * grid->col_spacing
                           - (grid->col_num / grid->group_col_num - 1) * (grid->group_spacing - grid->col_spacing))
                         / grid->col_num;
      grid->cell_width = grid->cell_width > grid->max_cell_width ? grid->max_cell_width : grid->cell_width;
     */
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("configure width - minimum/allocated/cell: %d/%d/%d", minimum_width, widget_allocation.width, grid->cell_width);
#endif
    int minimum_height = grid_get_minimum_height (grid);
    //grid->header_height += widget->allocation.height - minimum_height;
    //minimum_height = grid_get_minimum_height (grid);
    grid->pixmap_height =  (minimum_height > widget_allocation.height)
            ? minimum_height : widget_allocation.height;

    grid->cell_height = (grid->pixmap_height - (grid->row_num - 1) * grid->row_spacing - grid->y_padding * 2
                         - grid->header_height)
            / grid->row_num;
    grid->cell_height = grid->cell_height > grid->max_cell_height ? grid->max_cell_height : grid->cell_height;
    
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("configure height - minimum/allocated/cell: %d/%d/%d", minimum_height, widget->allocation.height, grid->cell_height);
    DEBUG ("Creating pixmap of size %d x %d", grid->pixmap_width, grid->pixmap_height);
#endif
    
    grid->pixmap = gdk_pixmap_new (gtk_widget_get_window(widget), grid->pixmap_width, grid->pixmap_height, -1);

    grid_update_adjustments (grid);
    grid_set_default_theme (grid);
    grid_draw_all (grid);

    return TRUE;
}

static gboolean
grid_expose_event (GtkWidget *widget, GdkEventExpose *event, grid_t *grid)
{
    grid_display (grid, event->area.x, event->area.y, event->area.width, event->area.height, 1, 1);

    return FALSE;
}

static gboolean
grid_focus_changed_event (GtkWidget *widget, GdkEventFocus *event, grid_t *grid)
{
    if (grid->pointed_col != -1 && grid->pointed_row != -1)
    {
        if (event->in)
        {
            grid_draw_cell_pointer (grid, grid->pointed_col, grid->pointed_row, 0);
        }
        else
        {
            grid_draw_cell (grid, grid->pointed_col, grid->pointed_row, 1, 0);
        }
    }
    return FALSE;
}

static void
grid_size_request_event (GtkWidget *widget, GtkRequisition *requisition, grid_t *grid)
{
    requisition->width  = grid->hadj ? 0 : grid_get_minimum_width (grid);
    requisition->height = grid->vadj ? grid->header_height : grid_get_minimum_height (grid);
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("size request called - width/height: %d/%d", requisition->width, requisition->height);
#endif
}

static void
grid_toggle_cell (grid_t *grid, int col, int row, int mask)
{
    if (mask)
    {
        grid->last_mask = grid->cells[row][col].mask = grid->cells[row][col].mask ? 0 : 1;
        grid_draw_cell (grid, col, row, 1, 0);

        grid->last_col            = col;
        grid->last_row            = row;

        grid_cell_state_t *state  = malloc (sizeof (*state));
        state->col                = col;
        state->row                = row;
        state->value              = grid->cells[row][col].mask;
        event_fire (grid, "mask-changed", state, free);
    }
    else
    {
        grid->last_value = grid->cells[row][col].value = grid->cells[row][col].value ? 0 : 1;
        grid_draw_cell (grid, col, row, 1, 0);

        grid->last_col            = col;
        grid->last_row            = row;

        grid_cell_state_t *state  = malloc (sizeof (*state));
        state->col                = col;
        state->row                = row;
        state->value              = grid->cells[row][col].value;
        event_fire (grid, "value-changed", state, free);
    }
}

static void
grid_move_pointer (grid_t *grid, int col, int row, int mask)
{
    if (grid->pointed_col != -1 && grid->pointed_row != -1 &&
        (grid->pointed_col != col || grid->pointed_row != row || grid->mask_pointer != mask))
        grid_draw_cell (grid, grid->pointed_col, grid->pointed_row, 1, 0);
    grid_draw_cell_pointer (grid, col, row, mask);

    grid->pointed_col = col;
    grid->pointed_row = row;
    grid->mask_pointer = mask;
}

static gboolean
grid_button_press_event (GtkWidget *widget, GdkEventButton *event, grid_t *grid)
{
    if (event->button == 1 && grid->pixmap != NULL)
    {
        int col = grid_x2col (grid, event->x + grid_get_window_xpos (grid));
        int row = grid_y2row (grid, event->y + grid_get_window_ypos (grid));
        if (col >= 0 && col < grid->col_num && row >= 0 && row < grid->row_num)
        {
            grid_toggle_cell (grid, col, row, event->state & GDK_SHIFT_MASK);
            grid_move_pointer (grid, col, row, event->state & GDK_SHIFT_MASK);
        }
    }

    return TRUE;
}

static void
grid_pointer_moved (grid_t *grid, int x, int y, GdkModifierType state)
{
    x += grid_get_window_xpos (grid);
    y += grid_get_window_ypos (grid);
    int col = grid_x2col (grid, x);
    int row = grid_y2row (grid, y);

    if (col >= 0 && col < grid->col_num && row >= 0 && row < grid->row_num)
    {
        if (state & GDK_BUTTON1_MASK && ((col != grid->last_col) || (row != grid->last_row)))
        {
            if ((state & GDK_SHIFT_MASK) && (grid->cells[row][col].mask != grid->last_mask))
                grid_toggle_cell (grid, col, row, 1);
            else if (grid->cells[row][col].value != grid->last_value)
                grid_toggle_cell (grid, col, row, 0);
        }

        grid_move_pointer (grid, col, row, state & GDK_SHIFT_MASK);
    }

    gtk_widget_grab_focus (grid->area);
}

static gboolean
grid_motion_notify_event (GtkWidget *widget, GdkEventMotion *event, grid_t *grid)
{
    int x, y;
    GdkModifierType state;

    if (grid->pixmap == NULL) return TRUE;

    if (event->is_hint)
        gdk_window_get_pointer (event->window, &x, &y, &state);
    else
    {
        x = event->x;
        y = event->y;
        state = event->state;
    }

    grid_pointer_moved (grid, x, y, state);

    return TRUE;
}

static gboolean
grid_key_action_event (GtkWidget * widget, GdkEventKey *event, grid_t * grid)
{
    int col = grid->pointed_col;
    int row = grid->pointed_row;
    //int mask = event->state & GDK_SHIFT_MASK;
    int mask = 0;
    int toggle = 0, moved = 0;
    if (event->type == GDK_KEY_PRESS)
    {
        switch (event->keyval)
        {
            case GDK_KEY_Up:    row--;
                break;
            case GDK_KEY_Right: col++;
                break;
            case GDK_KEY_Down:  row++;
                break;
            case GDK_KEY_Left:  col--;
                break;
            case GDK_KEY_Home:  col = 0;
                break;
            case GDK_KEY_End:   col = grid->col_num - 1;
                break;
            case GDK_KEY_N:
            case GDK_KEY_n:
                mask = 1;
            case GDK_KEY_B:
            case GDK_KEY_b:
            case GDK_KEY_Return:
                toggle = 1;
                break;
                //FIXME: what about GDK_Delete as an eraser?
            default:
                return FALSE;
        }
    }
    else
    {
        switch (event->keyval)
        {
            case GDK_KEY_Shift_L:
            case GDK_KEY_Shift_R:
                mask = 0;
                break;
            default:
                return FALSE;
        }
    }
    if (toggle)
    {
        if (col >= 0 && col < grid->col_num && row >= 0 && row < grid->row_num)
            grid_toggle_cell (grid, col, row, mask);
    }
    else
    {
        row = row < 0 ? grid->row_num - 1 : (row >= grid->row_num ? 0  : row);
        col = col < 0 ? grid->col_num - 1 : (col >= grid->col_num ? 0  : col);
    }

    moved = (row != grid->pointed_row || col != grid->pointed_col);
    grid_move_pointer (grid, col, row, 0);

    if (moved)
    {
        grid_bring_into_view (grid, col, row);
        grid_cell_state_t *state = calloc (1, sizeof (*state));
        state->col    = col;
        state->row    = row;
        event_fire (grid, "pointer-keymoved", state, free);
    }

    return TRUE;
}

gboolean
grid_animation_cb (gpointer data)
{
    grid_t *grid = (grid_t *) data;

    if (!grid->area) return TRUE;

    int col, row;
    grid_cell_t *colp;
    for (row = 0; row < grid->row_num; row++)
        for (col = 0; col < grid->col_num; col++)
        {
            colp = grid->cells[row] + col;
            if (colp->level != colp->next_level)
            {
                if (colp->timecount > GRID_ANIM_LEVEL_TIMEOUT)
                {
                    if (colp->timecount - GRID_ANIM_LEVEL_TIMEOUT > GRID_ANIM_LEVEL_FADEOUT)
                    {
                        colp->level = colp->next_level;
                    }
                    else
                    {
                        float next_level = colp->next_level == -1 ? 0 : colp->next_level;
                        colp->level -= (colp->level - next_level)
                                * (colp->timecount - GRID_ANIM_LEVEL_TIMEOUT) / (float) GRID_ANIM_LEVEL_FADEOUT;
                    }
#ifdef HAVE_GTK_QUARTZ
                    grid_draw_cell (grid, col, row, 1, 0);
#else
                    grid_draw_cell (grid, col, row, 0, 1);
#endif          
                }
            }
            if (colp->timecount < 5000) colp->timecount += GRID_ANIM_INTERVAL;
        }
    return TRUE;
}
