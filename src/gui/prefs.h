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
 *   SVN:$Id: $
 */

#ifndef JACKBEAT_GUI_PREFS_H
#define JACKBEAT_GUI_PREFS_H

typedef struct gui_prefs_t gui_prefs_t;

#include "gui/common.h"

void gui_prefs_init(gui_t *gui);
void gui_prefs_cleanup(gui_t *gui);
void gui_prefs_run(GtkWidget * w, gpointer data);
int gui_prefs_is_running(gui_t *gui);
void gui_prefs_update_audio_status(gui_t *gui);

#endif
