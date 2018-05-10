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

#ifndef JACKBEAT_GUI_BUILDER_H
#define JACKBEAT_GUI_BUILDER_H

#include <gtk/gtk.h>

typedef struct gui_builder_t gui_builder_t;

gui_builder_t * gui_builder_new(char *filename, void * userdata, ...);
void gui_builder_destroy(gui_builder_t *);
GtkWidget * gui_builder_get_widget(gui_builder_t *, char *name);
void gui_builder_get_widgets(gui_builder_t *, ...);

#endif
