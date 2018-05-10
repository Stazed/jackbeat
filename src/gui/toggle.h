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

#ifndef JACKBEAT_GUI_TOGGLE_H
#define JACKBEAT_GUI_TOGGLE_H

#include "gui/dk.h"

typedef struct toggle_t toggle_t;

toggle_t *  toggle_new (GtkWidget *layout, const char *text);
int         toggle_get_width (toggle_t *toggle);
void        toggle_allocate (toggle_t *toggle, GtkAllocation * alloc);
void        toggle_get_allocation (toggle_t *toggle, GtkAllocation * alloc);
int         toggle_get_active (toggle_t *toggle);
void        toggle_set_active (toggle_t *toggle, int active);
void        toggle_set_hsv_shift(toggle_t *toggle, dk_hsv_t *shift);

#endif
