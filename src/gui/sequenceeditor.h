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
 *   SVN:$Id: sequenceeditor.h 278 2008-09-01 21:28:50Z olivier $
 */

#ifndef JACKBEAT_GUI_SEQUENCE_EDITOR_H
#define JACKBEAT_GUI_SEQUENCE_EDITOR_H

#include <gtk/gtk.h>
#include "sequence.h"

typedef struct gui_sequence_editor_t gui_sequence_editor_t;

gui_sequence_editor_t * gui_sequence_editor_new (sequence_t *);
GtkWidget *             gui_sequence_editor_get_widget (gui_sequence_editor_t *self);
int                     gui_sequence_editor_get_active_track (gui_sequence_editor_t *);

#endif
