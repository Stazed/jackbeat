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
 *   SVN:$Id: track.h 253 2008-08-21 23:28:27Z olivier $
 */

#ifndef JACKBEAT_GUI_MISC_H
#define JACKBEAT_GUI_MISC_H

#include <gtk/gtk.h>

GtkWidget * gui_misc_add_menu_item (GtkWidget *menu, char *type, char *label, GCallback callback, 
                                    gpointer data, GClosureNotify destroy_data);
void        gui_misc_dropdown_prepend (GtkWidget *combo_box, char *text);
void        gui_misc_dropdown_append (GtkWidget *combo_box, char *text);
char *      gui_misc_dropdown_get_text (GtkWidget *combo_box, int index);
char *      gui_misc_dropdown_get_active_text (GtkWidget *combo_box);
void        gui_misc_dropdown_set_active_text (GtkWidget *combo_box, char *text);
void        gui_misc_dropdown_clear (GtkWidget *combo_box);
void        gui_misc_normalize_dialog_spacing (GtkWidget *widget);

#endif
