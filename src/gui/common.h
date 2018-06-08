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
 *   SVN:$Id: common.h 552 2009-04-30 10:39:30Z olivier $
 */

#ifndef JACKBEAT_GUI_COMMON_H
#define JACKBEAT_GUI_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <libgen.h>
#include <config.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <dirent.h>

#ifdef HAVE_GTK_QUARTZ
#include <ige-mac-menu.h>
#include "Carbon.h"
// TODO: use native mac dialogs. See: 
// http://developer.apple.com/documentation/Carbon/Reference/Navigation_Services_Ref/Reference/reference.html
#endif

#include "song.h"
#include "sequence.h"
#include "sample.h"
#include "gui.h"
#include "jab.h"
#include "error.h"
//#include "grid.h"
#include "gui/sequenceeditor.h"
#include "gui/file.h"
#include "gui/builder.h"
#include "gui/prefs.h"
#include "gui/dk.h"
#include "gui/misc.h"
#include "core/event.h"
#include "core/array.h"
#include "util.h"
#include "osc.h"

#ifdef MEMDEBUG
#include "memdebug.h"
#endif

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#undef USE_KNOB
#ifdef USE_PHAT
#include "gui/phat/phat.h"
#endif

/* GUI object type */
struct gui_t {
    /* nested objects */
    song_t * song;
    sequence_t * sequence;
    rc_t * rc;
    arg_t * arg;
    osc_t *osc;
    stream_t *stream;
    /* flags & info */
    char filename[256];
    int sequence_is_modified;
    int refreshing;
    int filename_is_set;
    int transpose_volumes_round;
    int instance_index;
    int last_export_framerate;
    int last_export_sustain_type;
    char last_export_wdir[512];
    int window_sized;
    int window_grown_by;
    int is_initial;
    /* widgets */
    gui_builder_t * builder;
    GtkWidget * window;
    GtkWidget * main_vbox;
    gui_sequence_editor_t * sequence_editor;
    GtkWidget * tracks_num;
    GtkWidget * beats_num;
    GtkWidget * measure_len;
    GtkWidget * bpm;
    GtkWidget * loop;
    GtkWidget * rewind;
    GtkWidget * progress_window;
    GtkWidget * progress_bar;
#if GTK_CHECK_VERSION(3,0,0)
    GtkWidget * pitch_octave;
    GtkWidget * pitch_semitone;
    GtkWidget * pitch_finetune;
#endif
#ifdef USE_DEPRECIATED
    GtkTooltips * tooltips;
#endif
    
    /* File menu */
    GtkWidget * fileMenu;
    GtkWidget * fileFileMenu;
    GtkWidget * newFileMi;
    GtkWidget * openFileMi;
    GtkWidget * saveFileMi;
    GtkWidget * saveAsFileMi;
    GtkWidget * exportFileMi;
    GtkWidget * closeFileMi;
    GtkWidget * quitFileMi;
    
    /* Edit menu */
    GtkWidget * editMenu;
    GtkWidget * editEditMenu;
    GtkWidget * addTrackEditMi;
    GtkWidget * loadSampleEditMi;
    GtkWidget * renameTrackEditMi;
    GtkWidget * muteTrackEditMi;
    GtkWidget * toggleTrackSoloEditMi;
    GtkWidget * volumeTrackEditMenu;      // submenu
    GtkWidget * trackVolumeEditMenu;
    GtkWidget * volUp2dbVolumeMi;
    GtkWidget * volDown2dbVolumeMi;
    GtkWidget * volSeparator1Mi;
    GtkWidget * volUp3dbVolumeMi;
    GtkWidget * volDown3dbVolumeMi;
    GtkWidget * volSeparator2Mi;
    GtkWidget * volResetVolumeMi;       // end submenu
    GtkWidget * removeTrackEditMi;
    GtkWidget * separatorEditMi;
    GtkWidget * clearEditMi;
    GtkWidget * doubleEditMi;
    GtkWidget * transposeEditMi;
    GtkWidget * clearSoloEditMi;
    GtkWidget * separatorPrefsEditMi;
    GtkWidget * preferencsEditMi;
    
    /* Playback menu */
    GtkWidget * playbackMenu;
    GtkWidget * playbackPlaybackMenu;
    GtkWidget * playPausePlaybackMi;
    GtkWidget * rewindPlaybackMi;
    
    /* Help menu */
    GtkWidget * helpMenu;
    GtkWidget * helpHelpMenu;
    GtkWidget * aboutHelpMi;
    
    GtkWidget * menubar;
    GtkWidget * menubar_quit_item;
    GtkAccelGroup *accel_group;
    gui_prefs_t * prefs;
    //grid_t *              grid;
    GtkWidget * sample_display;
    /* callback helpers */
    gint timeout_tag;
};

void gui_new_child(rc_t *rc, arg_t *arg, gui_t *parent, song_t *song,
        sequence_t *sequence, char *filename, osc_t *osc,
        stream_t *stream);
#define gui_set_modified(G,S) _gui_set_modified(G, S,__func__)

void _gui_set_modified(gui_t * gui, int status, const char *func);
gboolean gui_no_delete(GtkWidget *widget, GdkEvent *event, gpointer data);
void gui_show_progress(gui_t *gui, char *title, char *text);
void gui_hide_progress(gui_t * gui);
void gui_refresh(gui_t * gui);
void gui_progress_callback(char * status, double fraction, void * data);
void gui_display_error(gui_t * gui, char *text);
void gui_display_error_from_window(gui_t * gui, char *text, GtkWidget *window);
int gui_ask_confirmation(gui_t * gui, char *text);
char * gui_get_next_sequence_name(gui_t *);
void gui_enable_timeout(gui_t *gui);
void gui_disable_timeout(gui_t *gui);
void gui_block_handler(gui_t *gui, GtkWidget *widget, gpointer handler);
void gui_unblock_handler(gui_t *gui, GtkWidget *widget, gpointer handler);
void gui_wait_cursor(GdkWindow *window, int state);
GtkWidget * gui_add_menu_item(GtkWidget *menu, char *type, char *label, GCallback callback, gpointer data,
        GClosureNotify destroy_data);
char * gui_ask_track_name(gui_t *gui, char *current_name, int is_name_doublon, int allow_cancel);

#define DEBUG(M, ...) { printf("GUI %.2d  %s(): ",gui->instance_index, __func__); printf(M, ## __VA_ARGS__); printf("\n"); }

#endif
