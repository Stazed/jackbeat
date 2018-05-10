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
 *   SVN:$Id: grid.h 551 2009-04-30 10:38:56Z olivier $
 */

#include <gtk/gtk.h>

typedef struct grid_t grid_t;

typedef struct {
    int col;
    int row;
    int value;
} grid_cell_state_t;

grid_t *    grid_new                          ();
void        grid_destroy                      (grid_t *grid);
void        grid_resize                       (grid_t *grid, int col_num, int row_num);
GtkWidget * grid_get_widget                   (grid_t *grid);
void        grid_set_value                    (grid_t *grid, int col, int row, int value);
void        grid_set_mask                     (grid_t *grid, int col, int row, int mask);
void        grid_highlight_cell               (grid_t *grid, int col, int row, float level);
void        grid_set_cell_size                (grid_t *grid, int min_width, int max_width, 
                                               int min_height, int max_height);
void        grid_set_spacing                  (grid_t *grid, int min_col_spacing, int row_spacing);
void        grid_set_column_group_size        (grid_t *grid, int column_num);
void        grid_set_header_height            (grid_t *grid, int height);
void        grid_set_header_height_no_redraw  (grid_t *grid, int height);
void        grid_set_vpadding                 (grid_t *grid, int padding);
void        grid_set_header_label_ypos        (grid_t *grid, int y);
void        grid_set_scroll_adjustments       (grid_t* grid, GtkAdjustment *hadj, GtkAdjustment *vadj);
int         grid_get_minimum_width            (grid_t *grid);
int         grid_get_minimum_height           (grid_t *grid);


