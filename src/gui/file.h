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
 *   SVN:$Id: file.h 508 2009-02-08 16:40:07Z olivier $
 */

#ifndef JACKBEAT_GUI_FILE_H
#define JACKBEAT_GUI_FILE_H

typedef void (* gui_file_selected_callback_t) (gui_t *gui, char *filename, void *data);

void gui_file_load_sample(gui_t *gui, int track);
sequence_t * gui_file_do_load_sequence(gui_t *gui, char *filename);
void gui_file_load_sequence(GtkWidget * w, gpointer data);
void gui_file_export_sequence(GtkWidget * w, gpointer data);
int gui_file_do_save_sequence(gui_t * gui, char *filename);
void gui_file_save_as_sequence(GtkWidget * w, gpointer data);
void gui_file_save_sequence(GtkWidget * w, gpointer data);
GtkWidget * gui_file_build_recent_samples_menu(gui_t *gui, int track);
#endif
