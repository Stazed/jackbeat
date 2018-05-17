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

#include "gui/prefs.h"
#include "gui/misc.h"
#include "stream/stream.h"
#include "stream/device.h"

const int sample_rates[] = {22050, 44100, 48000, 88200, 96000, 192000, 0};
static int prefs_running = 0;

typedef enum prefs_section_t
{
    PREFS_AUDIO_STREAM  = 1,
    PREFS_AUDIO_RUNTIME = 2,
    PREFS_OSC           = 4
} prefs_section_t;

struct gui_prefs_t
{
    GtkWidget * dialog;
    GtkWidget * notebook;
    GtkWidget * apply_button;

    // Audio
    int         audio_tab;
    GtkWidget * audio_output;
    GtkWidget * audio_sample_rate;
    GtkWidget * audio_client_name;
    GtkWidget * audio_jack_settings;
    GtkWidget * audio_auto_start;
    GtkWidget * audio_auto_connect;
    GtkWidget * audio_transport_query;
    GtkWidget * audio_connect;
    GtkWidget * audio_disconnect;
    GtkWidget * audio_status_output;
    GtkWidget * audio_status_sample_rate;
    GtkWidget * audio_resample_linear;
    GtkWidget * audio_resample_sinc;

    // OSC
    int         osc_tab;
    GtkWidget * osc_server_port;
    GtkWidget * osc_dest_host;
    GtkWidget * osc_dest_port;
    GtkWidget * osc_dest_prefix;
    GtkWidget * osc_input_toggle;
    GtkWidget * osc_input_prefix;
    GtkWidget * osc_methods_tree;

    prefs_section_t changes;
} ;

G_MODULE_EXPORT void
gui_osc_build_methods_tree (GtkWidget *widget, gui_t *gui) // Glade callback
{
    int i, j;
    GtkTreeIter methods, events, item;

    GtkWidget *tree = gui_builder_get_widget (gui->builder, "osc_methods_tree");
    GtkTreeStore *store = (GtkTreeStore *) gtk_tree_view_get_model (GTK_TREE_VIEW (tree));

    GtkWidget *input_prefix_w = gui_builder_get_widget (gui->builder, "osc_input_prefix");
    const char *input_prefix = gtk_entry_get_text (GTK_ENTRY (input_prefix_w));
    GtkWidget *dest_prefix_w = gui_builder_get_widget (gui->builder, "osc_dest_prefix");
    const char *dest_prefix = gtk_entry_get_text (GTK_ENTRY (dest_prefix_w));

    osc_method_desc_t **descs = osc_reflect_sequence_methods (gui->osc, gui->sequence);

    if (store == NULL)
    {
        store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
        gtk_tree_view_set_model (GTK_TREE_VIEW (tree), GTK_TREE_MODEL (store));

        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;

        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("Path/Type", renderer, "text",
                                                           0, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("Description", renderer, "text",
                                                           1, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);


        gtk_tree_store_append (store, &methods, NULL);
        gtk_tree_store_set (store, &methods, 0, "OSC methods (receiving)", -1);
        gtk_tree_store_append (store, &events, NULL);
        gtk_tree_store_set (store, &events, 0, "OSC events (sending)", -1);

        GtkTreeIter item, argument;
        for (i = 0; descs[i] != NULL; i++)
        {
            if (descs[i]->type == OSC_IN)
                gtk_tree_store_append (store, &item, &methods);
            else
                gtk_tree_store_append (store, &item, &events);

            gtk_tree_store_set (store, &item, 1, descs[i]->comment, -1);

            for (j = 0; j < strlen (descs[i]->typespec); j++)
            {
                gtk_tree_store_append (store, &argument, &item);
                char argtype[16];
                switch (descs[i]->typespec[j])
                {
                    case 'i':
                        strcpy (argtype, "integer");
                        break;
                    case 'f':
                        strcpy (argtype, "float");
                        break;
                    default:
                        sprintf (argtype, "%c", descs[i]->typespec[j]);
                }
                gtk_tree_store_set (store, &argument, 0, argtype, 1, descs[i]->parameters[j], -1);
            }
        }
    }
    else
    {
        gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &methods, NULL, 0);
        gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &events, NULL, 1);
    }

    int method_i = 0, event_i = 0;
    for (i = 0; descs[i] != NULL; i++)
    {
        char *path;
        if (descs[i]->type == OSC_IN)
        {
            gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &item,
                                           &methods, method_i++);
            path = malloc (strlen (input_prefix) + strlen (descs[i]->name) + 2);
            sprintf (path, "%s/%s", input_prefix, descs[i]->name);
        }
        else
        {
            gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &item,
                                           &events, event_i++);
            path = malloc (strlen (dest_prefix) + strlen (descs[i]->name) + 2);
            sprintf (path, "%s/%s", dest_prefix, descs[i]->name);
        }

        gtk_tree_store_set (store, &item, 0, path, -1);
        free (path);
    }

    osc_reflect_free (descs);
}

G_MODULE_EXPORT void
gui_osc_input_prefix_toggled (GtkWidget *widget, gui_t *gui) // Glade callback
{
    GtkWidget *input = gui_builder_get_widget (gui->builder, "osc_input_prefix");
    gtk_widget_set_sensitive (input, !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
    gui_osc_build_methods_tree (NULL, gui);
}

int
gui_prefs_get_named_tab_index (gui_t *gui, char *name)
{
    int index = -1;
    GtkWidget *child = gui_builder_get_widget (gui->builder, name);
    int i, ii = gtk_notebook_get_n_pages (GTK_NOTEBOOK (gtk_widget_get_parent(child)));
    for (i = 0; i < ii; i++)
    {
        if (child == gtk_notebook_get_nth_page (GTK_NOTEBOOK (gtk_widget_get_parent(child)), i))
        {
            index = i;
            break;
        }
    }

    DEBUG ("%s at %d", name, index);
    return index;
}

void
gui_prefs_init (gui_t *gui)
{
    gui_prefs_t *prefs = gui->prefs = malloc (sizeof (gui_prefs_t));
    prefs->audio_tab = gui_prefs_get_named_tab_index (gui, "audio_tab");
    prefs->osc_tab = gui_prefs_get_named_tab_index (gui, "osc_tab");
    gui_builder_get_widgets (gui->builder,
                             "preferences",               &prefs->dialog,
                             "prefs_apply_button",        &prefs->apply_button,
                             "prefs_notebook",            &prefs->notebook,

                             "audio_output",              &prefs->audio_output,
                             "audio_sample_rate",         &prefs->audio_sample_rate,
                             "audio_client_name",         &prefs->audio_client_name,
                             "audio_jack_settings",       &prefs->audio_jack_settings,
                             "audio_auto_start",          &prefs->audio_auto_start,
                             "audio_auto_connect",        &prefs->audio_auto_connect,
                             "audio_transport_query",     &prefs->audio_transport_query,
                             "audio_connect",             &prefs->audio_connect,
                             "audio_disconnect",          &prefs->audio_disconnect,
                             "audio_status_output",       &prefs->audio_status_output,
                             "audio_status_sample_rate",  &prefs->audio_status_sample_rate,
                             "audio_resample_linear",     &prefs->audio_resample_linear,
                             "audio_resample_sinc",       &prefs->audio_resample_sinc,

                             "osc_server_port",           &prefs->osc_server_port,
                             "osc_dest_host",             &prefs->osc_dest_host,
                             "osc_dest_port",             &prefs->osc_dest_port,
                             "osc_dest_prefix",           &prefs->osc_dest_prefix,
                             "osc_input_toggle",          &prefs->osc_input_toggle,
                             "osc_input_prefix",          &prefs->osc_input_prefix,
                             "osc_methods_tree",          &prefs->osc_methods_tree,
                             NULL);

    gui_misc_normalize_dialog_spacing (prefs->dialog);
    gui_prefs_update_audio_status (gui);

    const int *rate = sample_rates;
    char ratestr[16];
    while (*rate)
    {
        sprintf (ratestr, "%dHz", *rate++);
        gui_misc_dropdown_append (prefs->audio_sample_rate, ratestr);
    }
}

void
gui_prefs_cleanup (gui_t *gui)
{
    free (gui->prefs);
}

static void
gui_prefs_osc_display (gui_t *gui)
{
    gui_prefs_t *prefs = gui->prefs;
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (prefs->osc_server_port), osc_get_port (gui->osc));

    char *dest_host, *dest_prefix;
    int dest_port;
    osc_get_sequence_target (gui->osc, gui->sequence, &dest_host, &dest_port, &dest_prefix);

    gtk_entry_set_text (GTK_ENTRY (prefs->osc_dest_host), dest_host);
    free (dest_host);
    gtk_entry_set_text (GTK_ENTRY (prefs->osc_dest_prefix), dest_prefix);
    free (dest_prefix);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (prefs->osc_dest_port), (gdouble) dest_port);

    char *input_prefix;
    int is_default = osc_get_sequence_input_prefix (gui->osc, gui->sequence, &input_prefix);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefs->osc_input_toggle), is_default);
    gtk_widget_set_sensitive (prefs->osc_input_prefix, !is_default);
    gtk_entry_set_text (GTK_ENTRY (prefs->osc_input_prefix), input_prefix);
    free (input_prefix);

    gui_osc_build_methods_tree (NULL, gui);

}

static int
gui_prefs_osc_save (gui_t *gui)
{
    int success = 1;
    gui_prefs_t *prefs = gui->prefs;
    int port = gtk_spin_button_get_value (GTK_SPIN_BUTTON (prefs->osc_server_port));
    gui_wait_cursor (gtk_widget_get_window(prefs->dialog), 1);
    port = osc_set_port (gui->osc, port);
    gui_wait_cursor (gtk_widget_get_window(prefs->dialog), 0);
    if (!port)
    {
        gtk_notebook_set_current_page (GTK_NOTEBOOK (prefs->notebook), prefs->osc_tab);
        gui_display_error_from_window (gui,
                                       "Listening port is invalid or already in use", prefs->dialog);
        success = 0;
    }
    else
    {
        port = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (prefs->osc_dest_port));
        osc_set_sequence_target (gui->osc, gui->sequence,
                                 gtk_entry_get_text (GTK_ENTRY (prefs->osc_dest_host)),
                                 port,
                                 gtk_entry_get_text (GTK_ENTRY (prefs->osc_dest_prefix)));
        const char *prefix =
                gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prefs->osc_input_toggle))
                ? NULL : gtk_entry_get_text (GTK_ENTRY (prefs->osc_input_prefix));

        osc_set_sequence_input_prefix (gui->osc, gui->sequence, prefix);
    }
    return success;
}

static void
gui_prefs_audio_do_update_devices (gui_t *gui, char *current_device)
{
    gui_prefs_t *prefs = gui->prefs;
    gui_misc_dropdown_clear (prefs->audio_output);

    char **devices = stream_device_list ();
    int i;
    int active = 0;
    if (devices)
    {
        for (i = 0; devices[i] != NULL; i++)
        {
            gui_misc_dropdown_append (prefs->audio_output, devices[i]);
            if (current_device && !strcmp (devices[i], current_device))
            {
                active = 1;
                gtk_combo_box_set_active (GTK_COMBO_BOX (prefs->audio_output), i);
            }
            free (devices[i]);
        }
        free (devices);
        if (current_device && !active)
        {
            gui_misc_dropdown_prepend (prefs->audio_output, current_device);
            gtk_combo_box_set_active (GTK_COMBO_BOX (prefs->audio_output), 0);
        }
    }
}

G_MODULE_EXPORT void
gui_prefs_audio_update_devices (GtkWidget *widget, gui_t *gui) // Glade callback
{
    gui_prefs_audio_do_update_devices (gui, gui_misc_dropdown_get_active_text (gui->prefs->audio_output));
}

static void
gui_prefs_audio_display (gui_t *gui)
{
    gui_prefs_t *prefs = gui->prefs;
    gui_prefs_audio_do_update_devices (gui, gui->rc->audio_output);

    gtk_entry_set_text (GTK_ENTRY (prefs->audio_client_name), gui->rc->client_name);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefs->audio_auto_start), gui->rc->jack_auto_start);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefs->audio_auto_connect), gui->rc->auto_connect);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefs->audio_transport_query), gui->rc->transport_query);

    int i = 0;
    for (i = 0; sample_rates[i] && sample_rates[i] != gui->rc->audio_sample_rate; i++)
        ;
    if (sample_rates[i])
        gtk_combo_box_set_active (GTK_COMBO_BOX (prefs->audio_sample_rate), i);

    if (gui->rc->default_resampler_type == SEQUENCE_SINC)
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefs->audio_resample_sinc), TRUE);
    }
    else
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefs->audio_resample_linear), TRUE);
    }
}

static int
gui_prefs_audio_do_connect (gui_t *gui, int save)
{
    int success = 1;
    gui_prefs_t *prefs = gui->prefs;

    const char *client_name = gtk_entry_get_text (GTK_ENTRY (prefs->audio_client_name));
    char *device_name = gui_misc_dropdown_get_active_text (prefs->audio_output);
    int auto_start = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prefs->audio_auto_start));

    int rate_index = gtk_combo_box_get_active (GTK_COMBO_BOX (prefs->audio_sample_rate));
    int rate = sample_rates[rate_index];

    gui_wait_cursor (gtk_widget_get_window(prefs->dialog), 1);
    success = stream_device_open (gui->stream, device_name, rate, client_name, auto_start);
    gui_wait_cursor (gtk_widget_get_window(prefs->dialog), 0);
    if (success)
    {
        if (save)
        {
            strcpy (gui->rc->audio_output, device_name);
            strcpy (gui->rc->client_name, client_name);
            gui->rc->jack_auto_start = auto_start;
            gui->rc->audio_sample_rate = rate;
        }
    }
    else
    {
        char e[256];
        sprintf (e, "Failed to open audio output: %s", device_name);
        gtk_notebook_set_current_page (GTK_NOTEBOOK (prefs->notebook), prefs->audio_tab);
        gui_display_error_from_window (gui, e, prefs->dialog);
    }

    return success;
}

static int
gui_prefs_audio_stream_save (gui_t *gui)
{
    return gui_prefs_audio_do_connect (gui, 1);
}

static int
gui_prefs_audio_runtime_save (gui_t *gui)
{
    int auto_connect = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gui->prefs->audio_auto_connect)) ? 1 : 0;
    stream_auto_connect (gui->stream, auto_connect);

    int transport_query = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gui->prefs->audio_transport_query)) ? 1 : 0;
    int resampler_type  = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gui->prefs->audio_resample_sinc))
            ? SEQUENCE_SINC : SEQUENCE_LINEAR;
    int i, ii = song_count_sequences (gui->song);
    sequence_t **sequences = song_list_sequences (gui->song);
    for (i = 0; i < ii; i++)
    {
        sequence_set_transport (sequences[i], gui->rc->transport_aware, transport_query);
        sequence_set_resampler_type (sequences[i], resampler_type);
    }

    gui->rc->auto_connect           = auto_connect;
    gui->rc->transport_query        = transport_query;
    gui->rc->default_resampler_type = resampler_type;

    return 1;
}

G_MODULE_EXPORT void
gui_prefs_audio_runtime_changed (GtkWidget *widget, gui_t *gui) // Glade callback
{
    gui->prefs->changes  |= PREFS_AUDIO_RUNTIME;
    gtk_widget_set_sensitive (gui->prefs->apply_button, TRUE);
}

G_MODULE_EXPORT void
gui_prefs_audio_stream_changed (GtkWidget *widget, gui_t *gui) // Glade callback
{
    if (gui->prefs)
    {
        gui->prefs->changes |= PREFS_AUDIO_STREAM;
        gtk_widget_set_sensitive (gui->prefs->apply_button, TRUE);
        char *output = gui_misc_dropdown_get_active_text (gui->prefs->audio_output);
        if (output)
        {
            int is_jack = !strcmp (output, STREAM_DEVICE_JACK);
            int has_sample_rate = !is_jack && strcmp (output, STREAM_DEVICE_AUTO) && strcmp (output, STREAM_DEVICE_PULSE);
            gtk_widget_set_sensitive (gui->prefs->audio_jack_settings, is_jack);
            gtk_widget_set_sensitive (gui->prefs->audio_sample_rate, has_sample_rate);
        }
    }
}

G_MODULE_EXPORT void
gui_prefs_audio_device_changed (GtkWidget *widget, gui_t *gui) // Glade callback
{
    if (gui->prefs)
    {
        char *output = gui_misc_dropdown_get_active_text (gui->prefs->audio_output);
        if (output && strcmp (gui->rc->audio_output, output))
        {
            gui_prefs_audio_stream_changed (NULL, gui);
        }
    }
}

G_MODULE_EXPORT void
gui_prefs_osc_changed (GtkWidget *widget, gui_t *gui) // Glade callback
{
    gui->prefs->changes |= PREFS_OSC;
    gtk_widget_set_sensitive (gui->prefs->apply_button, TRUE);
}

void
gui_prefs_run (GtkWidget * w, gpointer data)
{
    gui_t *gui = data;
    prefs_running = 1;
    gui_prefs_t *prefs = gui->prefs;

    gui_prefs_osc_display (gui);
    gui_prefs_audio_display (gui);

    gui->prefs->changes = 0;
    while (1)
    {
        gtk_widget_set_sensitive (prefs->apply_button, gui->prefs->changes ? TRUE : FALSE);
        gint response = gtk_dialog_run (GTK_DIALOG (prefs->dialog));
        DEBUG ("response: %d", response);
        if ((response == GTK_RESPONSE_OK) || (response == GTK_RESPONSE_APPLY))
        {
            int success = 1;
            if (success && gui->prefs->changes & PREFS_OSC)
                if ((success = gui_prefs_osc_save (gui)))
                    gui->prefs->changes -= PREFS_OSC;

            if (success && gui->prefs->changes & PREFS_AUDIO_STREAM)
                if ((success = gui_prefs_audio_stream_save (gui)))
                    gui->prefs->changes -= PREFS_AUDIO_STREAM;

            if (success && gui->prefs->changes & PREFS_AUDIO_RUNTIME)
                if ((success = gui_prefs_audio_runtime_save (gui)))
                    gui->prefs->changes -= PREFS_AUDIO_RUNTIME;

            if (gui->prefs->changes == 0 && response == GTK_RESPONSE_OK)
                break;
        }
        else if (response)
        {
            break;
        }
    }
    gtk_widget_hide (prefs->dialog);
    prefs_running = 0;
}

int
gui_prefs_is_running (gui_t *gui)
{
    return prefs_running;
}

G_MODULE_EXPORT void
gui_prefs_audio_connect (GtkWidget *widget, gui_t *gui) // Glade callback
{
    gui_prefs_audio_do_connect (gui, 0);
}

G_MODULE_EXPORT void
gui_prefs_audio_disconnect (GtkWidget *widget, gui_t *gui) // Galde callback
{
    stream_disconnect (gui->stream);
}

void
gui_prefs_update_audio_status (gui_t *gui)
{
    gui_prefs_t *prefs = gui->prefs;

    char *output = stream_device_get_name (gui->stream);

    char ratestr[16];
    if (!strcmp (output, STREAM_DEVICE_NONE))
    {
        strcpy (ratestr, "N/A");
        gtk_label_set_markup (GTK_LABEL (prefs->audio_status_output),
                              "<span foreground=\"#CC0000\">Disconnected</span>");
        gtk_widget_set_sensitive (prefs->audio_connect, TRUE);
        gtk_widget_set_sensitive (prefs->audio_disconnect, FALSE);
    }
    else
    {
        gtk_label_set_markup (GTK_LABEL (prefs->audio_status_output), output);
        int sample_rate = stream_get_sample_rate (gui->stream);
        sprintf (ratestr, "%dHz", sample_rate);
        gtk_widget_set_sensitive (prefs->audio_connect, FALSE);
        gtk_widget_set_sensitive (prefs->audio_disconnect, TRUE);
    }

    gtk_label_set_text (GTK_LABEL (prefs->audio_status_sample_rate), ratestr);

    gui_prefs_audio_do_update_devices (gui, gui_misc_dropdown_get_active_text (prefs->audio_output));

    free (output);
}
