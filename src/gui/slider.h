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

#ifndef JACKBEAT_GUI_SLIDER_H
#define JACKBEAT_GUI_SLIDER_H

#include "gui/dk.h"

typedef enum {
    SLIDER_FOLDED,
    SLIDER_UNFOLDED
} slider_state_t;

typedef enum {
    SLIDER_LINEAR,
    SLIDER_DECIBEL
} slider_curve_t;

typedef struct slider_t slider_t;

slider_t * slider_new(GtkWidget *layout, GtkAdjustment *adj);
void slider_allocate(slider_t *slider, slider_state_t state, GtkAllocation * alloc);
void slider_get_allocation(slider_t *slider, slider_state_t state, GtkAllocation * alloc);
void slider_add_overlap(slider_t *slider, GtkWidget *widget);
GtkAdjustment * slider_get_adjustment(slider_t *slider);
void slider_set_curve(slider_t *slider, slider_curve_t curve);
slider_state_t slider_get_state(slider_t *slider);

#endif
