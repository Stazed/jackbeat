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
 *   SVN:$Id: jab.c 640 2009-11-23 19:08:34Z olivier $
 */

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <locale.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "config.h"
#include "sequence.h"
#include "jab.h"
#include "util.h"
#include "error.h"
#include "core/array.h"

#ifdef MEMDEBUG
#include "memdebug.h"
#endif

#ifdef DMALLOC
#include "dmalloc.h"
#endif

// Minimum version of Jackbeat required to opened JAB files saved here
#define JAB_MIN_VERSION "0.7"

#define DEBUG(M, ...) { printf("JAB  %s(): ", __func__); printf(M, ## __VA_ARGS__); printf("\n"); }

struct jab_t
{
    FILE *              fd;
    sequence_t **       sequences;
    int                 sequences_num;
    char                path[512];
    char                samples_path[512];
    char                tmpdir[512];
    xmlDocPtr           xml_des;
    int                 cur_sequence;
    int                 mode;
    progress_callback_t progress_callback;
    void *              progress_data;
    double              progress_ratio;
    int                 progress_step;
} ;

#define JAB_XML_STRING 1
#define JAB_XML_FLOAT 2
#define JAB_XML_INTEGER 3

int _jab_xml_scan (xmlDocPtr doc, char *xroot, char *xsub, void *content, int type);

jab_t *
jab_open (char *path, int mode, progress_callback_t progress_callback,
          void *progress_data, int *error)
{
    jab_t *jab = NULL;
    *error = 0;
    FILE *fd;
    char wd[256];
    
    if (getcwd (wd, 256) == NULL)
    {
        DEBUG("ERROR: getcwd: %d", errno);
        return NULL;
    }

    DEBUG ("Current working directory : %s", wd);
    switch (mode)
    {
        case JAB_READ:
            fd = fopen (path, "r");
            if (fd)
            {
                char signature[4] = "";
                fread (signature, 1, 4, fd);
                fclose (fd);
                if (strncmp ("jab/", signature, 4) == 0)
                {
                    char *tmpdir = util_mktmpdir ();
                    if (tmpdir)
                    {
                        progress_callback ("Extracting files (tar)", 0, progress_data);
                        char str[512];
                        if (util_exec ("tar", "-C", tmpdir, "-xf", path, NULL))
                        {
                            // jab.xml may be renamed to song.xml in a later release to break 
                            // compatibility with jackbeat versions < 0.7, that did not had any
                            // compatibility checking support as provided by minVersion
                            sprintf (str, "%s/jab/jab.xml", tmpdir);
                            xmlDocPtr xdoc = xmlParseFile (str);
                            if (xdoc == NULL)
                            {
                                sprintf (str, "%s/jab/song.xml", tmpdir);
                                xdoc = xmlParseFile (str);
                            }
                            if (xdoc != NULL)
                            {
                                char minVersion[16];
                                // minVersion was added in jackbeat 0.7. If it's not there it means we have a 
                                // jab created with jackbeat version < 0.7, and that we can open the file.
                                if (!_jab_xml_scan (xdoc, "/jackbeat", "/minVersion", minVersion, JAB_XML_STRING)
                                    || util_version_cmp (minVersion, VERSION) >= 0)
                                {
                                    jab = calloc (1, sizeof (jab_t));
                                    jab->xml_des = xdoc;
                                    jab->cur_sequence = 1;
                                    sprintf (jab->samples_path, "%s/jab/samples", tmpdir);
                                    jab->mode = JAB_READ;
                                    jab->progress_callback = progress_callback;
                                    jab->progress_data = progress_data;
                                    strcpy (jab->tmpdir, tmpdir);
                                }
                                else
                                {
                                    *error = ERR_JAB_VERSION;
                                }
                            }
                        }

                        if (!jab)
                        {
                            util_wipe_tmpdir (tmpdir);
                            DEBUG ("Done")
                        }
                        free (tmpdir);
                    }
                }
            }

            if (!jab && *error == 0)
                *error = ERR_JAB_EXTRACT;

            break;
        case JAB_WRITE:
            jab = calloc (1, sizeof (jab_t));
            jab->sequences = NULL;
            strcpy (jab->path, path);
            jab->mode = JAB_WRITE;
            DEBUG ("Opening jab file in write mode : %s", path)
            jab->progress_callback = progress_callback;
            jab->progress_data = progress_data;
            break;
    }
    return jab;
}

xmlXPathObjectPtr
_jab_xml_get_nodeset (xmlDocPtr doc, xmlChar *xpath)
{
    DEBUG ("Looking for %s", xpath)

    xmlXPathContextPtr context;
    xmlXPathObjectPtr result;

    context = xmlXPathNewContext (doc);
    result = xmlXPathEvalExpression (xpath, context);
    if (xmlXPathNodeSetIsEmpty (result->nodesetval))
    {
        /*                printf("No result\n");*/
        return NULL;
    }
    xmlXPathFreeContext (context);
    return result;
}

int
_jab_xml_node_exists (xmlDocPtr doc, char *xpath)
{
    return (_jab_xml_get_nodeset (doc, (xmlChar *) xpath) != NULL);
}

int
_jab_xml_scan (xmlDocPtr doc, char *xroot, char *xsub, void *content, int type)
{
    xmlXPathObjectPtr result;
    char *str;
    char xpath[256];
    sprintf (xpath, "%s%s", xroot, xsub);
    result = _jab_xml_get_nodeset (doc, (xmlChar *) xpath);
    if (result != NULL)
    {
        xmlNodeSetPtr nodeset = result->nodesetval;
        str = (char *) xmlNodeListGetString (doc, nodeset->nodeTab[0]->xmlChildrenNode, 1);
        g_strstrip (str);
        switch (type)
        {
            case JAB_XML_STRING: strcpy ((char *) content, str);
                break;
            case JAB_XML_FLOAT: sscanf (str, "%f", (float *) content);
                break;
            case JAB_XML_INTEGER: sscanf (str, "%d", (int *) content);
                break;
        }
        xmlFree ((xmlChar *) str);
        xmlXPathFreeObject (result);
        return 1;
    }
    return 0;
}

void
jab_progress_callback (char * status, double fraction, void * data)
{
    jab_t *jab = (jab_t *) data;
    jab->progress_callback (status,
                            0.1 + jab->progress_ratio * ((double) jab->progress_step + fraction),
                            jab->progress_data);
}

sequence_t *
jab_retrieve_sequence (jab_t *jab, stream_t *stream, char *sequence_name, int *error)
{
    sequence_t *sequence = NULL;
    char str[256];
    int x, y, z;
    float f;
    char root[128];
    int seq_error;

    char *ext_locale = setlocale (LC_NUMERIC, NULL);
    setlocale (LC_NUMERIC, "C");

    jab->progress_callback ("Reading XML description", 0.1, jab->progress_data);

    sprintf (root, "/jackbeat/sequence[%d]", jab->cur_sequence++);

    if (!_jab_xml_node_exists (jab->xml_des, root)) *error = ERR_JAB_XML;
    else
    {
        /* Parsing global sequence settings */
        if (sequence_name)
        {
            if (!(sequence = sequence_new (stream, sequence_name, &seq_error)))
            {
                *error = seq_error;
                return NULL;
            }
        }
        else
        {
            _jab_xml_scan (jab->xml_des, root, "/name", str, JAB_XML_STRING);
            if (!(sequence = sequence_new (stream, str, &seq_error)))
            {
                *error = seq_error;
                return NULL;
            }
        }

        _jab_xml_scan (jab->xml_des, root, "/bpm", &f, JAB_XML_FLOAT);
        sequence_set_bpm (sequence, f);

        _jab_xml_scan (jab->xml_des, root, "/tracksNumber", &x, JAB_XML_INTEGER);
        jab->progress_ratio = 0.8 / (double) x;
        _jab_xml_scan (jab->xml_des, root, "/beatsNumber", &y, JAB_XML_INTEGER);
        _jab_xml_scan (jab->xml_des, root, "/measureLength", &z, JAB_XML_INTEGER);
        sequence_resize (sequence, x, y, z, 0);

        int tracks_num = x;
        int beats_num = y;

        _jab_xml_scan (jab->xml_des, root, "/isLooping", &x, JAB_XML_INTEGER);
        if (x) sequence_set_looping (sequence);
        else sequence_unset_looping (sequence);

        /* Parsing tracks */
        char track[128];
        sprintf (track, "%s/track[1]", root);
        int i = 0;
        sample_t *sample;
        char spath[513];
        jab->progress_step = 0;
        if (_jab_xml_node_exists (jab->xml_des, track) && (i < tracks_num))
        {
            do
            {
                _jab_xml_scan (jab->xml_des, track, "/name", str, JAB_XML_STRING);
                sequence_normalize_name (str);
                if (sequence_force_track_name (sequence, i, str))
                {
                    *error = ERR_INTERNAL;
                    sequence_destroy (sequence);
                    return NULL;
                }

                _jab_xml_scan (jab->xml_des, track, "/isMuted", &x, JAB_XML_INTEGER);
                sequence_mute_track (sequence, x, i);

                x = 0;
                _jab_xml_scan (jab->xml_des, track, "/isSolo", &x, JAB_XML_INTEGER);
                sequence_solo_track (sequence, x, i);

                if (_jab_xml_scan (jab->xml_des, track, "/mask", str, JAB_XML_STRING)
                    && (strncmp (str, "enabled", 7) == 0))
                    sequence_enable_mask (sequence, i);

                if (_jab_xml_scan (jab->xml_des, track, "/smoothing", str, JAB_XML_STRING)
                    && (strncmp (str, "disabled", 7) == 0))
                    sequence_set_smoothing (sequence, i, 0);
                else
                    sequence_set_smoothing (sequence, i, 1);

                if (_jab_xml_scan (jab->xml_des, track, "/volume", &f, JAB_XML_FLOAT))
                    sequence_set_volume (sequence, i, f);

                sprintf (str, "%s/sample", track);
                if (_jab_xml_node_exists (jab->xml_des, str))
                {
                    _jab_xml_scan (jab->xml_des, track, "/sample/file", str,
                                   JAB_XML_STRING);
                    sprintf (spath, "%s/%s", jab->samples_path, str);
                    sample = sample_new (spath, jab_progress_callback, (void *) jab);
                    sequence_set_sample (sequence, i, sample);
                }

                if (_jab_xml_scan (jab->xml_des, track, "/pitch", &f, JAB_XML_FLOAT))
                    sequence_set_pitch (sequence, i, f);

                sprintf (track, "%s/track[%d]", root, ++i + 1);
                jab->progress_step++;
            }
            while (_jab_xml_node_exists (jab->xml_des, track) && (i < tracks_num));
        }

        /* Parsing pattern */
        jab->progress_callback ("Reading XML description", 0.9, jab->progress_data);
        char beat[128];
        sprintf (beat, "%s/pattern/beat[1]", root);
        i = 0;
        int t, j;
        gchar **bs;
        if (_jab_xml_node_exists (jab->xml_des, beat))
        {
            do
            {
                _jab_xml_scan (jab->xml_des, beat, "", str, JAB_XML_STRING);

                bs = g_strsplit_set (str, " -", -1);

                for (j = 0, t = 0; bs[j] && t < tracks_num; j++)
                {
                    if (strlen (bs[j]) > 0)
                    {
                        int assigned =  sscanf (bs[j], "%d+%d", &x, &y);
                        DEBUG ("bs[%d] = %s (assigned:%d, beat:%d, mask:%d)", j, bs[j], assigned, x, y)
                        if (assigned == 1)
                        {
                            sequence_set_beat (sequence, t++, i, x);
                        }
                        else if (assigned == 2)
                        {
                            sequence_set_beat (sequence, t, i, x);
                            sequence_set_mask_beat (sequence, t++, i, y);
                        }
                    }
                }

                g_strfreev (bs);

                sprintf (beat, "%s/pattern/beat[%d]", root, ++i + 1);
            }
            while (i < beats_num && _jab_xml_node_exists (jab->xml_des, beat));
        }
        jab->progress_callback ("Done", 1, jab->progress_data);
    }

    sequence_wait (sequence);
    //sequence_process_events (sequence);

    setlocale (LC_NUMERIC, ext_locale);

    return sequence;
}

void
jab_add_sequence (jab_t *jab, sequence_t *sequence)
{
    ARRAY_ADD (sequence_t, jab->sequences, jab->sequences_num, sequence)
}

#define GS(F, ...) g_string_append_printf (gs, F, ## __VA_ARGS__)

GString *
_jab_get_xml_description (jab_t *jab)
{
    int i, j, k, tn, bn;
    GString *gs = g_string_new ("");

    char *ext_locale = setlocale (LC_NUMERIC, NULL);
    setlocale (LC_NUMERIC, "C");

    GS ("<?xml version=\"1.0\"?>\n");
    GS ("<jackbeat>\n");
    GS ("  <version> %s </version>\n", VERSION);
    GS ("  <minVersion> %s </minVersion>\n", JAB_MIN_VERSION);

    sequence_t *s;
    for (i = 0; i < jab->sequences_num; i++)
    {
        s = jab->sequences[i];
        tn = sequence_get_tracks_num (s);
        bn = sequence_get_beats_num (s);
        char *name = sequence_get_name (s);

        GS ("  <sequence>\n");
        GS ("    <name> %s </name>\n", name);
        GS ("    <bpm> %f </bpm>\n", sequence_get_bpm (s));
        GS ("    <tracksNumber> %d </tracksNumber>\n", tn);
        GS ("    <beatsNumber> %d </beatsNumber>\n", bn);
        GS ("    <measureLength> %d </measureLength>\n",
            sequence_get_measure_len (s));
        GS ("    <isLooping> %d </isLooping>\n", sequence_is_looping (s));
        free (name);

        sample_t *sample;
        for (j = 0; j < tn; j++)
        {
            GS ("    <track>\n");
            name = sequence_get_track_name (s, j);
            GS ("      <name> %s </name>\n", name);
            free (name);
            GS ("      <isMuted> %d </isMuted>\n", sequence_track_is_muted (s, j));
            GS ("      <isSolo> %d </isSolo>\n", sequence_track_is_solo (s, j));
            GS ("      <pitch> %f </pitch>\n", sequence_get_pitch (s, j));
            GS ("      <volume> %f </volume>\n", sequence_get_volume (s, j));
            GS ("      <mask> %s </mask>\n", (sequence_is_enabled_mask (s, j)) ?
                "enabled" : "disabled");
            GS ("      <smoothing> %s </smoothing>\n", (sequence_get_smoothing (s, j)) ?
                "enabled" : "disabled");
            sample = sequence_get_sample (s, j);
            if (sample)
            {
                GS ("      <sample>\n");
                GS ("        <name> %s </name>\n", sample->name);
                GS ("        <file> %s </file>\n",
                    sample_storage_basename (sample));
                GS ("      </sample>\n");
            }
            GS ("    </track>\n");
        }
        GS ("    <pattern>\n");

        for (j = 0; j < bn; j++)
        {
            GS ("      <beat> ");
            for (k = 0; k < tn - 1; k++)
            {
                if (sequence_is_enabled_mask (s, k))
                    GS ("%d+%d - ", sequence_get_beat (s, k, j),
                        sequence_get_mask_beat (s, k, j));
                else
                    GS ("%d - ", sequence_get_beat (s, k, j));
            }
            if (sequence_is_enabled_mask (s, k))
                GS ("%d+%d </beat>\n", sequence_get_beat (s, k, j),
                    sequence_get_mask_beat (s, k, j));
            else
                GS ("%d </beat>\n", sequence_get_beat (s, k, j));
        }

        GS ("    </pattern>\n");
        GS ("  </sequence>\n");
    }
    GS ("</jackbeat>\n");

    setlocale (LC_NUMERIC, ext_locale);
    return gs;
}

int
jab_close (jab_t *jab)
{
    //FIXME: not checking for duplicate samples. That is going
    //       to sample_write() them twice or more.
    //FIXME: different samples with the same base name may conflict

    int success = 0;

    char p[512];
    if (jab->mode == JAB_WRITE)
    {
        char *tmpdir = util_mktmpdir ();
        if (tmpdir)
        {
            sprintf (p, "%s/jab", tmpdir);
            if (util_mkdir (p, 0700))
            {
                jab->progress_callback ("Writing XML description", 0,
                                        jab->progress_data);
                sprintf (p, "%s/jab/jab.xml", tmpdir);
                FILE *xml = fopen (p , "w");
                if (xml)
                {
                    GString *gs = _jab_get_xml_description (jab);
                    if (gs)
                    {
                        fputs (gs->str, xml);
                        g_string_free (gs, TRUE);
                        fclose (xml);
                        sprintf (p, "%s/jab/samples", tmpdir);
                        util_mkdir (p, 0700);
                        int i, j, tn;
                        sample_t *s;
                        for (i = 0, j = 0; i < jab->sequences_num; i++)
                            j += sequence_get_tracks_num (jab->sequences[i]);
                        jab->progress_ratio = 0.8 / (double) j;
                        jab->progress_step = 0;
                        for (i = 0; i < jab->sequences_num; i++)
                        {
                            tn = sequence_get_tracks_num (jab->sequences[i]);
                            int err = 0;
                            for (j = 0; j < tn && (!err); j++)
                            {
                                s = sequence_get_sample (jab->sequences[i], j);
                                if (s)
                                {
                                    sprintf (p, "%s/jab/samples/%s", tmpdir,
                                             sample_storage_basename (s));
                                    if (!sample_write (s, p, jab_progress_callback, (void *) jab)) err = 1;
                                }
                                jab->progress_step++;
                            }
                            if (!err)
                            {
                                jab->progress_callback ("Packing with tar", 0.9,
                                                        jab->progress_data);
                                success = util_exec ("tar", "-C", tmpdir, "-cf", jab->path, "jab", NULL);
                                DEBUG ("Done")
                            }
                        }
                    }
                }
            }

            util_wipe_tmpdir (tmpdir);
            free (tmpdir);
            DEBUG ("Done")
            if (success) jab->progress_callback ("Done", 1, jab->progress_data);
        }
    }
    else if (jab->mode == JAB_READ)
    {
        util_wipe_tmpdir (jab->tmpdir);
        DEBUG ("Done")
        success = 1;
    }

    free (jab);
    jab = NULL;
    return success;
}
