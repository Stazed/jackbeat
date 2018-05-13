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
 *   SVN:$Id: file.c 661 2009-12-16 12:35:51Z olivier $
 */

#ifdef __WIN32__
#include <windows.h>
#include <gdk/gdkwin32.h>
#endif
#include "gui/common.h"

static char * gui_file_dialog_open (const char *title, GtkWidget *parent, const char *dir,
                                    const char *filter_name, char *filter_exts[]);
static char * gui_file_dialog_save (const char *title, GtkWidget *parent, const char *filename, const char *ext,
                                    const char *filter_name, char *filter_exts[]);
static void
gui_file_add_extension (char **path, const char *ext) ;

extern gui_t *   gui_last_focus;

typedef struct gui_file_track_sample_t
{
    gui_t * gui;
    int     track;
    char *  sample_fname;
} gui_file_track_sample_t;

void
gui_file_do_load_sample (gui_t *gui, int track, char *filename)
{
    /* Trying to reuse memory if this sample is already loaded*/
    sample_t *sample = song_try_reuse_sample (gui->song, filename);

    if (sample == NULL)
    {
        /* New or modified sample : loading ... */
        gui_show_progress (gui, "Loading sample", "Hold on...");
        if ((sample = sample_new (filename, gui_progress_callback, (void *) gui)) != NULL)
        {
            song_register_sample (gui->song, sample);
        }
        gui_hide_progress (gui);
    }

    if (sample)
    {
        /* We got a sample */
        if (sequence_set_sample (gui->sequence, track, sample))
        {
            rc_add_sample (gui->rc, sample->filename);

            gui_set_modified (gui, 1);
            gui_refresh (gui);
        }
        else
        {
            gui_display_error (gui, error_to_string (sequence_get_error (gui->sequence)));
        }
    }
    else
    {
        gui_display_error (gui,
                           "Unable to load the requested sample file.");
    }
}

void
gui_file_load_sample (gui_t *gui, int track)
{
    char **exts = sample_list_known_extensions ();
    char *filename = gui_file_dialog_open ("Load Sample", gui->window,
                                           gui->rc->sample_wdir,
                                           "Samples", exts);
    if (filename)
    {
        gui_file_do_load_sample (gui, track, filename);

        char dir[256];
        sprintf (dir, "%s/", dirname (filename));
        DIR *dir_stream;
        if ((dir_stream = opendir (dirname (filename))))
        {
            closedir (dir_stream);
            DEBUG ("Setting sample_wdir to : %s", dir);
            strcpy (gui->rc->sample_wdir, dir);
        }
        else
        {
            DEBUG ("Not setting sample_wdir to : %s", dir);
        }
        free (filename);
    }
}

sequence_t *
gui_file_do_load_sequence (gui_t *gui, char *filename)
{
    jab_t *jab;
    char *y = strdup (filename);
    int error;
    sprintf (gui->rc->sequence_wdir, "%s/", dirname (y));
    free (y);
    gui_show_progress (gui, "Loading JAB file", "Hold on...");

    if (!(jab = jab_open (filename, JAB_READ, gui_progress_callback, (void *) gui, &error)))
    {
        gui_hide_progress (gui);
        char s[256];
        sprintf (s, "Unable to load \"%s\": %s", filename, error_to_string (error));
        gui_display_error (gui, s);
        return NULL;
    }
    sequence_t *sequence;
    char *name = gui_get_next_sequence_name (gui);
    if (jab && (sequence = jab_retrieve_sequence (jab, gui->stream, name, &error)))
    {
        rc_add_sequence (gui->rc, filename);
        song_register_sequence (gui->song, sequence);
        song_register_sequence_samples (gui->song, sequence);
        sequence_set_transport (sequence, gui->rc->transport_aware, gui->rc->transport_query);
        sequence_set_resampler_type (sequence, gui->rc->default_resampler_type);
        gui_hide_progress (gui);
        jab_close (jab);
        return sequence;
    }
    else
    {
        char s[256];
        sprintf (s, "Unable to load \"%s\": %s", filename, error_to_string (error));
        gui_display_error (gui, s);
        gui_hide_progress (gui);
        if (jab) jab_close (jab);
        return 0;
    }
    free (name);
}

void
gui_file_load_sequence (GtkWidget * w,  gpointer data)
{
    gui_t *gui = data;
    char *exts[] = {"jab", NULL};
    char *filename = gui_file_dialog_open ("Load Sequence", gui->window, gui->rc->sequence_wdir,
                                           "JAB files", exts);
    if (filename)
    {
        sequence_t *sequence;
        if ((sequence = gui_file_do_load_sequence (gui, filename)))
            gui_new_child (gui->rc, gui->arg, gui, gui->song, sequence, filename, gui->osc, gui->stream);
        free (filename);
    }
}

static int
gui_file_get_export_settings (gui_t *gui, int *framerate, int *sustain_type)
{
    int ret = 0;
    static int normalized = 0;
    GtkWidget *dialog, *rate_combo, *loop_button, *keep_button, *truncate_button;

    gui_builder_get_widgets (gui->builder,
                             "export_settings", &dialog,
                             "export_rate", &rate_combo,
                             "export_loop", &loop_button,
                             "export_truncate", &truncate_button,
                             "export_keep", &keep_button,
                             NULL);

    if (!normalized)
    {
        gui_misc_normalize_dialog_spacing (dialog);
        normalized = 1;
    }
    switch (gui->last_export_sustain_type)
    {
        case SEQUENCE_SUSTAIN_LOOP:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (loop_button), 1);
            break;
        case SEQUENCE_SUSTAIN_TRUNCATE:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (truncate_button), 1);
            break;
        case SEQUENCE_SUSTAIN_KEEP:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (keep_button), 1);
            break;
        default:
            if (sequence_is_looping (gui->sequence))
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (loop_button), 1);
            else
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (keep_button), 1);
    }

    char str[64];
    int cur_rate = sequence_get_framerate (gui->sequence);
    sprintf (str, "Same as audio device (%d)", cur_rate);
    gui_misc_dropdown_clear (rate_combo);
    gui_misc_dropdown_append (rate_combo, str);
    if (gui->last_export_framerate == 0)
        gtk_combo_box_set_active (GTK_COMBO_BOX (rate_combo), 0);

    int rates[] = {22050, 32000, 44100, 48000, 88200, 96000};
    int i;
    for (i = 0; i < 6; i++)
    {
        sprintf (str, "%d", rates[i]);
        gui_misc_dropdown_append (rate_combo, str);
        if (gui->last_export_framerate == rates[i])
            gtk_combo_box_set_active (GTK_COMBO_BOX (rate_combo), i + 1);
    }

    if (gtk_combo_box_get_active (GTK_COMBO_BOX (rate_combo)) == -1)
    {
        sprintf (str, "%d", gui->last_export_framerate);
        gui_misc_dropdown_append (rate_combo, str);
        gtk_combo_box_set_active (GTK_COMBO_BOX (rate_combo), i + 1);
    }
    while (1)
    {
        int response = gtk_dialog_run (GTK_DIALOG (dialog));
        if (response == GTK_RESPONSE_OK)
        {

            if (gtk_combo_box_get_active (GTK_COMBO_BOX (rate_combo)) == 0)
            {
                *framerate = cur_rate;
                gui->last_export_framerate = 0;
            }
            else
            {
                char *active_text = gui_misc_dropdown_get_active_text (rate_combo);
                if (((sscanf (active_text, "%d", framerate) != 1))
                    || (*framerate < 4000) || (*framerate > 192000))
                {
                    gui_display_error (gui, "Invalid frame rate (must be an integer between 4000 and 192000)");
                    continue;
                }
                gui->last_export_framerate = *framerate;
            }

            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (loop_button)))
                *sustain_type = SEQUENCE_SUSTAIN_LOOP;
            else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (truncate_button)))
                *sustain_type = SEQUENCE_SUSTAIN_TRUNCATE;
            else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (keep_button)))
                *sustain_type = SEQUENCE_SUSTAIN_KEEP;
            ret = 1;
            break;
        }
        else if (response)
        {
            break;
        }
    }
    gtk_widget_hide (dialog);
    return ret;
}

void
gui_file_export_sequence (GtkWidget * w, gpointer data)
{
    gui_t * gui = data;
    char filename[512];
    char *tmp;

    if (strlen (gui->last_export_wdir))
    {
        tmp = strdup (gui->filename);
        sprintf (filename, "%s/%s", gui->last_export_wdir, basename (tmp));
        free (tmp);
    }
    else
    {
        if (gui->filename_is_set)
        {
            strcpy (filename, gui->filename);
        }
        else
        {
            if (getcwd (filename, 512) == NULL)
            {
              DEBUG("ERROR: getcwd: %d", errno);
              return;
            }
            strcat (filename, "/");
            tmp = strdup (gui->filename);
            strcat (filename, basename (tmp));
            free (tmp);
        }
    }

    int len = strlen (filename);

    if ((len >= 4) && (strncasecmp (filename + len - 4, ".jab", 4) == 0))
        filename[len - 4] = '\0';

    strcat (filename, ".wav");

    DEBUG ("Proposed name: %s", filename);

    //gui->file_selection = gui_file_create_selector (gui, "Export sequence as", filename, 1);

    char *exts[] = {"wav", NULL};
    char *chosen_name = gui_file_dialog_save ("Export Sequence as",
                                              gui->window, filename, ".wav",
                                              "WAV files", exts);
    if (chosen_name)
    {

        int framerate, sustain_type = 0;

        if (gui_file_get_export_settings (gui, &framerate, &sustain_type))
        {
            DEBUG ("Chosen name: %s", chosen_name);
            DEBUG ("Parameters: framerate: %d, sustain: %d", framerate, sustain_type);
            gui->last_export_sustain_type = sustain_type;
            char *s = strdup (chosen_name);
            sprintf (gui->last_export_wdir, "%s/", dirname (s));
            free (s);

            gui_show_progress (gui, "Exporting sequence", "Hold on...");
            gui_disable_timeout (gui); // avoid deadlock
            sequence_export (gui->sequence, chosen_name, framerate, sustain_type, gui_progress_callback, (void *) gui);
            gui_enable_timeout (gui);
            gui_hide_progress (gui);
        }

        free (chosen_name);
    }
}

int
gui_file_do_save_sequence (gui_t * gui, char *filename)
{
    int success = 0;
    jab_t *jab;
    gui_show_progress (gui, "Saving sequence", "Hold on...");
    int error;
    if ((jab = jab_open (filename, JAB_WRITE, gui_progress_callback,
                         (void *) gui, &error)))
    {
        jab_add_sequence (jab, gui->sequence);
        if (jab_close (jab)) success = 1;
    }
    gui_hide_progress (gui);
    if (success)
        gui_set_modified (gui, 0);
    else
        gui_display_error (gui, "Unable to save the current sequence.");
    return success;
}

void
gui_file_save_as_sequence (GtkWidget * w, gpointer data)
{
    gui_t * gui = data;
    char filename[512];
    if (gui->filename_is_set)
        strcpy (filename, gui->filename);
    else
    {
        char *s = strdup (gui->filename);
        sprintf (filename, "%s/%s", gui->rc->sequence_wdir, basename (s));
        free (s);
    }

    char *exts[] = {"jab", NULL};
    char *chosen_name = gui_file_dialog_save ("Save Sequence as", gui->window,
                                              filename, ".jab", "JAB files", exts);
    if (chosen_name)
    {
        if (gui_file_do_save_sequence (gui, chosen_name))
        {
            strcpy (gui->filename, chosen_name);
            rc_add_sequence (gui->rc, chosen_name);
            gui->filename_is_set = 1;
            sprintf (gui->rc->sequence_wdir, "%s/", dirname (chosen_name));
            gui_set_modified (gui, 0);
        }
        free (chosen_name);
    }
}

void
gui_file_save_sequence (GtkWidget * w, gpointer data)
{
    gui_t * gui = data;
    if (gui->filename_is_set)
    {
        DEBUG ("Ok, we already have a filename. Let's try and save...");
        gui_file_do_save_sequence (gui, gui->filename);
    }
    else
        gui_file_save_as_sequence (0, gui);
}

static void
gui_file_load_sample_from_history (GtkWidget * menu_item, gui_file_track_sample_t * ts)
{
    gui_t *gui = ts->gui;
    char *filename = strdup (ts->sample_fname);
    gui_file_do_load_sample (gui, ts->track, filename);
    free (filename);
}

void
gui_file_track_sample_destroy (gpointer data, GClosure *closure)
{
    gui_file_track_sample_t *track_sample = (gui_file_track_sample_t *) data;
    free (track_sample);
}

void
guimenutest  (GtkContainer *container, GtkWidget    *widget, gpointer      user_data)
{
    gui_t *gui = (gui_t *) user_data;
    DEBUG ("menu add");
}

GtkWidget *
gui_file_build_recent_samples_menu (gui_t *gui, int track)
{
    GtkWidget *menu = gtk_menu_new ();
    g_signal_connect (G_OBJECT (menu), "add",
                      G_CALLBACK (guimenutest), (gpointer) gui);
    int i;
    gui_file_track_sample_t *ts;

    if (gui->rc->sample_history_num == 0)
    {
        gui_misc_add_menu_item (menu, "#ghost", "<No sample>", NULL, NULL, NULL);
    }
    else
    {
        for (i = 0; i < gui->rc->sample_history_num; i++)
        {
            ts = malloc (sizeof (gui_file_track_sample_t));
            ts->gui = gui;
            ts->track = track;
            ts->sample_fname = gui->rc->sample_history[i];
            char *hist = strdup (gui->rc->sample_history[i]);
            char *dirname1 = strdup (dirname (hist));
            free (hist);
            char *_dirname1 = strdup (dirname1);
            char *dirname2 = strdup (dirname (_dirname1));
            free (_dirname1);
            _dirname1 = strdup (dirname1);
            char *dir_basename = strdup (basename (_dirname1));
            free (_dirname1);
            hist = strdup (gui->rc->sample_history[i]);
            char *basename1 = strdup (basename (hist));
            free (hist);
            hist = strdup (gui->rc->sample_history[i]);

            char display[256];

            if ((strcmp (dirname1, "/") == 0) || (strcmp (dirname2, "/") == 0)) strcpy (display, hist);
            else sprintf (display, "/.../%s/%s", dir_basename, basename1);

            free (dirname1);
            free (dirname2);
            free (dir_basename);
            free (basename1);
            free (hist);

            gui_misc_add_menu_item (menu, "#item", display, G_CALLBACK (gui_file_load_sample_from_history),
                                    (gpointer) ts, gui_file_track_sample_destroy);
        }
    }

    return menu;
}

#ifdef HAVE_GTK_QUARTZ    

void
gui_file_dialog_osx_common (NavDialogCreationOptions *options, const char *title)
{
    NavDialogCreationOptions opt = {
        .version = kNavDialogCreationOptionsVersion,
        .optionFlags = kNavDefaultNavDlogOptions - kNavAllowMultipleFiles,
        .location.h = -1,
        .location.v = -1,
        .clientName = CFSTR ("Jackbeat"),
        .windowTitle = CFStringCreateWithCString (NULL, title, kCFStringEncodingUTF8), //FIXME: free
        .actionButtonLabel = NULL,
        .cancelButtonLabel = NULL,
        .message = NULL,
        .preferenceKey = 0, //FIXME: should be different for sequence, sample, etc ..
        .modality = kWindowModalityAppModal,
        .parentWindow = NULL
    };

    *options = opt;
}
#endif

#ifdef __WIN32__

char *
gui_file_build_filter (const char *name, char *extensions[])
{
    static char filter[512];
    if (extensions)
    {
        char fullname[255];
        char extlist[255] = "";
        int first = 1;
        sprintf (fullname, "%s (", name);
        while (*extensions)
        {
            if (!first)
            {
                strcat (fullname, ", ");
                strcat (extlist, ";");
            }

            char pattern[16];
            sprintf (pattern, "*.%s", *extensions);
            strcat (fullname, *extensions);
            strcat (extlist, pattern);
            extensions++;
            first = 0;
        }
        strcat (fullname, ")");
        sprintf (filter, "%s!%s!", fullname, extlist);
    }
    else
    {
        sprintf (filter, "%s!*.*!", name);
    }
    return filter;
}
#else

GtkFileFilter *
gui_file_build_filter (const char *name, char *extensions[])
{
    GtkFileFilter *filter = gtk_file_filter_new ();
    if (extensions)
    {
        char fullname[512];
        int first = 1;
        sprintf (fullname, "%s (", name);
        while (*extensions)
        {
            char pattern[16];
            sprintf (pattern, "*.%s", *extensions);
            gtk_file_filter_add_pattern (filter, pattern);
            if (!first)
                strcat (fullname, ", ");
            strcat (fullname, *extensions);
            extensions++;
            first = 0;
        }
        strcat (fullname, ")");
        gtk_file_filter_set_name (filter, fullname);
    }
    else
    {
        gtk_file_filter_add_pattern (filter, "*");
        gtk_file_filter_set_name (filter, name);
    }
    return filter;
}
#endif

static char *
gui_file_dialog_open (const char *title, GtkWidget *parent, const char *dir,
                      const char *filter_name, char *filter_exts[])
{
    char *path = NULL, *_path;
#ifdef HAVE_GTK_QUARTZ    
    NavDialogCreationOptions options;
    gui_file_dialog_osx_common (&options, title);
    NavDialogRef dialog;
    NavCreateGetFileDialog (&options, NULL, NULL, NULL, NULL, NULL, &dialog);
    NavDialogRun (dialog);
    NavReplyRecord reply;
    NavDialogGetReply (dialog, &reply);
    if (reply.validRecord)
    {
        long n;
        AECountItems (&reply.selection, &n);
        if (n == 1)
        {
            FSRef ref;
            AEGetNthPtr (&reply.selection, 1, typeFSRef, 0, 0, &ref, sizeof (ref), 0);
            path = malloc (512);
            FSRefMakePath (&ref, (UInt8 *) path, 512);
        }

    }
    NavDisposeReply (&reply);
    NavDialogDispose (dialog);
    gtk_window_present (GTK_WINDOW (parent));
#elif __WIN32__
    OPENFILENAME ofn;
    char szFileName[MAX_PATH] = "";
    ZeroMemory (&ofn, sizeof (ofn));
    ofn.lStructSize = sizeof (ofn);
    ofn.lpstrInitialDir = strdup (dir);
    util_path (ofn.lpstrInitialDir);
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST |  OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    char filter[512] = "";
    if (filter_name)
    {
        strcat (filter, gui_file_build_filter (filter_name, filter_exts));
    }
    strcat (filter, gui_file_build_filter ("All files", NULL));
    util_str_creplace (filter, '!', '\0');
    ofn.lpstrFilter = filter;

    if (GetOpenFileName (&ofn))
    {
        path = strdup (szFileName);
    }
    free (ofn.lpstrInitialDir);
#else
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new (title, GTK_WINDOW (parent),
                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                          NULL
                                          );
    if (filter_name)
    {
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), gui_file_build_filter (filter_name, filter_exts));
    }
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), gui_file_build_filter ("All files", NULL));
    if (dir)
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), dir);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        _path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        path = strdup (_path);
        g_free (_path);
    }
    gtk_widget_destroy (dialog);
#endif
    return path;
}

static char *
gui_file_dialog_save (const char *title, GtkWidget *parent, const char *filename, const char *ext,
                      const char *filter_name, char *filter_exts[])
{
    char *path = NULL, *_path, *tmp_filename;
#ifdef HAVE_GTK_QUARTZ    
    NavDialogCreationOptions options;
    gui_file_dialog_osx_common (&options, title);
    options.saveFileName = CFStringCreateWithCString (NULL, basename (filename), kCFStringEncodingUTF8); //FIXME: free
    NavDialogRef dialog;
    NavCreatePutFileDialog (&options, 0, 0, NULL, NULL, &dialog);
    NavDialogRun (dialog);
    NavReplyRecord reply;
    NavDialogGetReply (dialog, &reply);
    if (reply.validRecord)
    {
        long n;
        AECountItems (&reply.selection, &n);
        if (n == 1)
        {
            char basename[128];
            FSRef ref;
            AEGetNthPtr (&reply.selection, 1, typeFSRef, 0, 0, &ref, sizeof (ref), 0);
            path = malloc (512);
            FSRefMakePath (&ref, (UInt8 *) path, 512);
            CFStringGetCString (reply.saveFileName, basename, 128, kCFStringEncodingUTF8);
            strcat (path, "/");
            strcat (path, basename); // dangerous
        }

    }
    NavDisposeReply (&reply);
    NavDialogDispose (dialog);
    gtk_window_present (GTK_WINDOW (parent));
#elif __WIN32__
    OPENFILENAME ofn;
    char szFileName[MAX_PATH] = "";
    ZeroMemory (&ofn, sizeof (ofn));
    ofn.lStructSize = sizeof (ofn);
    ofn.lpstrFileTitle = util_basename (filename);
    ofn.lpstrInitialDir = util_dirname (filename);
    if (ext)
        ofn.lpstrDefExt = ext + 1;
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    char filter[512] = "";
    if (filter_name)
    {
        strcat (filter, gui_file_build_filter (filter_name, filter_exts));
    }
    strcat (filter, gui_file_build_filter ("All files", NULL));
    util_str_creplace (filter, '!', '\0');
    ofn.lpstrFilter = filter;
    if (GetSaveFileName (&ofn))
    {
        path = strdup (szFileName);
    }
    free (ofn.lpstrFileTitle);
    free (ofn.lpstrInitialDir);
#else
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new (title, GTK_WINDOW (parent),
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                          NULL
                                          );
    if (filename)
    {
        tmp_filename = strdup (filename);
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), dirname (tmp_filename));
        free (tmp_filename);
        tmp_filename = strdup (filename);
        gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog),
                                           basename (tmp_filename));
        free (tmp_filename);
    }
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
    if (filter_name)
    {
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), gui_file_build_filter (filter_name, filter_exts));
    }
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), gui_file_build_filter ("All files", NULL));
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        _path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        path = strdup (_path);
        g_free (_path);
    }
    gtk_widget_destroy (dialog);
    gui_file_add_extension (&path, ext);
#endif    
    return path;
}

static void
gui_file_add_extension (char **path, const char *ext)
{
    if (*path && ext)
    {
        int len = strlen (*path);
        int extlen = strlen (ext);
        if ((len < extlen) || (strncasecmp (*path + len - extlen, ext, extlen) != 0))
        {
            *path = realloc (*path, len + extlen + 1);
            strcat (*path, ext);
        }
    }
}



