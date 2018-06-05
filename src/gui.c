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
 *   SVN:$Id: gui.c 643 2009-11-23 19:38:59Z olivier $
 */

#include "gui/common.h"
#include <fcntl.h>  /* for fcntl, O_NONBLOCK  - signal handlers */

#define GUI_ANIMATION_INTERVAL 34 // miliseconds

static int      gui_instance_counter  = 0;
static int      gui_instances_num     = 0;
static gui_t ** gui_instances         = NULL;
gui_t *         gui_last_focus        = NULL;
static int      signal_pipe[2];

static void gui_clear_sequence (GtkWidget * w, gpointer data);
static void gui_new_instance (GtkWidget * w, gpointer data);
static void gui_close_from_menu (GtkWidget * w, gpointer data);
static void gui_exit (GtkWidget * w, gpointer data);
static void gui_duplicate_sequence (GtkWidget * w, gpointer data);
static void gui_transpose_volumes_dialog (GtkWidget * w, gpointer data);
static void gui_about_dialog (GtkWidget * w, gpointer data);
static void gui_load_sample (GtkWidget * w, gpointer data);
static void gui_rename_track (GtkWidget * w, gpointer data);
static void gui_mute_track (GtkWidget * w, gpointer data);
static void gui_solo_track (GtkWidget * w, gpointer data);
static void gui_clear_solo (GtkWidget * w, gpointer data);
static void gui_menu_play_clicked (GtkWidget * w, gpointer data);
static void gui_menu_rewind_clicked (GtkWidget * w, gpointer data);
G_MODULE_EXPORT gboolean gui_track_name_focus_lost (GtkWidget * widget, GdkEventFocus *event, gui_t * gui); // Glade callback
static void gui_add_track (GtkWidget * w, gpointer data);
static void gui_remove_track (GtkWidget * w, gpointer data);
static void gui_adjust_track_volume (gui_t * gui, int action);
static void gui_shift_track_volume (GtkWidget * w);
gboolean deliver_signal (GIOChannel *source, GIOCondition cond, gpointer d);
void pipe_signals (int signal);
void install_signal_handlers ();
#ifdef HAVE_GTK_QUARTZ
static void gui_mac_prefs_run (gui_t *gui, guint action, GtkWidget *widget);
#endif

enum
{
    GUI_SHIFT_RESET = 1,
    GUI_SHIFT_DOWN,
    GUI_SHIFT_UP,
    GUI_SHIFT_DOWN_BIG,
    GUI_SHIFT_UP_BIG
} ;

#ifdef USE_DEPRECIATED
static GtkItemFactoryEntry gui_menu_items[] = {
    /* File menu */
    {"/_File", NULL, NULL, 0, "<Branch>"},
    {"/File/New", "<control>N", gui_new_instance, 1, "<StockItem>",
        GTK_STOCK_NEW},
    {"/File/Open", "<control>O", gui_file_load_sequence, 1, "<StockItem>",
        GTK_STOCK_OPEN},
    {"/File/Save", "<control>S", gui_file_save_sequence, 1, "<StockItem>",
        GTK_STOCK_SAVE},
    {"/File/Save as", "<control><shift>S", gui_file_save_as_sequence, 1, "<StockItem>",
        GTK_STOCK_SAVE_AS},
    {"/File/Export waveform", "<control>E", gui_file_export_sequence, 1, "<StockItem>",
        GTK_STOCK_CONVERT},
    {"/File/Close", "<control>W", gui_close_from_menu, 1, "<StockItem>",
        GTK_STOCK_CLOSE},
#ifndef HAVE_GTK_QUARTZ
    {"/File/quit_separator", NULL, NULL, 0, "<Separator>"},
#endif  
    {"/File/Quit", "<control>Q", gui_exit, 1, "<StockItem>",
        GTK_STOCK_QUIT},

    /* Edit menu */
    {"/_Edit", NULL, NULL, 0, "<Branch>"},
    {"/Edit/Add track", "<control><shift>N", gui_add_track, 1, "<StockItem>", GTK_STOCK_ADD},
    {"/Edit/Load sample", "<control>L", gui_load_sample, 1, "<StockItem>", GTK_STOCK_OPEN},
    {"/Edit/Rename track", "F2", gui_rename_track, 1, "<Item>"},
    {"/Edit/Mute\\/Unmute track", "M", gui_mute_track, 1, "<Item>"},
    {"/Edit/Toggle track solo", "S", gui_solo_track, 1, "<Item>"},
    {"/Edit/Track volume", NULL, NULL, 0, "<Branch>"},
    /* Warning: the track volume accelerators are also handled in
       gui_hijack_key_press() */
    {"/Edit/Track volume/Increase by 0.2dB", "P", gui_shift_track_volume, GUI_SHIFT_UP, "<Item>"},
    {"/Edit/Track volume/Decrease by 0.2dB", "L", gui_shift_track_volume, GUI_SHIFT_DOWN, "<Item>"},
    {"/Edit/Track volume/track_vol_sep1", NULL, NULL, 0, "<Separator>"},
    {"/Edit/Track volume/Increase by 3dB", "<shift>P", gui_shift_track_volume, GUI_SHIFT_UP_BIG, "<Item>"},
    {"/Edit/Track volume/Decrease by 3dB", "<shift>L", gui_shift_track_volume, GUI_SHIFT_DOWN_BIG, "<Item>"},
    {"/Edit/Track volume/track_vol_sep2", NULL, NULL, 0, "<Separator>"},
    {"/Edit/Track volume/Reset to 0dB", "O", gui_shift_track_volume, GUI_SHIFT_RESET, "<Item>"},
    {"/Edit/Remove track", "<control>Delete", gui_remove_track, 1, "<StockItem>", GTK_STOCK_REMOVE},
    {"/Edit/edit_separator", NULL, NULL, 0, "<Separator>"},
    {"/Edit/Clear pattern", "<control><shift>K", gui_clear_sequence, 1, "<StockItem>", GTK_STOCK_DELETE},
    {"/Edit/Double", "<control><shift>D", gui_duplicate_sequence, 1, "<Item>"},
    {"/Edit/Transpose volumes", "<control><shift>T", gui_transpose_volumes_dialog, 1, "<Item>"},
    {"/Edit/Clear solo", "X", gui_clear_solo, 1, "<Item>"},
#ifdef HAVE_GTK_QUARTZ
    {"/Edit/Preferences", NULL, gui_mac_prefs_run, 1, "<StockItem>", GTK_STOCK_PREFERENCES},
#else
    {"/Edit/prefs_separator", NULL, NULL, 0, "<Separator>"},
    {"/Edit/Preferences", NULL, gui_prefs_run, 1, "<StockItem>", GTK_STOCK_PREFERENCES},
#endif  

    /* Playback menu */
    {"/_Playback", NULL, NULL, 0, "<Branch>"},
    {"/Playback/Play\\/Pause", "space", gui_menu_play_clicked, 1, "<StockItem>", GTK_STOCK_MEDIA_PLAY},
    {"/Playback/Rewind", "Z", gui_menu_rewind_clicked, 1, "<StockItem>", GTK_STOCK_MEDIA_REWIND},

    /* Help menu */
    {"/_Help", NULL, NULL, 0, "<Branch>"},
    {"/_Help/_About Jackbeat", NULL, gui_about_dialog, 1, "<StockItem>", GTK_STOCK_ABOUT},
};


static gint gui_menu_nitems =
        sizeof (gui_menu_items) / sizeof (gui_menu_items[0]);
#endif // USE_DEPRECIATED

void gui_toggle_sequence_properties (GtkWidget *widget, gui_t *gui);
void gui_toggle_track_properties (GtkWidget *widget, gui_t *gui);
static void gui_update_track_properties (gui_t *gui);

static void
gui_update_window_title (gui_t * gui)
{
    char s[128];
    char *name = sequence_get_name (gui->sequence);
    sprintf (s, "%s%s - %s", (gui->sequence_is_modified) ? "*" : "",
             basename (gui->filename), name);
    free (name);
    gtk_window_set_title (GTK_WINDOW (gui->window), s);
}

void
_gui_set_modified (gui_t * gui, int status, const char *func)
{
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("from: %s", func);
#endif
    gui->sequence_is_modified = status;
    gui_update_window_title (gui);
}

static void
gui_on_transport_changed (event_t *event)
{
    gui_t *gui = (gui_t *) event->self;
    sequence_t *sequence = (sequence_t *) event->source;
    sequence_get_transport (sequence, &gui->rc->transport_aware, &gui->rc->transport_query);

#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("transport: aware=%d, query=%d", gui->rc->transport_aware, gui->rc->transport_query);
#endif

    if (gui->rewind)
        gtk_widget_set_sensitive (gui->rewind, (gboolean) gui->rc->transport_query);
}

void
gui_display_error_from_window (gui_t * gui, char *text, GtkWidget *window)
{
    GtkWidget *dialog;

    dialog =
            gtk_message_dialog_new (window ? GTK_WINDOW (window) : NULL,
                                    GTK_DIALOG_DESTROY_WITH_PARENT |
                                    GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK, "%s", text);

    gtk_window_set_title (GTK_WINDOW (dialog), "Jackbeat");
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}

void
gui_display_error (gui_t * gui, char *text)
{
    gui_display_error_from_window (gui, text, gui->window);
}

int
gui_ask_confirmation (gui_t * gui, char *text)
{
    GtkWidget *dialog;
    gint response;

    dialog =
            gtk_message_dialog_new (gui->window ? GTK_WINDOW (gui->window) : NULL,
                                    GTK_DIALOG_DESTROY_WITH_PARENT |
                                    GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
                                    GTK_BUTTONS_OK_CANCEL, "%s", text);
    gtk_window_set_title (GTK_WINDOW (dialog), "Jackbeat");

    response = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);

    if (response == GTK_RESPONSE_OK)
        return 1;

    return 0;
}

gboolean
gui_no_delete (GtkWidget *widget, GdkEvent  *event, gpointer data)
{
    return TRUE;
}

static void
gui_transpose_volumes (GtkWidget *adj, gui_t *gui)
{
    GtkWidget *master = g_object_get_data (G_OBJECT (adj), "user-data");
    double *orig_volumes = g_object_get_data (G_OBJECT (master), "user-data");
    double mvol = (double) gtk_spin_button_get_value (GTK_SPIN_BUTTON (master));
    int ntracks = sequence_get_tracks_num (gui->sequence);
    int i;
    for (i = 0; i < ntracks; i++)
    {
        double tvol = orig_volumes[i] * mvol / 100;
        if (gui->transpose_volumes_round) tvol =  floor (tvol);
        sequence_set_volume (gui->sequence, i, tvol);
    }
}

static void
gui_transpose_volumes_toggle_round (GtkWidget * widget, gui_t * gui)
{
    gui->transpose_volumes_round =
            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)) ? 1 : 0;
    if (gui->transpose_volumes_round)
    {
        GtkWidget *adj = g_object_get_data (G_OBJECT (widget), "user-data");
        gui_transpose_volumes (adj, gui);
    }
}

static void
gui_transpose_volumes_dialog (GtkWidget *widget, gpointer data)
{
    gui_t *gui = data;
    GtkWidget * dialog;
#if GTK_CHECK_VERSION(3,0,0)
    GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    
#else
    GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
    GtkWidget *vbox = gtk_vbox_new (FALSE, 0);
#endif
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 15);

    int ntracks = sequence_get_tracks_num (gui->sequence);
    double *orig_volumes = calloc (ntracks, sizeof (double));
    int i;
    for (i = 0; i < ntracks; i++)
        orig_volumes[i] = sequence_get_volume (gui->sequence, i);

    char s[128];
    char *name = sequence_get_name (gui->sequence);
    sprintf (s, "Transpose volumes - %s", name);

    dialog = gtk_dialog_new_with_buttons (
                                          s,
                                          GTK_WINDOW (gui->window),
                                          GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                          "_Close", GTK_RESPONSE_ACCEPT, NULL);

    sprintf (s, "[%s] Transpose all volumes by (%%) :",  name);
    free (name);
    GtkWidget *label = gtk_label_new (s);
    gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 8);


    GtkAdjustment *adj = (GtkAdjustment *)
            gtk_adjustment_new (100, 0, 999, 1, 0.01, 0);
    GtkWidget *master = gtk_spin_button_new (adj, 0.5, 2);
    g_object_set_data (G_OBJECT (adj), "user-data", (gpointer) master);
    g_object_set_data (G_OBJECT (master), "user-data", (gpointer) orig_volumes);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (master),
                                       GTK_UPDATE_IF_VALID);
    g_signal_connect (G_OBJECT (adj), "value_changed",
                      G_CALLBACK (gui_transpose_volumes), (gpointer) gui);

    gtk_box_pack_start (GTK_BOX (vbox), master, TRUE, TRUE, 8);

    GtkWidget *button = gtk_check_button_new_with_label ("Round values");
    g_object_set_data (G_OBJECT (button), "user-data", (gpointer) adj);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                  gui->transpose_volumes_round ? TRUE : FALSE);
    g_signal_connect (G_OBJECT (button), "toggled",
                      G_CALLBACK (gui_transpose_volumes_toggle_round), (gpointer) gui);

    gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 8);

    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area(GTK_DIALOG (dialog))), hbox, TRUE, TRUE,
                        0);
    gtk_widget_show_all (dialog);

    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    free (orig_volumes);
}

void
gui_wait_cursor (GdkWindow *window, int state)
{
    GdkCursor *cursor = state ? gdk_cursor_new (GDK_WATCH) : NULL;
    gdk_window_set_cursor (window, cursor);
    gtk_main_iteration_do (FALSE);
}

static void
gui_about_dialog (GtkWidget * w, gpointer data)
{
    gui_t *gui = data;
    
    GdkPixbuf *logo = gdk_pixbuf_new_from_file (util_pkgdata_path ("pixmaps/jackbeat_logo.png"), NULL);
    char *license;
    g_file_get_contents (util_pkgdata_path ("help/COPYING"), &license, NULL, NULL);

    gtk_show_about_dialog (GTK_WINDOW (gui->window),
                           "program-name", "Jackbeat",
                           "version", VERSION,
                           "logo", logo,
                           "copyright", "Copyright (c) 2004-2009 Olivier Guilyardi",
                           "website", "http://jackbeat.samalyse.org",
                           "license", license,
                           NULL);
    g_object_unref (logo);
    g_free (license);
}

void
gui_show_progress (gui_t *gui, char *title, char *text)
{
    gui->progress_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    if (gui->window != NULL)
    {
        gtk_window_set_modal (GTK_WINDOW (gui->progress_window), TRUE);
        gtk_window_set_transient_for (GTK_WINDOW (gui->progress_window),
                                      GTK_WINDOW (gui->window));
        gtk_window_set_destroy_with_parent (GTK_WINDOW (gui->progress_window), TRUE);
    }
    gtk_window_set_default_size (GTK_WINDOW (gui->progress_window), 350, 50);
    gtk_window_set_position (GTK_WINDOW (gui->progress_window), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_title (GTK_WINDOW (gui->progress_window), title);
    g_signal_connect (G_OBJECT (gui->progress_window), "delete_event",
                      G_CALLBACK (gui_no_delete), NULL);


#if GTK_CHECK_VERSION(3,0,0)
    GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *hbox_top = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
#else
    GtkWidget *vbox = gtk_vbox_new (FALSE, 0);
    GtkWidget *hbox_top = gtk_hbox_new (FALSE, 0);
#endif

    gtk_container_add (GTK_CONTAINER (gui->progress_window), vbox);

    GtkWidget *spacer = gtk_label_new (" ");
    gtk_box_pack_start (GTK_BOX (vbox), spacer, TRUE, TRUE, 0);

    gtk_box_pack_start (GTK_BOX (vbox), hbox_top, TRUE, TRUE, 0);

    GtkWidget *icon = gtk_image_new_from_icon_name ("dialog-information", GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start (GTK_BOX (hbox_top), icon, FALSE, FALSE, 15);

    GtkWidget *label = gtk_label_new (text);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.2);
    gtk_box_pack_start (GTK_BOX (hbox_top), label, FALSE, FALSE, 0);

    spacer = gtk_label_new (" ");
    gtk_box_pack_start (GTK_BOX (vbox), spacer, TRUE, TRUE, 0);
#if GTK_CHECK_VERSION(3,0,0)
    GtkWidget *hbox_bot = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
#else
    GtkWidget *hbox_bot = gtk_hbox_new (FALSE, 0);
#endif
    gtk_box_pack_start (GTK_BOX (vbox), hbox_bot, TRUE, TRUE, 0);

    gui->progress_bar = gtk_progress_bar_new ();
    gtk_box_pack_start (GTK_BOX (hbox_bot), gui->progress_bar, TRUE, TRUE, 15);

    spacer = gtk_label_new (" ");
    gtk_box_pack_start (GTK_BOX (vbox), spacer, TRUE, TRUE, 0);

    gtk_widget_show_all (vbox);

}

void
gui_progress_callback (char * status, double fraction, void * data)
{
    gui_t *gui = (gui_t *) data;
    if (!gtk_widget_get_visible(gui->progress_window))
        gtk_widget_show (gui->progress_window);
    gtk_progress_bar_set_text (GTK_PROGRESS_BAR (gui->progress_bar), status);
    if (fraction > 1)
    {
        DEBUG ("Warning: progress fraction is greater than 1. Correcting..");
        fraction = 1;
    }
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (gui->progress_bar), fraction);
    while (g_main_context_iteration (NULL, FALSE));
}

void
gui_hide_progress (gui_t * gui)
{
    gtk_widget_destroy (gui->progress_window);
}

static void
gui_clear_sequence (GtkWidget * w, gpointer data)
{
    gui_t * gui = data;
    if (!gui->sequence_is_modified || gui_ask_confirmation
        (gui, "This will clear the whole pattern. Are you sure ?"))
    {
        int ntracks = sequence_get_tracks_num (gui->sequence);
        int nbeats = sequence_get_beats_num (gui->sequence);
        int i, j;
        for (i = 0; i < ntracks; i++)
            for (j = 0; j < nbeats; j++)
            {
                sequence_set_beat (gui->sequence, i, j, 0);
                sequence_set_mask_beat (gui->sequence, i, j, 1);
            }
    }
}


static GtkWidget *
gui_menubar (gui_t * gui)
{
    gui->menubar = gtk_menu_bar_new();
    
    gui->fileMenu = gtk_menu_new();
    
    /* File menu */
    gui->fileFileMenu = gtk_menu_item_new_with_mnemonic("_File");
    gui->newFileMi = gtk_image_menu_item_new_from_stock(GTK_STOCK_NEW, gui->accel_group);
    gui->openFileMi = gtk_image_menu_item_new_from_stock(GTK_STOCK_OPEN, gui->accel_group);
    gui->saveFileMi = gtk_image_menu_item_new_from_stock(GTK_STOCK_SAVE, gui->accel_group);
    gui->saveAsFileMi = gtk_image_menu_item_new_from_stock(GTK_STOCK_SAVE_AS, gui->accel_group);
    gui->exportFileMi = gtk_menu_item_new_with_label("Export waveform");
    gui->closeFileMi = gtk_image_menu_item_new_from_stock(GTK_STOCK_CLOSE, gui->accel_group);
    gui->quitFileMi = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, gui->accel_group);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(gui->fileFileMenu), gui->fileMenu);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->fileMenu), gui->newFileMi);
    gtk_widget_add_accelerator(gui->newFileMi, "activate", gui->accel_group, 
        GDK_KEY_n, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->newFileMi), "activate", G_CALLBACK (gui_new_instance), gui);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->fileMenu), gui->openFileMi);
    gtk_widget_add_accelerator(gui->openFileMi, "activate", gui->accel_group, 
        GDK_KEY_o, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->openFileMi), "activate", G_CALLBACK (gui_file_load_sequence), gui);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->fileMenu), gui->saveFileMi);
    gtk_widget_add_accelerator(gui->saveFileMi, "activate", gui->accel_group, 
        GDK_KEY_s, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->saveFileMi), "activate", G_CALLBACK (gui_file_save_sequence), gui);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->fileMenu), gui->saveAsFileMi);
    gtk_widget_add_accelerator(gui->saveAsFileMi, "activate", gui->accel_group, 
        GDK_KEY_s, GDK_CONTROL_MASK|GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->saveAsFileMi), "activate", G_CALLBACK (gui_file_save_as_sequence), gui);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->fileMenu), gui->exportFileMi);
    gtk_widget_add_accelerator(gui->exportFileMi, "activate", gui->accel_group, 
        GDK_KEY_e, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->exportFileMi), "activate", G_CALLBACK (gui_file_export_sequence), gui);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->fileMenu), gui->closeFileMi);
    gtk_widget_add_accelerator(gui->closeFileMi, "activate", gui->accel_group, 
        GDK_KEY_w, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->closeFileMi), "activate", G_CALLBACK (gui_close_from_menu), gui);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->fileMenu), gui->quitFileMi);
    gtk_widget_add_accelerator(gui->quitFileMi, "activate", gui->accel_group, 
        GDK_KEY_q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->quitFileMi), "activate", G_CALLBACK (gui_exit), gui);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->menubar), gui->fileFileMenu);
    
    /* Edit menu */
    gui->editMenu = gtk_menu_new();
    gui->editEditMenu = gtk_menu_item_new_with_mnemonic("_Edit");
    gui->addTrackEditMi = gtk_menu_item_new_with_label("Add track");
    gui->loadSampleEditMi = gtk_menu_item_new_with_label("Load sample");
    gui->renameTrackEditMi = gtk_menu_item_new_with_label("Rename track");
    gui->muteTrackEditMi = gtk_menu_item_new_with_label("Mute/Unmute track");
    gui->toggleTrackSoloEditMi = gtk_menu_item_new_with_label("Toggle track solo");
    gui->volumeTrackEditMenu = gtk_menu_new();     // submenu
    gui->trackVolumeEditMenu = gtk_menu_item_new_with_label("Track volume");
    gui->volUp2dbVolumeMi = gtk_menu_item_new_with_label("Track volume/Increase by 0.2dB");
    gui->volDown2dbVolumeMi = gtk_menu_item_new_with_label("Track volume/Decrease by 0.2dB");
    gui->volSeparator1Mi = gtk_separator_menu_item_new(); 
    gui->volUp3dbVolumeMi = gtk_menu_item_new_with_label("Track volume/Increase by 3dB");
    gui->volDown3dbVolumeMi = gtk_menu_item_new_with_label("Track volume/Decrease by 3dB");
    gui->volSeparator2Mi = gtk_separator_menu_item_new(); 
    gui->volResetVolumeMi = gtk_menu_item_new_with_label("Track volume/Reset to 0dB"); // end submenu
    gui->removeTrackEditMi = gtk_menu_item_new_with_label("Remove track");
    gui->separatorEditMi = gtk_separator_menu_item_new(); 
    gui->clearEditMi = gtk_menu_item_new_with_label("Clear pattern");
    gui->doubleEditMi = gtk_menu_item_new_with_label("Double");
    gui->transposeEditMi = gtk_menu_item_new_with_label("Transpose volumes");
    gui->clearSoloEditMi = gtk_menu_item_new_with_label("Clear solo");
    gui->separatorPrefsEditMi = gtk_separator_menu_item_new(); 
    gui->preferencsEditMi = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(gui->editEditMenu), gui->editMenu);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->editMenu), gui->addTrackEditMi);
    gtk_widget_add_accelerator(gui->addTrackEditMi, "activate", gui->accel_group, 
        GDK_KEY_n, GDK_CONTROL_MASK|GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->addTrackEditMi), "activate", G_CALLBACK (gui_add_track), gui);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->editMenu), gui->loadSampleEditMi);
    gtk_widget_add_accelerator(gui->loadSampleEditMi, "activate", gui->accel_group, 
        GDK_KEY_l, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->loadSampleEditMi), "activate", G_CALLBACK (gui_load_sample), gui);

    gtk_menu_shell_append(GTK_MENU_SHELL(gui->editMenu), gui->renameTrackEditMi);
    gtk_widget_add_accelerator(gui->renameTrackEditMi, "activate", gui->accel_group, 
        GDK_KEY_F2, 0, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->renameTrackEditMi), "activate", G_CALLBACK (gui_rename_track), gui);

    gtk_menu_shell_append(GTK_MENU_SHELL(gui->editMenu), gui->muteTrackEditMi);
    gtk_widget_add_accelerator(gui->muteTrackEditMi, "activate", gui->accel_group, 
        GDK_KEY_m, 0, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->muteTrackEditMi), "activate", G_CALLBACK (gui_mute_track), gui);

    gtk_menu_shell_append(GTK_MENU_SHELL(gui->editMenu), gui->toggleTrackSoloEditMi);
    gtk_widget_add_accelerator(gui->toggleTrackSoloEditMi, "activate", gui->accel_group, 
        GDK_KEY_s, 0, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->toggleTrackSoloEditMi), "activate", G_CALLBACK (gui_solo_track), gui);
    
    /* Track Volume sub menu */
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(gui->trackVolumeEditMenu), gui->volumeTrackEditMenu);

    gtk_menu_shell_append(GTK_MENU_SHELL(gui->volumeTrackEditMenu), gui->volUp2dbVolumeMi);
    gtk_widget_add_accelerator(gui->volUp2dbVolumeMi, "activate", gui->accel_group, 
        GDK_KEY_p, 0, GTK_ACCEL_VISIBLE); 
    g_object_set_data(G_OBJECT(gui->volUp2dbVolumeMi), "volume_adjust", GINT_TO_POINTER(GUI_SHIFT_UP));
    g_object_set_data(G_OBJECT(gui->volUp2dbVolumeMi), "gui", gui);
    g_signal_connect(G_OBJECT(gui->volUp2dbVolumeMi), "activate", G_CALLBACK (gui_shift_track_volume), 0);    // GUI_SHIFT_UP
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->volumeTrackEditMenu), gui->volDown2dbVolumeMi);
    gtk_widget_add_accelerator(gui->volDown2dbVolumeMi, "activate", gui->accel_group, 
        GDK_KEY_l, 0, GTK_ACCEL_VISIBLE); 
    g_object_set_data(G_OBJECT(gui->volDown2dbVolumeMi), "volume_adjust", GINT_TO_POINTER(GUI_SHIFT_DOWN));
    g_object_set_data(G_OBJECT(gui->volDown2dbVolumeMi), "gui", gui);
    g_signal_connect(G_OBJECT(gui->volDown2dbVolumeMi), "activate", G_CALLBACK (gui_shift_track_volume), 0);  // GUI_SHIFT_DOWN
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->volumeTrackEditMenu), gui->volSeparator1Mi);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->volumeTrackEditMenu), gui->volUp3dbVolumeMi);
    gtk_widget_add_accelerator(gui->volUp3dbVolumeMi, "activate", gui->accel_group, 
        GDK_KEY_p, GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE); 
    g_object_set_data(G_OBJECT(gui->volUp3dbVolumeMi), "volume_adjust", GINT_TO_POINTER(GUI_SHIFT_UP_BIG));
    g_object_set_data(G_OBJECT(gui->volUp3dbVolumeMi), "gui", gui);
    g_signal_connect(G_OBJECT(gui->volUp3dbVolumeMi), "activate", G_CALLBACK (gui_shift_track_volume), 0);    // GUI_SHIFT_UP_BIG
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->volumeTrackEditMenu), gui->volDown3dbVolumeMi);
    gtk_widget_add_accelerator(gui->volDown3dbVolumeMi, "activate", gui->accel_group, 
        GDK_KEY_l, GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE); 
    g_object_set_data(G_OBJECT(gui->volDown3dbVolumeMi), "volume_adjust", GINT_TO_POINTER(GUI_SHIFT_DOWN_BIG));
    g_object_set_data(G_OBJECT(gui->volDown3dbVolumeMi), "gui", gui);
    g_signal_connect(G_OBJECT(gui->volDown3dbVolumeMi), "activate", G_CALLBACK (gui_shift_track_volume), 0);  // GUI_SHIFT_DOWN_BIG
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->volumeTrackEditMenu), gui->volSeparator2Mi);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->volumeTrackEditMenu), gui->volResetVolumeMi);
    gtk_widget_add_accelerator(gui->volResetVolumeMi, "activate", gui->accel_group, 
        GDK_KEY_0, 0, GTK_ACCEL_VISIBLE); 
    g_object_set_data(G_OBJECT(gui->volResetVolumeMi), "volume_adjust", GINT_TO_POINTER(GUI_SHIFT_RESET));
    g_object_set_data(G_OBJECT(gui->volResetVolumeMi), "gui", gui);
    g_signal_connect(G_OBJECT(gui->volResetVolumeMi), "activate", G_CALLBACK (gui_shift_track_volume), 0);    // GUI_SHIFT_RESET
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->editMenu), gui->trackVolumeEditMenu);
    
    /* End track volume sub menu */
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->editMenu), gui->removeTrackEditMi);
    gtk_widget_add_accelerator(gui->removeTrackEditMi, "activate", gui->accel_group, 
        GDK_KEY_Delete, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->removeTrackEditMi), "activate", G_CALLBACK (gui_remove_track), gui);

    gtk_menu_shell_append(GTK_MENU_SHELL(gui->editMenu), gui->separatorEditMi);

    gtk_menu_shell_append(GTK_MENU_SHELL(gui->editMenu), gui->clearEditMi);
    gtk_widget_add_accelerator(gui->clearEditMi, "activate", gui->accel_group, 
        GDK_KEY_k, GDK_CONTROL_MASK|GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->clearEditMi), "activate", G_CALLBACK (gui_clear_sequence), gui);

    gtk_menu_shell_append(GTK_MENU_SHELL(gui->editMenu), gui->doubleEditMi);
    gtk_widget_add_accelerator(gui->doubleEditMi, "activate", gui->accel_group, 
        GDK_KEY_d, GDK_CONTROL_MASK|GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->doubleEditMi), "activate", G_CALLBACK (gui_duplicate_sequence), gui);

    gtk_menu_shell_append(GTK_MENU_SHELL(gui->editMenu), gui->transposeEditMi);
    gtk_widget_add_accelerator(gui->transposeEditMi, "activate", gui->accel_group, 
        GDK_KEY_t, GDK_CONTROL_MASK|GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->transposeEditMi), "activate", G_CALLBACK (gui_transpose_volumes_dialog), gui);

    gtk_menu_shell_append(GTK_MENU_SHELL(gui->editMenu), gui->clearSoloEditMi);
    gtk_widget_add_accelerator(gui->clearSoloEditMi, "activate", gui->accel_group, 
        GDK_KEY_x, 0, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->clearSoloEditMi), "activate", G_CALLBACK (gui_clear_solo), gui);

    gtk_menu_shell_append(GTK_MENU_SHELL(gui->editMenu), gui->separatorPrefsEditMi);

    gtk_menu_shell_append(GTK_MENU_SHELL(gui->editMenu), gui->preferencsEditMi);
    g_signal_connect(G_OBJECT(gui->preferencsEditMi), "activate", G_CALLBACK (gui_prefs_run), gui);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->menubar), gui->editEditMenu);
    
    /* Playback menu */
    gui->playbackMenu = gtk_menu_new();
    gui->playbackPlaybackMenu = gtk_menu_item_new_with_mnemonic("_Playback");
    gui->playPausePlaybackMi = gtk_image_menu_item_new_from_stock(GTK_STOCK_MEDIA_PLAY, NULL);
    gui->rewindPlaybackMi = gtk_image_menu_item_new_from_stock(GTK_STOCK_MEDIA_REWIND, NULL);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(gui->playbackPlaybackMenu), gui->playbackMenu);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->playbackMenu), gui->playPausePlaybackMi);
    gtk_widget_add_accelerator(gui->playPausePlaybackMi, "activate", gui->accel_group, 
        GDK_KEY_space, 0, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->playPausePlaybackMi), "activate", G_CALLBACK (gui_menu_play_clicked), gui);

    gtk_menu_shell_append(GTK_MENU_SHELL(gui->playbackMenu), gui->rewindPlaybackMi);
    gtk_widget_add_accelerator(gui->rewindPlaybackMi, "activate", gui->accel_group, 
        GDK_KEY_z, 0, GTK_ACCEL_VISIBLE); 
    g_signal_connect(G_OBJECT(gui->rewindPlaybackMi), "activate", G_CALLBACK (gui_menu_rewind_clicked), gui);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->menubar), gui->playbackPlaybackMenu);
    
    /* Help menu */
    gui->helpMenu = gtk_menu_new();
    gui->helpHelpMenu  = gtk_menu_item_new_with_mnemonic("_Help");
    gui->aboutHelpMi  = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT, NULL);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(gui->helpHelpMenu), gui->helpMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->helpMenu), gui->aboutHelpMi);
    g_signal_connect(G_OBJECT(gui->aboutHelpMi), "activate", G_CALLBACK (gui_about_dialog), gui);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(gui->menubar), gui->helpHelpMenu);
    
    return gui->menubar;
}

#ifdef USE_DEPRECIATED
static GtkWidget *
gui_make_menubar (gui_t * gui, GtkItemFactoryEntry * items, gint nitems)
{
    GtkItemFactory *item_factory;
    GtkAccelGroup *accel_group;

    accel_group = gtk_accel_group_new ();
    item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>",
                                         accel_group);
    gtk_item_factory_create_items (item_factory, nitems, items, (gpointer) gui);
    gtk_window_add_accel_group (GTK_WINDOW (gui->window), accel_group);

    gui->menubar_quit_item  = gtk_item_factory_get_widget (item_factory,
                                                           "/File/Quit");

#ifdef HAVE_GTK_QUARTZ
    static int first_run = 1;
    if (first_run)
    {
        GtkWidget *item = gtk_item_factory_get_widget (item_factory, "/Edit/Preferences");
        IgeMacMenuGroup *group = ige_mac_menu_add_app_menu_group ();
        ige_mac_menu_add_app_menu_item (group, GTK_MENU_ITEM (item), NULL);
    }
    first_run = 0;
#endif

    return gtk_item_factory_get_widget (item_factory, "<main>");
}
#endif // USE_DEPRECIATED

static void
gui_set_handler_active (gui_t *gui, GtkWidget *widget, gpointer handler, int state)
{
    gpointer instance = widget;
    /*
    if (GTK_IS_SPIN_BUTTON (widget)) 
      instance = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));
  #ifdef USE_PHAT    
    else if (PHAT_IS_KNOB (widget)) 
      instance = phat_knob_get_adjustment (PHAT_KNOB (widget));
    else if (PHAT_IS_FAN_SLIDER (widget)) 
      instance = phat_fan_slider_get_adjustment (PHAT_FAN_SLIDER (widget));
  #endif    
     */

    if (state)
    {
        if (!g_signal_handlers_unblock_matched (instance, G_SIGNAL_MATCH_FUNC,
                                                0, 0, NULL, handler, NULL))
        {
            DEBUG ("Warning: no handler matched");
        }
    }
    else
    {
        if (!g_signal_handlers_block_matched (instance, G_SIGNAL_MATCH_FUNC,
                                              0, 0, NULL, handler, NULL))
        {
            DEBUG ("Warning: no handler matched");
        }
    }
}

void
gui_block_handler (gui_t *gui, GtkWidget *widget, gpointer handler)
{
    gui_set_handler_active (gui, widget, handler, 0);
}

void
gui_unblock_handler (gui_t *gui, GtkWidget *widget, gpointer handler)
{
    gui_set_handler_active (gui, widget, handler, 1);
}

static void
gui_on_sequence_modified (event_t *event)
{
    gui_t *gui = (gui_t *) event->self;
    gui_set_modified (gui, 1);
}

static void
gui_on_desktop_open_action (event_t *event)
{
    sequence_t *sequence;
    gui_t *gui = gui_last_focus;
    printf ("Received desktop action event\n");
    if (!gui)
    {
        printf ("ERROR: received desktop open event but can't find the last "
                "focused gui\n");
        return;
    }
    char *filename = (char *) event->data;
    if ((sequence = gui_file_do_load_sequence (gui, filename)))
        gui_new_child (gui->rc, gui->arg, gui, gui->song, sequence, filename, gui->osc, gui->stream);
}

void
gui_refresh (gui_t * gui)
{
    if (!gui->refreshing)
    {
#ifdef PRINT_EXTRA_DEBUG
        DEBUG ("Refreshing GUI");
#endif
        gui->refreshing = 1;
        gui_update_window_title (gui);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (gui->tracks_num),
                                   sequence_get_tracks_num (gui->sequence));
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (gui->beats_num),
                                   sequence_get_beats_num (gui->sequence));
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (gui->measure_len),
                                   sequence_get_measure_len (gui->sequence));
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (gui->bpm), sequence_get_bpm (gui->sequence));
        if (sequence_is_looping (gui->sequence))
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gui->loop), TRUE);
        else
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gui->loop), FALSE);

        //gui_track_draw (gui);
        gui_update_track_properties (gui);
        gui->refreshing = 0;
    }
}

void
gui_do_exit (gui_t *gui)
{
    DEBUG ("Writing rc settings");
    rc_write (gui->rc);
    DEBUG ("Exiting GTK");
    gtk_main_quit ();
    DEBUG ("Bye");
}

gboolean
gui_timeout (gpointer data)
{
    gui_t *gui = (gui_t *) data;
    int active_track = gui_sequence_editor_get_active_track (gui->sequence_editor);
    if (sequence_is_playing (gui->sequence))
        sample_display_set_mixer_position (SAMPLE_DISPLAY (gui->sample_display),
                                           sequence_get_sample_position (gui->sequence, active_track));

    //sequence_process_events (gui->sequence);
    event_process_queue (gui);
    event_process_queue (NULL);

    return TRUE;
}

void
gui_enable_timeout (gui_t *gui)
{
    gui->timeout_tag = g_timeout_add (GUI_ANIMATION_INTERVAL, gui_timeout, (gpointer) gui);
}

void
gui_disable_timeout (gui_t *gui)
{
    g_source_remove (gui->timeout_tag);
}

int
gui_confirm_exit (gui_t *gui, int transient, char *msg)
{
    int confirm;
    GtkWidget * dialog = gui_builder_get_widget (gui->builder, "close_warning");

    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gui->window));
    if (transient)
    {
        gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    }
    else
    {
        gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    }

    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", msg);
    confirm = (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK);
    gtk_widget_hide (dialog);
    return confirm;
}

G_MODULE_EXPORT gboolean
gui_close (GtkWidget * widget, GdkEvent  *event, gui_t * gui) // Glade callback
{
    if (!gui->sequence_is_modified || gui_confirm_exit (gui, 1, "Are your sure that you want to close this sequence?"))
    {
        if (gui == gui_last_focus)
            gui_last_focus = NULL;
        gui_disable_timeout (gui);
        sequence_destroy (gui->sequence);
        event_unsubscribe_all (gui);
        gui_prefs_cleanup (gui);
#ifdef USE_DEPRECIATED
        gtk_object_destroy (GTK_WIDGET (gui->tooltips));
#endif
        gui_builder_destroy (gui->builder);
        ARRAY_REMOVE (gui_t, gui_instances, gui_instances_num, gui);
        if (gui_instances_num == 0)
        {
            gui_do_exit (gui);
        }
        free (gui);
        return FALSE;
    }
    return TRUE;
}

#ifdef HAVE_GTK_QUARTZ

static void
gui_mac_prefs_run (gui_t *gui, guint action, GtkWidget *widget)
{
    if (gui_last_focus)
        gui = gui_last_focus;

    gui_prefs_run (gui, action, widget);
}
#endif

G_MODULE_EXPORT gboolean
gui_focus (GtkWidget * widget, GdkEventFocus * event, gui_t * gui) // Glade callback
{
    gui_last_focus = gui;
#ifdef HAVE_GTK_QUARTZ
    ige_mac_menu_set_menu_bar (GTK_MENU_SHELL (gui->menubar));
    ige_mac_menu_set_quit_menu_item (GTK_MENU_ITEM (gui->menubar_quit_item));
    ige_mac_menu_set_global_key_handler_enabled (TRUE);
#endif
    return FALSE;
}

G_MODULE_EXPORT gboolean
gui_focus_out (GtkWidget * widget, GdkEventFocus * event, gui_t * gui) // Glade callback
{
#ifdef HAVE_GTK_QUARTZ
    // Deactivate the Carbon menu shortcuts when a popup is opened, etc..
    ige_mac_menu_set_global_key_handler_enabled (FALSE);
#endif
    return FALSE;
}

G_MODULE_EXPORT gboolean
gui_entry_focus (GtkWidget * widget, GdkEventFocus * event, gui_t * gui) // Glade callback
{
#ifdef HAVE_GTK_QUARTZ
    // Prevent conflict between single-letter Carbon menu shortcuts and gtk entries
    ige_mac_menu_set_global_key_handler_enabled (FALSE);
#endif
    return FALSE;
}

G_MODULE_EXPORT gboolean
gui_entry_focus_out (GtkWidget * widget, GdkEventFocus * event, gui_t * gui) // Glade callback
{
#ifdef HAVE_GTK_QUARTZ
    // Restore Carbon menu shortcuts when leaving a gtk entry
    ige_mac_menu_set_global_key_handler_enabled (TRUE);
#endif
    return FALSE;
}

static void
gui_duplicate_sequence (GtkWidget * w, gpointer data)
{
    gui_t * gui = data;
    if (!gui->refreshing)
    {
#ifdef PRINT_EXTRA_DEBUG
        DEBUG ("Pattern resized");
#endif
        gint tracks_num = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (gui->tracks_num));
        gint beats_num = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (gui->beats_num));
        gint measure_len = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (gui->measure_len));

        gui_disable_timeout (gui);

        if (!sequence_resize (gui->sequence, tracks_num, beats_num * 2, measure_len, 1))
            gui_display_error (gui, error_to_string (sequence_get_error (gui->sequence)));

        gui_enable_timeout (gui);
    }
}

static gboolean
gui_sequence_do_resize (gpointer data)
{
    gui_t *gui = (gui_t *) data;
    gint tracks_num = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (gui->tracks_num));
    gint beats_num = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (gui->beats_num));
    gint measure_len = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (gui->measure_len));

    gui_disable_timeout (gui);

    if (!sequence_resize (gui->sequence, tracks_num, beats_num, measure_len, 0))
        gui_display_error (gui, error_to_string (sequence_get_error (gui->sequence)));

    gui_enable_timeout (gui);
    return FALSE;
}

void
gui_on_sequence_resized (event_t *event)
{
    gui_t *gui = (gui_t *) event->self;
    gui_refresh (gui);
    gui_set_modified (gui, 1);
}

G_MODULE_EXPORT void
gui_sequence_resized (GtkWidget * widget, gui_t * gui) // Glade callback
{
    if (!gui->refreshing)
    {
#ifdef PRINT_EXTRA_DEBUG
        DEBUG ("Pattern resized");
#endif
        g_idle_add (gui_sequence_do_resize, (gpointer) gui);
    }
}

G_MODULE_EXPORT void
gui_bpm_changed (GtkWidget * widget, gui_t * gui) // Glade callback
{
    sequence_set_bpm (gui->sequence, gtk_spin_button_get_value (GTK_SPIN_BUTTON (gui->bpm)));
}

static void
gui_on_bpm_changed (event_t *event)
{
    gui_t *gui = (gui_t *) event->self;
    gui_block_handler (gui, gui->bpm, gui_bpm_changed);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (gui->bpm),
                               sequence_get_bpm (gui->sequence));
    gui_unblock_handler (gui, gui->bpm, gui_bpm_changed);
    gui_set_modified (gui, 1);
}

static void
gui_show_disconnect_warning (gui_t *gui, int transient)
{
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("Showing disconnection warning");
#endif
    static GtkWidget *dialog = NULL;
    if (!dialog)
    {
        dialog = gui_builder_get_widget (gui->builder, "disconnect_warning");
        GtkWidget * disconnect_prefs = gui_builder_get_widget (gui->builder, "disconnect_prefs");
        GtkWidget * disconnect_ok = gui_builder_get_widget (gui->builder, "disconnect_ok");

        gtk_dialog_add_action_widget (GTK_DIALOG (dialog), disconnect_prefs, GTK_RESPONSE_OK);
        gtk_dialog_add_action_widget (GTK_DIALOG (dialog), disconnect_ok, GTK_RESPONSE_CANCEL);
        gtk_widget_set_sensitive (disconnect_prefs, gui_prefs_is_running (gui) ? FALSE : TRUE);
        gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
        gtk_widget_grab_focus (disconnect_ok);
        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gui->window));
        if (transient)
        {
            gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        }
        else
        {
            gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
        }
#ifdef PRINT_EXTRA_DEBUG
        DEBUG ("Run dialog");
#endif
        gtk_widget_show (dialog);
        int confirm = (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK);
        gtk_widget_hide (dialog);
        dialog = NULL;
        if (confirm)
            gui_prefs_run (0, gui);
    }
}

G_MODULE_EXPORT void
gui_play_clicked (GtkWidget * widget, gui_t * gui) // Glade callback 
{
    if (stream_is_connected (gui->stream))
        sequence_start (gui->sequence);
    else
        gui_show_disconnect_warning (gui, 1);
}

static void
gui_menu_play_clicked (GtkWidget * w, gpointer data)
{
    gui_t * gui = data;
    if (sequence_is_playing (gui->sequence))
        sequence_stop (gui->sequence);
    else
        sequence_start (gui->sequence);
}

void start_sequence()
{
    int i = 0;
    for (i = 0; i < gui_instances_num; i++)
    {
        if(!sequence_is_playing (gui_instances[i]->sequence))
        {
            if (stream_is_connected(gui_instances[i]->stream))
                sequence_start (gui_instances[i]->sequence);
        }
    }
}

G_MODULE_EXPORT void
gui_pause_clicked (GtkWidget * widget, gui_t * gui) // Glade callback
{
    sequence_stop (gui->sequence);
}

G_MODULE_EXPORT void
gui_rewind_clicked (GtkWidget * widget, gui_t * gui) // Glade callback
{
    sequence_rewind (gui->sequence);
}

static void
gui_menu_rewind_clicked (GtkWidget * w, gpointer data)
{
    gui_t * gui = data;
    gui_rewind_clicked (w, gui);
}

G_MODULE_EXPORT void
gui_loop_toggled (GtkWidget * widget, gui_t * gui) // Glade callback
{
    if (!gui->refreshing)
    {
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
            sequence_set_looping (gui->sequence);
        else
            sequence_unset_looping (gui->sequence);
    }
}

static void
gui_on_looping_changed (event_t *event)
{
    gui_t *gui = (gui_t *) event->self;
    gui_block_handler (gui, gui->loop, gui_loop_toggled);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gui->loop),
                                  sequence_is_looping (gui->sequence));
    gui_unblock_handler (gui, gui->loop, gui_loop_toggled);
    gui_set_modified (gui, 1);
}

static double
gui_compute_pitch (GtkWidget *octave, GtkWidget *semitone, GtkWidget *finetune)
{
#if GTK_CHECK_VERSION(3,0,0)
    double pitch = gtk_spin_button_get_value (GTK_SPIN_BUTTON (octave)) * 12
            + gtk_spin_button_get_value (GTK_SPIN_BUTTON (semitone))
            + gtk_spin_button_get_value (GTK_SPIN_BUTTON (finetune));
#else
    double pitch = phat_knob_get_value (PHAT_KNOB (octave)) * 12
            + phat_knob_get_value (PHAT_KNOB (semitone))
            + phat_knob_get_value (PHAT_KNOB (finetune)) / 100;
#endif
    return pitch;
}

G_MODULE_EXPORT void
gui_pitch_changed (GtkRange *range, gui_t *gui)
{
    if (!gui->refreshing)
    {
        GtkWidget *octave = gui_builder_get_widget (gui->builder, "pitch_octave");
        GtkWidget *semitone = gui_builder_get_widget (gui->builder, "pitch_semitone");
        GtkWidget *finetune = gui_builder_get_widget (gui->builder, "pitch_finetune");

        double pitch = gui_compute_pitch (octave, semitone, finetune);
        int track = gui_sequence_editor_get_active_track (gui->sequence_editor);
#ifdef PRINT_EXTRA_DEBUG
        DEBUG ("knobs computed pitch: %lf", pitch);
#endif
        sequence_set_pitch (gui->sequence, track, pitch);
    }
}

G_MODULE_EXPORT void
gui_pitch_spinner_changed (GtkSpinButton *spinner, gui_t *gui)
{
    if (!gui->refreshing)
    {
        int track = gui_sequence_editor_get_active_track (gui->sequence_editor);
        sequence_set_pitch (gui->sequence, track, gtk_spin_button_get_value (spinner));
    }
}

static void
gui_update_pitch_controls (gui_t *gui, int track)
{
    GtkWidget *pitch_spinner = gui_builder_get_widget (gui->builder, "pitch_spinner");
    GtkWidget *octave = gui_builder_get_widget (gui->builder, "pitch_octave");
    GtkWidget *semitone = gui_builder_get_widget (gui->builder, "pitch_semitone");
    GtkWidget *finetune = gui_builder_get_widget (gui->builder, "pitch_finetune");

    int old         = gui->refreshing;
    gui->refreshing = 1;

    double current_pitch = gui_compute_pitch (octave, semitone, finetune);
    double pitch = sequence_get_pitch (gui->sequence, track);
    gtk_spin_button_set_value  (GTK_SPIN_BUTTON (pitch_spinner), pitch);
    if (pitch != current_pitch)
    {
        double octave_val = util_round (pitch / 12);
        double semitone_val = util_round (pitch) - octave_val * 12;
        double finetune_val = (pitch - octave_val * 12 - semitone_val) * 100;

#if GTK_CHECK_VERSION(3,0,0)
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (octave), octave_val);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (semitone), semitone_val);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (finetune), finetune_val);
#else
        phat_knob_set_value (PHAT_KNOB (octave), octave_val);
        phat_knob_set_value (PHAT_KNOB (semitone), semitone_val);
        phat_knob_set_value (PHAT_KNOB (finetune), finetune_val);
#endif
    }

    gui->refreshing = old;
}

static void
gui_on_pitch_changed (event_t *event)
{
    gui_t *gui = (gui_t *) event->self;
    sequence_position_t *pos = (sequence_position_t *) event->data;
    int active_track = gui_sequence_editor_get_active_track (gui->sequence_editor);
    if (active_track == pos->track)
        gui_update_pitch_controls (gui, pos->track);

    gui_set_modified (gui, 1);
}

G_MODULE_EXPORT void
gui_track_name_entered (GtkWidget * widget, gui_t * gui) // Glade callback
{
    gui_block_handler (gui, widget, gui_track_name_focus_lost);
    int active_track = gui_sequence_editor_get_active_track (gui->sequence_editor);
    char *new_name = strdup (gtk_entry_get_text (GTK_ENTRY (widget)));
    int error;

    while (new_name && (error = sequence_set_track_name (gui->sequence, active_track, new_name)))
    {
        char *p = new_name;
        new_name = gui_ask_track_name (gui, new_name, error, 1);
        free (p);
    }

    if (new_name)
    {
        sequence_set_track_name (gui->sequence, active_track, new_name);
        free (new_name);
    }
    else
    {
        gtk_entry_set_text (GTK_ENTRY (gui_builder_get_widget (gui->builder, "track_name")),
                            sequence_get_track_name (gui->sequence, active_track));
    }
    gui_unblock_handler (gui, widget, gui_track_name_focus_lost);
}

gboolean
gui_track_name_focus_lost (GtkWidget * widget, GdkEventFocus *event, gui_t * gui) // Glade callback
{
    gui_track_name_entered (widget, gui);
    return FALSE;
}

void
gui_on_track_name_changed (event_t *event)
{
    gui_t *gui = (gui_t *) event->self;
    sequence_position_t *pos = (sequence_position_t *) event->data;
    int active_track = gui_sequence_editor_get_active_track (gui->sequence_editor);
    if (active_track == pos->track)
    {
        gtk_entry_set_text (GTK_ENTRY (gui_builder_get_widget (gui->builder, "track_name")),
                            sequence_get_track_name (gui->sequence, active_track));
    }
    gui_set_modified (gui, 1);
}

void
gui_on_track_activated (event_t *event)
{
    gui_t *gui = (gui_t *) event->self;
    gui_update_track_properties (gui);
}

static void
gui_update_track_properties (gui_t *gui)
{
    int active_track = gui_sequence_editor_get_active_track (gui->sequence_editor);
    gui_update_pitch_controls (gui, active_track);
    gtk_entry_set_text (GTK_ENTRY (gui_builder_get_widget (gui->builder, "track_name")),
                        sequence_get_track_name (gui->sequence, active_track));
    sample_t *sample = sequence_get_sample (gui->sequence, active_track);
    if (sample)
    {
        sample_display_set_data (SAMPLE_DISPLAY (gui->sample_display), (void *) sample->data,
                                 SAMPLE_DISPLAY_DATA_FLOAT, sample->channels_num,
                                 sample->frames, FALSE);
        char *sample_name = strdup (sample->filename);
        gtk_label_set_text (GTK_LABEL (gui_builder_get_widget (gui->builder, "current_sample")),
                            basename (sample_name));
        free (sample_name);
    }
    else
    {
        float f = 0;
        sample_display_set_data (SAMPLE_DISPLAY (gui->sample_display), (void *) &f,
                                 SAMPLE_DISPLAY_DATA_FLOAT, 1,
                                 1, TRUE);
        gtk_label_set_text (GTK_LABEL (gui_builder_get_widget (gui->builder, "current_sample")),
                            "<no sample>");
    }
    GtkWidget *button = gui_builder_get_widget (gui->builder, "recent_samples_button");
    gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (button),
                                   gui_file_build_recent_samples_menu (gui, active_track));
}

G_MODULE_EXPORT void
gui_properties_size_request (GtkWidget *widget, GtkRequisition *req, gui_t *gui) // Glade callback
{
    return; // FIXME: Unused for now...
    DEBUG ("height: %d", req->height);
    guint padding;
    gtk_box_query_child_packing (GTK_BOX (gtk_widget_get_parent(widget)), widget, NULL, NULL, &padding, NULL);
    int delta = req->height + padding * 2 - gui->window_grown_by;
    if (delta > 0)
    {
        gui->window_grown_by += delta;
        int window_width, window_height;
        gtk_window_get_size (GTK_WINDOW (gui->window), &window_width, &window_height);
        gtk_window_resize (GTK_WINDOW (gui->window), window_width, window_height + delta);
    }
}

G_MODULE_EXPORT void
gui_toggle_sequence_properties (GtkWidget *widget, gui_t *gui) // Glade callback
{
    gtk_widget_hide (gui_builder_get_widget (gui->builder, "track_properties"));
    GtkWidget *other = gui_builder_get_widget (gui->builder, "track_prop_toggle");
    gui_block_handler (gui, other, gui_toggle_track_properties);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (other), FALSE);
    gui_unblock_handler (gui, other, gui_toggle_track_properties);
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
        gtk_widget_show (gui_builder_get_widget (gui->builder, "sequence_properties"));
    else
        gtk_widget_hide (gui_builder_get_widget (gui->builder, "sequence_properties"));
}

G_MODULE_EXPORT void
gui_toggle_track_properties (GtkWidget *widget, gui_t *gui) // Glade callback
{
    gtk_widget_hide (gui_builder_get_widget (gui->builder, "sequence_properties"));
    GtkWidget *other = gui_builder_get_widget (gui->builder, "sequence_prop_toggle");
    gui_block_handler (gui, other, gui_toggle_sequence_properties);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (other), FALSE);
    gui_unblock_handler (gui, other, gui_toggle_sequence_properties);
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
        gtk_widget_show (gui_builder_get_widget (gui->builder, "track_properties"));
    else
        gtk_widget_hide (gui_builder_get_widget (gui->builder, "track_properties"));
}

G_MODULE_EXPORT void
gui_load_sample_dialog (GtkWidget * widget, gui_t * gui) // Glade callback
{
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("Using sample_wdir : %s", gui->rc->sample_wdir);
#endif
    int active_track = gui_sequence_editor_get_active_track (gui->sequence_editor);
    gui_file_load_sample (gui, active_track);
}

void
gui_on_load_sample_request (event_t *event)
{
    gui_load_sample_dialog (NULL, (gui_t *) event->self);
}

static void
gui_load_sample (GtkWidget * w, gpointer data)
{
    gui_t * gui = data;
    gui_load_sample_dialog (NULL, gui);
}

static void
gui_do_rename_track (gui_t *gui, int track)
{
    char *new_name;
    char *old_name = sequence_get_track_name (gui->sequence, track);
    new_name = gui_ask_track_name (gui, old_name, 0, 1);

    int error;
    while (new_name && (error = sequence_set_track_name (gui->sequence, track, new_name)))
    {
        char *p = new_name;
        new_name = gui_ask_track_name (gui, new_name, error, 1);
        free (p);
    }

    free (old_name);

    if (new_name)
    {
        sequence_set_track_name (gui->sequence, track, new_name);
        free (new_name);
    }
}

void
gui_on_track_rename_request (event_t *event)
{
    gui_t *gui = (gui_t *) event->self;
    sequence_position_t *pos = (sequence_position_t *) event->data;
    gui_do_rename_track (gui, pos->track);
}

static void
gui_rename_track (GtkWidget * w, gpointer data)
{
    gui_t * gui = data;
    int active_track = gui_sequence_editor_get_active_track (gui->sequence_editor);
    gui_do_rename_track (gui, active_track);
}

static void
gui_mute_track (GtkWidget * w, gpointer data)
{
    gui_t * gui = data;
    int active_track = gui_sequence_editor_get_active_track (gui->sequence_editor);
    sequence_mute_track (gui->sequence,
                         !sequence_track_is_muted (gui->sequence, active_track),
                         active_track);
}

static void
gui_solo_track (GtkWidget * w, gpointer data)
{
    gui_t * gui = data;
    int active_track = gui_sequence_editor_get_active_track (gui->sequence_editor);
    sequence_solo_track (gui->sequence,
                         !sequence_track_is_solo (gui->sequence, active_track),
                         active_track);
}

static void
gui_clear_solo (GtkWidget * w, gpointer data)
{
    gui_t * gui = data;
    int i, ii = sequence_get_tracks_num (gui->sequence);
    for (i = 0; i < ii; i++)
    {
        sequence_solo_track (gui->sequence, 0, i);
    }
}

static void
gui_shift_track_volume (GtkWidget * w)
{
    gui_t * gui = g_object_get_data(G_OBJECT(w), "gui");
    int action = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "volume_adjust"));
    
    gui_adjust_track_volume(gui, action);
}

static void
gui_adjust_track_volume (gui_t * gui, int action)
{
    int active_track = gui_sequence_editor_get_active_track (gui->sequence_editor);
    double add = 0;
    switch (action)
    {
        case GUI_SHIFT_DOWN:      add = -0.2;
            break;
        case GUI_SHIFT_UP:        add =  0.2;
            break;
        case GUI_SHIFT_DOWN_BIG:  add = -3;
            break;
        case GUI_SHIFT_UP_BIG:    add =  3;
            break;
        case GUI_SHIFT_RESET:
            sequence_set_volume_db (gui->sequence, active_track, 0);
            break;
    }
    if (add)
    {
        sequence_increase_volume_db (gui->sequence, active_track, add);
    }
}

static void
gui_add_track (GtkWidget * w, gpointer data)
{
    gui_t * gui = data;
    int ntracks = sequence_get_tracks_num (gui->sequence);
    int nbeats  = sequence_get_beats_num (gui->sequence);
    int measure = sequence_get_measure_len (gui->sequence);
    sequence_resize (gui->sequence, ntracks + 1, nbeats, measure, 0);
}

static void
gui_remove_track (GtkWidget * w, gpointer data)
{
    gui_t * gui = data;
    if (sequence_get_tracks_num (gui->sequence) > 1)
    {
        int active_track = gui_sequence_editor_get_active_track (gui->sequence_editor);
        sequence_remove_track (gui->sequence, active_track);
    }
}

void
gui_on_size_delta_request (event_t *event)
{
    gui_t *gui = (gui_t *) event->self;
    if (!gui->window_sized)
    {
#ifdef PRINT_EXTRA_DEBUG
        DEBUG ("window is visible: %d", gtk_widget_get_visible (gui->window));
#endif
        if (gui->is_initial)
        {
            gtk_window_resize (GTK_WINDOW (gui->window), 780, 550);
        }
        else
        {
            GdkDisplay *display = gdk_display_get_default ();
            GdkScreen *screen = gdk_display_get_default_screen (display);
            int screen_width = gdk_screen_get_width (screen);
            int screen_height = gdk_screen_get_height (screen);

            int window_width, window_height;
            gtk_window_get_size (GTK_WINDOW (gui->window), &window_width, &window_height);

            GtkAllocation *size_delta = (GtkAllocation *) event->data;
            int new_width = window_width + size_delta->width;
            int new_height = window_height + size_delta->height;

            if (new_height > screen_height / 2)
                new_height = screen_height / 2;

            if (sequence_get_beats_num (gui->sequence) > 16)
            {
                if (new_width > new_height * 7 / 2)
                    new_width = new_height * 7 / 2;
            }

            if (new_width > screen_width * 2 / 3)
                new_width = screen_width * 2 / 3;

            if ((new_width != window_width) || (new_height != window_height))
                gtk_window_resize (GTK_WINDOW (gui->window), new_width, new_height);
        }

        gui->window_sized = 1;
    }
}

G_MODULE_EXPORT gboolean // Glade callback
gui_hijack_key_press (GtkWindow *win, GdkEventKey *event, gui_t *gui)
{
    /* Let the focused widget handle key press events before the window, when 
       there is no modifier. This essentially allows to enter into GtkEntry 
       the letters that are menu accelerators. */
    int modifiers = GDK_CONTROL_MASK | GDK_MOD1_MASK;
    if (gtk_window_get_focus(win) && !(event->state & modifiers))
    {
        int handled = gtk_widget_event (gtk_window_get_focus(win), (GdkEvent *) event);
        if (!handled)
        {
            // The following is redundant with some menu accelerators, but needed
            // on OS X to allow repeating when the key is hold
            int shift  = event->state & GDK_SHIFT_MASK;
            int action = 0;
            switch (event->keyval)
            {
                case GDK_KEY_p:
                case GDK_KEY_P:
                    action = shift ? GUI_SHIFT_UP_BIG : GUI_SHIFT_UP;
                    break;
                case GDK_KEY_l:
                case GDK_KEY_L:
                    action = shift ? GUI_SHIFT_DOWN_BIG : GUI_SHIFT_DOWN;
                    break;
            }
            if (action)
            {
                gui_adjust_track_volume (gui, action);
                handled = 1;
            }
        }
        return handled;
    }
    return FALSE;
}

static void
gui_init (gui_t * gui)
{
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("Initializing GUI");
#endif

    gui->window = gui_builder_get_widget (gui->builder, "main_window");
    gui_update_window_title (gui);
    gui->main_vbox = gui_builder_get_widget (gui->builder, "main_vbox");
    
    gui->accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(gui->window), gui->accel_group);
    
    gui->menubar = gui_menubar(gui);
    
#ifdef USE_DEPRECIATED
    gui->menubar = gui_make_menubar (gui, gui_menu_items,
                                     gui_menu_nitems);
#endif

    gtk_box_pack_start (GTK_BOX (gui->main_vbox), gui->menubar, FALSE, TRUE, 0);

    gui->tracks_num = gui_builder_get_widget (gui->builder, "tracks_num");
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (gui->tracks_num), sequence_get_tracks_num (gui->sequence));

    gui->beats_num = gui_builder_get_widget (gui->builder, "beats_num");
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (gui->beats_num), sequence_get_beats_num (gui->sequence));

    gui->measure_len = gui_builder_get_widget (gui->builder, "measure_len");
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (gui->measure_len), sequence_get_measure_len (gui->sequence));

    gui->bpm = gui_builder_get_widget (gui->builder, "bpm");
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (gui->bpm), sequence_get_bpm (gui->sequence));

    gui->loop = gui_builder_get_widget (gui->builder, "loop");
    if (sequence_is_looping (gui->sequence))
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gui->loop), TRUE);

    gui->rewind = gui_builder_get_widget (gui->builder, "rewind");
    gtk_widget_set_sensitive (gui->rewind, gui->rc->transport_query ? TRUE : FALSE);
    /*
      g_signal_connect (G_OBJECT (gui->window), "key_press_event",
                        G_CALLBACK (gui_playback_toggle_pressed), (gpointer) gui);
     */
    gtk_box_reorder_child (GTK_BOX (gui->main_vbox), gui->menubar, 0);

    /* The FALSE flag is for edit mode - currently disabled - FIXME 
     * Edit mode allows for setting a partial or cropped sample to be played.
     * It will currently show the range - (left and right mouse buttons) but
     * the range limit play is currently not implemented. Also needs a user
     * button to toggle the edit mode. */
    gui->sample_display = sample_display_new (TRUE);
    
    GdkColor *c = SAMPLE_DISPLAY_CLASS (GTK_WIDGET_GET_CLASS (gui->sample_display))->colors + SAMPLE_DISPLAYCOL_MIXERPOS;
    c->red = 0xff << 8;
    c->green = 0xc6 << 8;
    c->blue = 0x00 << 8;
    c->pixel = (gulong) ((c->red & 0xff00)*256 + (c->green & 0xff00) + (c->blue & 0xff00) / 256);
#if !GTK_CHECK_VERSION(3,0,0)
    gdk_color_alloc (gdk_colormap_get_system (), c);
#endif

    /* The horizontal center line in the sample display. */
    sample_display_enable_zero_line (SAMPLE_DISPLAY (gui->sample_display), TRUE);
    
    /* Handles that show on the top and bottom of the progress line. Disabled. */
    sample_display_enable_marker_handles (SAMPLE_DISPLAY (gui->sample_display), FALSE);

    GtkWidget *sbox = gui_builder_get_widget (gui->builder, "sample_view_box");
    gtk_box_pack_start (GTK_BOX (sbox), gui->sample_display, TRUE, TRUE, 0);
    gtk_widget_show (gui->sample_display);

    gui->sequence_editor = gui_sequence_editor_new (gui->sequence);
    event_scope_add (gui, gui->sequence_editor);
    event_subscribe (gui->sequence_editor, "track-activated", gui, gui_on_track_activated);
    event_subscribe (gui->sequence_editor, "size-delta-request", gui, gui_on_size_delta_request);
    event_subscribe (gui->sequence_editor, "load-sample-request", gui, gui_on_load_sample_request);
    event_subscribe (gui->sequence_editor, "track-rename-request", gui, gui_on_track_rename_request);
    GtkWidget *sequence_box = gui_builder_get_widget (gui->builder, "sequence_box");
    gtk_box_pack_start (GTK_BOX (sequence_box), gui_sequence_editor_get_widget (gui->sequence_editor),
                        TRUE, TRUE, 0);
#if GTK_CHECK_VERSION(3,0,0)
    /* This is necessary since apparently glade 3 only allows to enter two digit precision */
    GtkWidget *finetune = gui_builder_get_widget (gui->builder, "pitch_finetune");
    gtk_spin_button_set_range (finetune, -0.5, 0.4999);
    gtk_spin_button_set_increments (finetune, 0.0001, .001);
#endif
    gui_update_track_properties (gui);

    gtk_widget_show_all (gui->main_vbox);

#ifdef HAVE_GTK_QUARTZ
    gtk_widget_hide (gui->menubar);
#endif
}

void
gui_new (rc_t *rc, arg_t *arg, song_t *song, osc_t *osc, stream_t *stream)
{
    event_subscribe (NULL, "desktop-open-action", NULL, gui_on_desktop_open_action);
    gui_new_child (rc, arg, NULL, song, NULL, NULL, osc, stream);
}

static char *
gui_get_sequence_name (gui_t *gui)
{
    char *name = malloc (8 + (gui->instance_index + 1) / 10 + 1 + 1);
    sprintf (name, "sequence%d", gui->instance_index + 1);
    return name;
}

char *
gui_get_next_sequence_name (gui_t *gui)
{
    char *name = malloc (8 + (gui_instance_counter + 1) / 10 + 1 + 1);
    sprintf (name, "sequence%d", gui_instance_counter + 1);
    return name;
}

static void
gui_stream_connection_changed (event_t *event)
{
    gui_t *gui = (gui_t *) event->self;
    gui_prefs_update_audio_status (gui);
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("connect changed");
#endif
}

static void
gui_stream_connection_lost (event_t *event)
{
    gui_t *gui = (gui_t *) event->self;
    gui_show_disconnect_warning (gui, 0);
}

void
gui_new_child (rc_t *rc, arg_t *arg, gui_t *parent, song_t *song,
               sequence_t *sequence, char *filename, osc_t *osc, stream_t *stream)
{
    gui_t *gui;
    gui = calloc (1, sizeof (gui_t));
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("Creating new GUI (PID: %d)", getpid ());
#endif
    gui->sequence_is_modified = 0;
    gui->rewind = NULL;
    gui->refreshing = 0;
    gui->song = song;
    gui->progress_window = NULL;
    gui->window = NULL;
    gui->transpose_volumes_round = 0;
    gui->rc = rc;
    gui->arg = arg;
    gui->last_export_framerate = 0;
    gui->last_export_sustain_type = 0;
    gui->last_export_wdir[0] = '\0';
    gui->osc = osc;
    gui->stream = stream;
    gui->window_sized = 0;
    gui->window_grown_by = 0;
    gui->sequence_editor = NULL;
    gui->prefs = NULL;
    gui->is_initial = 0;

    if ((parent == NULL))
    {
        DEBUG ("Parsing rc file: %s", util_pkgdata_path ("glade/jackbeat.gtk.rc"));
        gtk_rc_parse (util_pkgdata_path ("glade/jackbeat.gtk.rc"));
        //gtk_init (&(arg->argc), &(arg->argv));
        if (arg->filename != NULL)
        {
            if (!(sequence = gui_file_do_load_sequence (gui, arg->filename)))
                exit (1);
            filename = arg->filename;
        }
        else
        {
            gui->is_initial = 1;
        }
    }

#if GTK_CHECK_VERSION(3,0,0)
    // needed on gtk 2.16/mingw32 (go figure), to register PhatKnob with GtkBuilder:
//    GtkWidget *junk = phat_knob_new (NULL);
//    gtk_widget_destroy (junk);
    gui->builder = gui_builder_new (util_pkgdata_path ("glade/jackbeat_gtk3.ui"),
                                    (void *) gui,
                                    "preferences",
                                    "main_window",
                                    "export_settings",
                                    "close_warning",
                                    "disconnect_warning",
                                    NULL);
#else
    // needed on gtk 2.16/mingw32 (go figure), to register PhatKnob with GtkBuilder:
    GtkWidget *junk = phat_knob_new (NULL);
    gtk_widget_destroy (junk);

    gui->builder = gui_builder_new (util_pkgdata_path ("glade/jackbeat.ui"),
                                    (void *) gui,
                                    "preferences",
                                    "main_window",
                                    "export_settings",
                                    "close_warning",
                                    "disconnect_warning",
                                    NULL);
#endif
    gui->instance_index = gui_instance_counter++;

    if (sequence) gui->sequence = sequence;
    else
    {
        char *name = gui_get_sequence_name (gui);
        int error;
        gui->sequence = sequence_new (gui->stream, name, &error);
        free (name);

        if (gui->sequence == NULL)
        {
            char *s = error_to_string (error);
            gui_display_error (gui, s);
            free (s);
            free (gui);
            return;
        }

        if (!sequence_resize (gui->sequence, 4, 16, 4, 0))
        {
            DEBUG ("Can't set initial sequence size");
            char *s = error_to_string (sequence_get_error (gui->sequence));
            gui_display_error (gui, s);
            free (s);
            sequence_destroy (gui->sequence);
            free (gui);
            return;
        }

        song_register_sequence (gui->song, gui->sequence);

        sequence_set_transport (gui->sequence, rc->transport_aware, rc->transport_query);

        if (gui->sequence && (rc->default_resampler_type != -1))
            sequence_set_resampler_type (gui->sequence, rc->default_resampler_type);

    }

    event_enable_queue (gui);
    event_subscribe (gui->stream, "connection-changed", gui, gui_stream_connection_changed);
    event_subscribe (gui->stream, "connection-lost", gui, gui_stream_connection_lost);
    event_subscribe (gui->sequence, "transport-changed", gui, gui_on_transport_changed);
    event_subscribe (gui->sequence, "beat-changed", gui, gui_on_sequence_modified);
    event_subscribe (gui->sequence, "bpm-changed", gui, gui_on_bpm_changed);
    event_subscribe (gui->sequence, "looping-changed", gui, gui_on_looping_changed);
    event_subscribe (gui->sequence, "track-mute-changed", gui, gui_on_sequence_modified);
    event_subscribe (gui->sequence, "track-solo-changed", gui, gui_on_sequence_modified);
    event_subscribe (gui->sequence, "track-pitch-changed", gui, gui_on_pitch_changed);
    event_subscribe (gui->sequence, "track-volume-changed", gui, gui_on_sequence_modified);
    event_subscribe (gui->sequence, "resized", gui, gui_on_sequence_resized);
    event_subscribe (gui->sequence, "reordered", gui, gui_on_sequence_modified);
    event_subscribe (gui->sequence, "track-name-changed", gui, gui_on_track_name_changed);

    gui_prefs_init (gui);

    if (filename)
    {
        strcpy (gui->filename, filename);
        gui->filename_is_set = 1;
    }
    else
    {
        strcpy (gui->filename, "untitled.jab");
        gui->filename_is_set = 0;
    }

#ifdef USE_DEPRECIATED
    gui->tooltips = gtk_tooltips_new ();
#endif
    
    gui_init (gui);
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("Starting GUI");
#endif
    gui_last_focus = gui;
    gui_enable_timeout (gui);
    gtk_widget_show (gui->window);
    ARRAY_ADD (gui_t, gui_instances, gui_instances_num, gui);
    if (gui->is_initial)
    {
        GtkWidget *button = gui_builder_get_widget (gui->builder, "track_prop_toggle");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), 1);
    }
    while (gtk_events_pending ())
        gtk_main_iteration ();
    gui_set_modified (gui, 0);

    install_signal_handlers ();

    if (!parent)
    {
        gtk_main ();
    }
}



static void
gui_new_instance (GtkWidget *widget,  gpointer data )
{
    gui_t *gui = data;
    gui_new_child (gui->rc, gui->arg, gui, gui->song, NULL, NULL, gui->osc, gui->stream);
    gui_refresh (gui);
}

static void
gui_close_from_menu (GtkWidget * w, gpointer data)
{
    gui_t * gui = data;
    gui_close (NULL, NULL, gui);
}

static void
gui_save ()
{
    int i = 0;
    for (i = 0; i < gui_instances_num; i++)
    {
        if (gui_instances[i]->sequence_is_modified)
        {
            if (gui_instances[i]->filename_is_set)
                gui_file_do_save_sequence (gui_instances[i], gui_instances[i]->filename);
            else
                gui_file_save_as_sequence (0, gui_instances[i]);
        }
    }
}

static void
gui_exit (GtkWidget * w, gpointer data)
{
    gui_t * gui = data;
    // FIXME: not checking if there's any unsaved sequences
    if (gui_last_focus)
        gui = gui_last_focus;

    int i, c = 0;
    for (i = 0; i < gui_instances_num; i++)
    {
        if (gui_instances[i]->sequence_is_modified)
            c++;
    }

    int confirm = 1;
    if (c)
    {
        char str[128];
        if (c > 1)
            sprintf (str, "There are %d unsaved sequences.", c);
        else
            sprintf (str, "There is one unsaved sequence.");

        strcat (str, " Are you sure that you want to quit?");
        confirm = gui_confirm_exit (gui, 0, str);
    }

    if (confirm)
        gui_do_exit (gui);
}

char *
gui_ask_track_name (gui_t *gui, char *current_name, int error, int allow_cancel)
{
    GtkWidget * dialog;
    char *new_name = NULL;

#if GTK_CHECK_VERSION(3,0,0)
    GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
#else
    GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
    GtkWidget *vbox = gtk_vbox_new (FALSE, 0);
#endif
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 15);

    if (allow_cancel)
    {
        dialog = gtk_dialog_new_with_buttons (
                                              "Rename track",
                                              GTK_WINDOW (gui->window),
                                              GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                              GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                              NULL);
    }
    else
        dialog = gtk_dialog_new_with_buttons (
                                              "Rename track",
                                              GTK_WINDOW (gui->window),
                                              GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                              GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                              NULL);
    g_signal_connect (G_OBJECT (dialog), "delete_event",
                      G_CALLBACK (gui_no_delete), NULL);

    if (error)
    {
        char *str = error_to_string (error);
        GtkWidget *label = gtk_label_new (str);
        free (str);
        gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 15);
    }

    GtkWidget *entry = gtk_entry_new ();
    gtk_entry_set_max_length (GTK_ENTRY (entry), 127);
    gtk_entry_set_text (GTK_ENTRY (entry), current_name);
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    gtk_box_pack_start (GTK_BOX (vbox), entry, TRUE, TRUE, 15);
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox, TRUE, TRUE,
                        0);
    gtk_widget_show_all (dialog);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        char *buf = (char *) gtk_entry_get_text (GTK_ENTRY (entry));
        if (strlen (buf))
        {
            new_name = malloc (128);
            strcpy (new_name, buf);
            g_strstrip (new_name);
        }
    }
    gtk_widget_destroy (dialog);
    return new_name;
}

/*
 * The signal handler is modified from catch_signals.c
 * http://askra.de/software/gtk-signals/x2992.html
 * Declined GTK Tutorial Contribution
 * Unofficial Appendix. Catching Unix signals
 */

void
pipe_signals (int signal)
{
    if (write (signal_pipe[1], &signal, sizeof (int)) != sizeof (int))
    {
        fprintf (stderr, "unix signal %d lost\n", signal);
    }
}

void
install_signal_handlers ()
{
    /*
     * In order to register the reading end of the pipe with the event loop
     * we must convert it into a GIOChannel.
     */
    GIOChannel *g_signal_in;

    GError *error = NULL;	/* handle errors */
    long fd_flags; 	    /* used to change the pipe into non-blocking mode */

    /*
     * Set the unix signal handling up.
     * First create a pipe.
     */
    if (pipe (signal_pipe))
    {
        perror ("pipe");
        exit (1);
    }

    /*
     * put the write end of the pipe into nonblocking mode,
     * need to read the flags first, otherwise we would clear other flags too.
     */
    fd_flags = fcntl (signal_pipe[1], F_GETFL);
    if (fd_flags == -1)
    {
        perror ("read descriptor flags");
        exit (1);
    }
    if (fcntl (signal_pipe[1], F_SETFL, fd_flags | O_NONBLOCK) == -1)
    {
        perror ("write descriptor flags");
        exit (1);
    }

    /* Install the unix signal handler pipe_signals for the signals of interest */
    signal (SIGINT, pipe_signals);
    signal (SIGUSR1, pipe_signals);

    /* convert the reading end of the pipe into a GIOChannel */
    g_signal_in = g_io_channel_unix_new (signal_pipe[0]);

    /*
     * we only read raw binary data from the pipe,
     * therefore clear any encoding on the channel
     */
    g_io_channel_set_encoding (g_signal_in, NULL, &error);
    if (error != NULL) 		/* handle potential errors */
    {
        fprintf (stderr, "g_io_channel_set_encoding failed %s\n",
                 error->message);
        exit (1);
    }

    /* put the reading end also into non-blocking mode */
    g_io_channel_set_flags (g_signal_in,
                            g_io_channel_get_flags (g_signal_in) | G_IO_FLAG_NONBLOCK, &error);

    if (error != NULL) 		/* tread errors */
    {
        fprintf (stderr, "g_io_set_flags failed %s\n",
                 error->message);
        exit (1);
    }

    /* register the reading end with the event loop */
    g_io_add_watch (g_signal_in, G_IO_IN | G_IO_PRI, deliver_signal, NULL);

}

/*
 * The event loop callback that handles the unix signals. Must be a GIOFunc.
 * The source is the reading end of our pipe, cond is one of
 *   G_IO_IN or G_IO_PRI (I don't know what could lead to G_IO_PRI)
 * the pointer d is always NULL
 */
gboolean
deliver_signal (GIOChannel *source, GIOCondition cond, gpointer d)
{
    GError *error = NULL;		/* for error handling */

    /*
     * There is no g_io_channel_read or g_io_channel_read_int, so we read
     * char's and use a union to recover the unix signal number.
     */
    union
    {
        gchar chars[sizeof (int)];
        int signal;
    } buf;
    GIOStatus status;		/* save the reading status */
    gsize bytes_read;		/* save the number of chars read */


    /*
     * Read from the pipe as long as data is available. The reading end is
     * also in non-blocking mode, so if we have consumed all unix signals,
     * the read returns G_IO_STATUS_AGAIN.
     */
    while ((status = g_io_channel_read_chars (source, buf.chars,
                                              sizeof (int), &bytes_read, &error)) == G_IO_STATUS_NORMAL)
    {
        g_assert (error == NULL);	/* no error if reading returns normal */

        /*
         * There might be some problem resulting in too few char's read.
         * Check it.
         */
        if (bytes_read != sizeof (int))
        {
            fprintf (stderr, "lost data in signal pipe (expected %d, received %d)\n",
                     (int) sizeof (int), (int) bytes_read);
            continue;	      /* discard the garbage and keep fingers crossed */
        }

        /* Ok, we read a unix signal number, so save or exit based on type */
        if (buf.signal == SIGUSR1)
        {
            gui_save ();
        }

        if (buf.signal == SIGINT)
        {
            gui_exit (0, gui_last_focus);
        }
    }

    /*
     * Reading from the pipe has not returned with normal status. Check for
     * potential errors and return from the callback.
     */
    if (error != NULL)
    {
        fprintf (stderr, "reading signal pipe failed: %s\n", error->message);
        return (FALSE);
    }
    if (status == G_IO_STATUS_EOF)
    {
        fprintf (stderr, "signal pipe has been closed\n");
        return (FALSE);
    }

    g_assert (status == G_IO_STATUS_AGAIN);
    return (TRUE);		/* keep the event source */
}
