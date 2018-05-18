/*
 *   Jackbeat - JACK sequencer
 *    
 *   Copyright (c) 2004-2008 Olivier Guilyardi <olivier {at} samalyse {dot} com>
 *   Copyright (c) 2004 Steve Harris, Uwe Koloska
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
 *   SVN:$Id: jab.c 136 2008-04-07 19:25:50Z olivier $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>
#include <lo/lo.h>
#include <config.h>
#include "core/event.h"
#include "core/array.h"
#include "osc.h"
#include "sequence.h"

#define DEBUG(M, ...) { printf("OSC  %s(): ", __func__); printf(M, ## __VA_ARGS__); printf("\n"); }

// Can't write a function because of the variadic args of lo_send():
#define osc_send_from_sequence(osc, sequence, path, type, ...) \
{ \
  osc_sequence_t *osc_sequence \
    = (osc_sequence_t *) g_hash_table_lookup (osc->sequences, (gpointer) sequence); \
  if (osc_sequence->output_address) \
  { \
    char *_path = malloc (strlen (osc_sequence->output_prefix) + strlen (path) + 1); \
    sprintf (_path, "%s%s", osc_sequence->output_prefix, path); \
    lo_send (osc_sequence->output_address, _path, type, __VA_ARGS__); \
    free (_path); \
  } \
}

typedef struct osc_method_def_t
{
    osc_method_type_t     type;
    char *                name;
    char *                typespec;
    void *                function;
    char *                parameters[5];
    char *                comment;
} osc_method_def_t;

typedef struct osc_method_t
{
    osc_t       *osc;
    void        *instance;
    lo_method_handler wrapper;
    char        *prefix;
    osc_method_def_t *def;
} osc_method_t;

typedef struct osc_sequence_t
{
    char *        input_prefix;
    int           input_default_prefix;
    lo_address    output_address;
    char *        output_prefix;
} osc_sequence_t;

struct osc_t
{
    lo_server_thread  server;
    osc_method_t      **methods;
    int               methods_num;
    GHashTable *      sequences;
} ;

typedef struct osc_data_t
{
    lo_arg **     argv;
    int           argc;
    osc_method_t *method;
} osc_data_t;

typedef void(* osc_sequence_method_handler_t) (osc_t *osc, sequence_t *sequence, osc_data_t *data);

void  osc_error (int num, const char *m, const char *path);
int   osc_generic_handler (const char *path, const char *types, lo_arg **argv,
                           int argc, void *data, void *user_data);
int   osc_del_method (osc_t *osc, osc_method_t *method);

void  osc_on_sequence_destroy (event_t *event);
void  osc_on_song_sequence_registered (event_t *event);

void  osc_sequence_set_beat (osc_t *osc, sequence_t *sequence, osc_data_t *data);
void  osc_sequence_mute_beat (osc_t *osc, sequence_t *sequence, osc_data_t *data);
void  osc_sequence_start (osc_t *osc, sequence_t *sequence, osc_data_t *data);
void  osc_sequence_stop (osc_t *osc, sequence_t *sequence, osc_data_t *data);
void  osc_sequence_rewind (osc_t *osc, sequence_t *sequence, osc_data_t *data);
void  osc_sequence_set_transport (osc_t *osc, sequence_t *sequence, osc_data_t *data);
void  osc_sequence_set_bpm (osc_t *osc, sequence_t *sequence, osc_data_t *data);
void  osc_sequence_mute_track (osc_t *osc, sequence_t *sequence, osc_data_t *data);
void  osc_sequence_solo_track (osc_t *osc, sequence_t *sequence, osc_data_t *data);
void  osc_sequence_loop (osc_t *osc, sequence_t *sequence, osc_data_t *data);
void  osc_sequence_set_track_pitch (osc_t *osc, sequence_t *sequence, osc_data_t *data);
void  osc_sequence_set_track_volume (osc_t *osc, sequence_t *sequence, osc_data_t *data);
void  osc_sequence_set_track_volume_db (osc_t *osc, sequence_t *sequence, osc_data_t *data);
void  osc_on_sequence_beat_changed (event_t *event);
void  osc_on_sequence_beat_on (event_t *event);
void
osc_on_sequence_beat_off (event_t *event);

static osc_method_def_t osc_sequence_interface[] = {
    // Methods (receiving)
    { OSC_IN, "start", "",          osc_sequence_start,
        {}, "Start sequence playback" },
    { OSC_IN, "stop", "",           osc_sequence_stop,
        {}, "Stop sequence playback" },
    { OSC_IN, "rewind", "",         osc_sequence_rewind,
        {}, "Rewind sequence" },
    { OSC_IN, "set_transport", "ii", osc_sequence_set_transport,
        {"respond", "query"},
        "Configure JACK transport handling" },
    { OSC_IN, "set_bpm", "f",       osc_sequence_set_bpm,
        {"bpm"}, "Change BPM" },
    { OSC_IN, "loop", "i",          osc_sequence_loop,
        {"state"}, "Enable/disable looping" },
    { OSC_IN, "set_track_pitch", "if",   osc_sequence_set_track_pitch,
        {"track", "pitch"},
        "Change track pitch" },
    { OSC_IN, "set_track_volume", "if",   osc_sequence_set_track_volume,
        {"track", "volume"},
        "Change track volume (coefficient)" },
    { OSC_IN, "set_track_volume_db", "if",   osc_sequence_set_track_volume_db,
        {"track", "volume"},
        "Change track volume (decibels)" },
    { OSC_IN, "mute_track", "ii",   osc_sequence_mute_track,
        {"track", "state"},
        "Mute/unmute a track" },
    { OSC_IN, "solo_track", "ii",   osc_sequence_solo_track,
        {"track", "state"},
        "Set track solo state" },
    { OSC_IN, "set_beat", "iii",    osc_sequence_set_beat,
        {"track", "beat", "state"},
        "Enable/disable a beat" },
    { OSC_IN, "mute_beat", "iii",   osc_sequence_mute_beat,
        {"track", "beat", "state"},
        "Mute/unmute a beat" },

    // Events (sending)
    { OSC_OUT, "beat_changed", "iii", NULL,
        {"track", "beat", "state"},
        "A beat was toggled" },
    { OSC_OUT, "beat_on", "ii", NULL,
        {"track", "beat"},
        "A beat is starting to play" },
    { OSC_OUT, "beat_off", "ii", NULL,
        {"track", "beat"},
        "A beat has finished playing" },
};

static void
osc_sequence_value_destroy (gpointer data)
{
    osc_sequence_t *osc_sequence = (osc_sequence_t *) data;
    free (osc_sequence->input_prefix);
    free (osc_sequence->output_prefix);
    if (osc_sequence->output_address)
        lo_address_free (osc_sequence->output_address);
    free (osc_sequence);
}

lo_server_thread
osc_create_server (int port)
{
    lo_server_thread server;
    char portstr[16];
    sprintf (portstr, "%d", port);
    if ((server = lo_server_thread_new (port ? portstr : NULL, osc_error)))
    {
        lo_server_thread_add_method (server, NULL, NULL, osc_generic_handler, NULL);
        lo_server_thread_start (server);
#ifdef PRINT_EXTRA_DEBUG
        DEBUG ("New server on port %d", lo_server_thread_get_port (server));
#endif
    }
    else
    {
        DEBUG ("Couldn't start server on port %d", port);
    }
    return server;
}

osc_t *
osc_new (song_t *song)
{
    lo_server_thread server = osc_create_server (10203);
    if (!server)
        server = osc_create_server (0);
    if (!server)
        return NULL;

    osc_t * osc = malloc (sizeof (osc_t));
    osc->methods = NULL;
    osc->methods_num = 0;
    osc->server = server;

    event_subscribe (song, "sequence-registered", osc, osc_on_song_sequence_registered);
    osc->sequences = g_hash_table_new_full (NULL, NULL, NULL, osc_sequence_value_destroy);

    return osc;
}

void
osc_destroy (osc_t *osc)
{
    int i;
    for (i = 0; i < osc->methods_num; i++)
        while ((i < osc->methods_num) && osc_del_method (osc, osc->methods[i]))
            ;
    lo_server_thread_free (osc->server);
    g_hash_table_unref (osc->sequences);
    free (osc);
}

void
osc_error (int num, const char *msg, const char *path)
{
    DEBUG ("liblo server error %d in path %s: %s\n", num, path, msg);
}

int
osc_generic_handler (const char *path, const char *types, lo_arg **argv,
                     int argc, void *data, void *user_data)
{
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("Received message with path: %s", path);
#endif
    return 1;
}

int
osc_sequence_method_handler (const char *path, const char *types, lo_arg **argv,
                             int argc, void *data, void *user_data)
{
    osc_data_t osc_data;
    osc_method_t *method = (osc_method_t *) user_data;
    osc_data.argv = argv;
    osc_data.argc = argc;
    osc_data.method = method;
    osc_sequence_method_handler_t handler = (osc_sequence_method_handler_t) method->def->function;
    sequence_t *sequence = (sequence_t *) method->instance;
    handler (method->osc, sequence, &osc_data);
    return 0;
}

static char *
osc_get_method_path (osc_method_t *method)
{
    char *path = malloc (strlen (method->prefix) + strlen (method->def->name) + 4);
    sprintf (path, "%s/%s", method->prefix, method->def->name);
    return path;
}

osc_method_t *
osc_create_method (osc_t *osc, char *prefix, osc_method_def_t *def,
                   void *instance, lo_method_handler wrapper)
{
    osc_method_t *method = malloc (sizeof (osc_method_t));
    method->instance = instance;
    method->def = def;
    method->wrapper = wrapper;
    method->prefix = strdup (prefix);
    method->osc = osc;

    osc->methods = realloc (osc->methods, ++osc->methods_num * sizeof (osc_method_t *));
    osc->methods[osc->methods_num - 1] = method;

    char *path = osc_get_method_path (method);
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("path: %s (%d method(s))", path, osc->methods_num);
#endif
    free (path);
    return method;
}

osc_method_t *
osc_add_sequence_method (osc_t *osc, sequence_t *sequence, osc_method_def_t *def)
{
    char *prefix;
    osc_get_sequence_input_prefix (osc, sequence, &prefix);
    osc_method_t *method = osc_create_method (osc, prefix, def, sequence,
                                              osc_sequence_method_handler);
    free (prefix);
    char *path = osc_get_method_path (method);
    lo_server_thread_add_method (osc->server, path, method->def->typespec,
                                 method->wrapper, method);
    free (path);
    return method;
}

int
osc_del_method (osc_t *osc, osc_method_t *method)
{
    int i;
    int found = 0;
    for (i = 0; i < osc->methods_num; i++)
    {
        if (osc->methods[i] == method)
        {
            char *path = osc_get_method_path (method);
            lo_server_thread_del_method (osc->server, path, method->def->typespec);
            
            memmove (osc->methods + i, osc->methods + i + 1,
                    sizeof (osc_method_t *) * (osc->methods_num - i - 1));
            
            if (--osc->methods_num == 0)
            {
                free (osc->methods);
                osc->methods = NULL;
            }
            else
            {
                osc->methods = realloc (osc->methods, osc->methods_num * sizeof (osc_method_t *));
            }
#ifdef PRINT_EXTRA_DEBUG
            DEBUG ("path: %s (%d methods)", path, osc->methods_num);
#endif
            free (path);
            free (method->prefix);
            free (method);
            found = 1;
            break;
        }
    }
    /*
    for (i = 0; i < osc->methods_num; i++) {
      DEBUG("method %d: %s", i, osc->methods[i]->path);
    }
     */
    return found;
}

static void
osc_print_method_def (osc_method_def_t *def, char *prefix)
{
    printf ("%s%s ", prefix, def->name);
    int i, ii = strlen (def->typespec);
    for (i = 0; i < ii; i++)
        printf ("<%c:%s> ", def->typespec[i], def->parameters[i]);
    printf ("\t%s\n", def->comment);
}

void
osc_print_interface ()
{
    int i, ii = sizeof (osc_sequence_interface) / sizeof (osc_sequence_interface[0]);
    printf ("Methods (receiving):\n");
    for (i = 0; i < ii; i++)
        if (osc_sequence_interface[i].type == OSC_IN)
            osc_print_method_def (osc_sequence_interface + i, "/<sequence>/");

    printf ("Events (sending):\n");
    for (i = 0; i < ii; i++)
        if (osc_sequence_interface[i].type == OSC_OUT)
            osc_print_method_def (osc_sequence_interface + i, "/<sequence>/");
}

osc_method_desc_t **
osc_reflect_sequence_methods (osc_t *osc, sequence_t *sequence)
{
    int i, desc_num = 0;
    osc_method_desc_t **descs = NULL;
    for (i = 0; i < osc->methods_num; i++)
        if (osc->methods[i]->instance == sequence)
        {
            osc_method_desc_t *item = malloc (sizeof (osc_method_desc_t));
            ARRAY_ADD (osc_method_desc_t, descs, desc_num, item);
            item->type = osc->methods[i]->def->type; // always OSC_IN
            item->path = osc_get_method_path (osc->methods[i]);
            item->name = osc->methods[i]->def->name;
            item->typespec = osc->methods[i]->def->typespec;
            item->parameters = osc->methods[i]->def->parameters;
            item->comment = osc->methods[i]->def->comment;
        }

    int ii = sizeof (osc_sequence_interface) / sizeof (osc_sequence_interface[0]);
    for (i = 0; i < ii; i++)
    {
        osc_method_def_t *def = osc_sequence_interface + i;
        if (def->type == OSC_OUT)
        {
            osc_method_desc_t *item = malloc (sizeof (osc_method_desc_t));
            ARRAY_ADD (osc_method_desc_t, descs, desc_num, item);

            item->type = def->type; // always OSC_OUT

            // FIXME: duplicated code:
            osc_sequence_t *osc_sequence
                    = (osc_sequence_t *) g_hash_table_lookup (osc->sequences, (gpointer) sequence);
            item->path = malloc (strlen (osc_sequence->output_prefix) + strlen (def->name) + 1);
            sprintf (item->path, "%s%s", osc_sequence->output_prefix, def->name);

            item->name = def->name;

            item->typespec = def->typespec;
            item->parameters = def->parameters;
            item->comment = def->comment;
        }
    }

    ARRAY_ADD (osc_method_desc_t, descs, desc_num, NULL);

    return descs;
}

void
osc_reflect_free (osc_method_desc_t **descs)
{
    int i;
    for (i = 0; descs[i] != NULL; i++)
    {
        free (descs[i]->path);
        free (descs[i]);
    }
    free (descs);
}

int
osc_set_port (osc_t *osc, int port)
{
    if (port != lo_server_thread_get_port (osc->server))
    {
        lo_server_thread server;
        if ((server = osc_create_server (port)))
        {
            int i;
            for (i = 0; i < osc->methods_num; i++)
            {
                osc_method_t *method = osc->methods[i];
                char *path = osc_get_method_path (method);
                lo_server_thread_add_method (server, path, method->def->typespec,
                                             method->wrapper, method);
                free (path);
            }
            lo_server_thread_free (osc->server);
            osc->server = server;
        }
        else
        {
            port = 0;
        }
    }
    return port;
}

int
osc_get_port (osc_t *osc)
{
    return lo_server_thread_get_port (osc->server);
}

// Method handlers

void
osc_sequence_start (osc_t *osc, sequence_t *sequence, osc_data_t *data)
{
    sequence_start (sequence);
}

void
osc_sequence_stop (osc_t *osc, sequence_t *sequence, osc_data_t *data)
{
    sequence_stop (sequence);
}

void
osc_sequence_rewind (osc_t *osc, sequence_t *sequence, osc_data_t *data)
{
    sequence_rewind (sequence);
}

void
osc_sequence_set_transport (osc_t *osc, sequence_t *sequence, osc_data_t *data)
{
    sequence_set_transport (sequence, (char) data->argv[0]->i, (char) data->argv[1]->i);
}

void
osc_sequence_set_bpm (osc_t *osc, sequence_t *sequence, osc_data_t *data)
{
    sequence_set_bpm (sequence, data->argv[0]->f);
}

void
osc_sequence_loop (osc_t *osc, sequence_t *sequence, osc_data_t *data)
{
    if (data->argv[0]->i)
        sequence_set_looping (sequence);
    else
        sequence_unset_looping (sequence);
}

void
osc_sequence_set_beat (osc_t *osc, sequence_t *sequence, osc_data_t *data)
{
    sequence_set_beat (sequence, data->argv[0]->i, data->argv[1]->i, (char) data->argv[2]->i);
}

void
osc_sequence_mute_beat (osc_t *osc, sequence_t *sequence, osc_data_t *data)
{
    sequence_set_mask_beat (sequence, data->argv[0]->i, data->argv[1]->i, (char) !data->argv[2]->i);
}

void
osc_sequence_mute_track (osc_t *osc, sequence_t *sequence, osc_data_t *data)
{
    sequence_mute_track (sequence, data->argv[1]->i, data->argv[0]->i);
}

void
osc_sequence_solo_track (osc_t *osc, sequence_t *sequence, osc_data_t *data)
{
    sequence_solo_track (sequence, data->argv[1]->i, data->argv[0]->i);
}

void
osc_sequence_set_track_pitch (osc_t *osc, sequence_t *sequence, osc_data_t *data)
{
    sequence_set_pitch (sequence, data->argv[0]->i, data->argv[1]->f);
}

void
osc_sequence_set_track_volume (osc_t *osc, sequence_t *sequence, osc_data_t *data)
{
    sequence_set_volume (sequence, data->argv[0]->i, data->argv[1]->f);
}

void
osc_sequence_set_track_volume_db (osc_t *osc, sequence_t *sequence, osc_data_t *data)
{
    sequence_set_volume_db (sequence, data->argv[0]->i, data->argv[1]->f);
}

// Event handlers

void
osc_on_song_sequence_registered (event_t *event)
{
    osc_t *osc = (osc_t *) event->self;
    sequence_t *sequence = (sequence_t *) event->data;
    osc_sequence_t *osc_sequence = malloc (sizeof (osc_sequence_t));
    osc_sequence->input_prefix = strdup ("");
    osc_sequence->output_prefix = strdup ("");
    osc_sequence->output_address = NULL;
    g_hash_table_replace (osc->sequences, (gpointer) sequence, (gpointer) osc_sequence);
    osc_set_sequence_input_prefix (osc, sequence, NULL);

    osc_method_def_t *def;
    int i, ii = sizeof (osc_sequence_interface) / sizeof (osc_sequence_interface[0]);
    for (i = 0; i < ii; i++)
    {
        def = osc_sequence_interface + i;
        if (def->type == OSC_IN)
            osc_add_sequence_method (osc, sequence, def);
    }

    event_subscribe (sequence, "destroy", osc, osc_on_sequence_destroy);
    event_subscribe (sequence, "beat-changed", osc, osc_on_sequence_beat_changed);
    event_subscribe (sequence, "beat-on", osc, osc_on_sequence_beat_on);
    event_subscribe (sequence, "beat-off", osc, osc_on_sequence_beat_off);
}

void
osc_on_sequence_destroy (event_t *event)
{
    osc_t *osc = (osc_t *) event->self;
    sequence_t *sequence = (sequence_t *) event->source;

    char *name = sequence_get_name (sequence);
#ifdef PRINT_EXTRA_DEBUG
    DEBUG ("Detaching sequence %s", name);
#endif
    free (name);
    int i;
    for (i = 0; i < osc->methods_num; i++)
    {
        while ((i < osc->methods_num)
               && (osc->methods[i]->instance == sequence)
               && osc_del_method (osc, osc->methods[i]))
            ;
    }
    g_hash_table_remove (osc->sequences, (gpointer) sequence);
}

void
osc_on_sequence_beat_changed (event_t *event)
{
    osc_t *osc = (osc_t *) event->self;
    sequence_t *sequence = (sequence_t *) event->source;
    sequence_position_t *pos = (sequence_position_t *) event->data;
    int state = (int) sequence_get_beat (sequence, pos->track, pos->beat);
    osc_send_from_sequence (osc, sequence, "/beat_changed", "iii", pos->track,
                            pos->beat, state);
}

void
osc_on_sequence_beat_on (event_t *event)
{
    osc_t *osc = (osc_t *) event->self;
    sequence_t *sequence = (sequence_t *) event->source;
    sequence_position_t *pos = (sequence_position_t *) event->data;
    osc_send_from_sequence (osc, sequence, "/beat_on", "ii", pos->track,
                            pos->beat);
}

void
osc_on_sequence_beat_off (event_t *event)
{
    osc_t *osc = (osc_t *) event->self;
    sequence_t *sequence = (sequence_t *) event->source;
    sequence_position_t *pos = (sequence_position_t *) event->data;
    osc_send_from_sequence (osc, sequence, "/beat_off", "ii", pos->track,
                            pos->beat);
}

void
osc_set_sequence_target (osc_t *osc, sequence_t *sequence, const char *host,
                         int port, const char *prefix)
{
    // Thread safe ?
    osc_sequence_t *osc_sequence
            = (osc_sequence_t *) g_hash_table_lookup (osc->sequences, (gpointer) sequence);
    free (osc_sequence->output_prefix);
    osc_sequence->output_prefix = strdup (prefix);
    char portstr[8];
    sprintf (portstr, "%d", port);
    if (osc_sequence->output_address)
        lo_address_free (osc_sequence->output_address);
    osc_sequence->output_address = lo_address_new (host, portstr);
}

void
osc_get_sequence_target (osc_t *osc, sequence_t *sequence, char **host,
                         int *port, char **prefix)
{
    osc_sequence_t *osc_sequence
            = (osc_sequence_t *) g_hash_table_lookup (osc->sequences, (gpointer) sequence);
    if (osc_sequence->output_address)
    {
        *host = strdup (lo_address_get_hostname (osc_sequence->output_address));
        sscanf (lo_address_get_port (osc_sequence->output_address), "%d", port);
    }
    else
    {
        *host = strdup ("localhost");
        *port = 0;
    }
    *prefix = strdup (osc_sequence->output_prefix);
}

void
osc_set_sequence_input_prefix (osc_t *osc, sequence_t *sequence, const char *prefix)
{
    osc_sequence_t *osc_sequence
            = (osc_sequence_t *) g_hash_table_lookup (osc->sequences, (gpointer) sequence);
    free (osc_sequence->input_prefix);
    if ((osc_sequence->input_default_prefix = (prefix == NULL)))
    {
        char *name = sequence_get_name (sequence);
        osc_sequence->input_prefix = malloc (strlen (name) + 2);
        sprintf (osc_sequence->input_prefix, "/%s", name);
        free (name);
    }
    else
        osc_sequence->input_prefix = strdup (prefix);

    int i;
    for (i = 0; i < osc->methods_num; i++)
    {
        osc_method_t *method = osc->methods[i];
        if (method->instance == sequence)
        {
            char *old_path = osc_get_method_path (method);
            lo_server_thread_del_method (osc->server, old_path, method->def->typespec);
            free (method->prefix);
            method->prefix = strdup (osc_sequence->input_prefix);
            char *path = osc_get_method_path (method);
            lo_server_thread_add_method (osc->server, path, method->def->typespec,
                                         method->wrapper, method);
#ifdef PRINT_EXTRA_DEBUG
            DEBUG ("Renamed %s to %s", old_path, path);
#endif
            free (path);
            free (old_path);
        }
    }
}

int
osc_get_sequence_input_prefix (osc_t *osc, sequence_t *sequence, char **prefix)
{
    osc_sequence_t *osc_sequence
            = (osc_sequence_t *) g_hash_table_lookup (osc->sequences, (gpointer) sequence);
    *prefix = strdup (osc_sequence->input_prefix);
    return osc_sequence->input_default_prefix;
}
