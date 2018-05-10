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
 *   SVN:$Id: rc.c 586 2009-06-23 15:42:46Z olivier $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sndfile.h>
#include <gtk/gtk.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "rc.h"
#include "util.h"
#include "stream/device.h"
#include "sequence.h"

#ifdef MEMDEBUG
#include "memdebug.h"
#endif

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define DEBUG(M, ...) { \
  printf("RC  %s(): ", __func__); \
  printf(M, ## __VA_ARGS__); printf("\n"); \
}

void
rc_read (rc_t *rc)
{
    FILE *fd;
    char *path;
    char s[256];
    char key[256];
    char val[256];
    struct stat b;


    sprintf (rc->sample_wdir, "%s/", util_home_dir ());
    sprintf (rc->sequence_wdir, "%s/", util_home_dir ());
    rc->sample_history_num = 0;
    rc->sequence_history_num = 0;
    rc->default_resampler_type = SEQUENCE_SINC;
    rc->transport_aware = 1;
    rc->transport_query = 1;
    rc->auto_connect = 1;
    rc->jack_auto_start = 1;
    strcpy (rc->client_name, PACKAGE_NAME);
    strcpy (rc->audio_output, stream_device_get_default ());
    rc->audio_sample_rate = 44100;

    path = util_settings_dir ();
    if (stat (path, &b) == 0 && S_ISREG (b.st_mode)) strcpy (s, path);
    else sprintf (s, "%s/config", path);
    util_path (s);

    fd = fopen (s, "r");
    if (fd)
    {
        while (fgets (s, 256, fd))
        {
            if ((sscanf (s, "%s = \"%[^\"]\"", key, val) == 2) || (sscanf (s, "%s = %s", key, val) == 2))
            {
                if (strcmp (key, "sample_wdir") == 0)
                    strcpy (rc->sample_wdir, val);
                else if (strcmp (key, "pattern_wdir") == 0)
                    strcpy (rc->sequence_wdir, val);
                else if ((strcmp (key, "sample_history") == 0) && rc->sample_history_num < 10)
                    strcpy (rc->sample_history[rc->sample_history_num++], val);
                else if ((strcmp (key, "sequence_history") == 0) && rc->sequence_history_num < 10)
                    strcpy (rc->sequence_history[rc->sequence_history_num++], val);
                else if (strcmp (key, "default_resampler_type") == 0)
                    rc->default_resampler_type = util_str_to_resampler_type (val);
                else if (strcmp (key, "transport_aware") == 0)
                    sscanf (val, "%d", &(rc->transport_aware));
                else if (strcmp (key, "transport_query") == 0)
                    sscanf (val, "%d", &(rc->transport_query));
                else if (strcmp (key, "auto_connect") == 0)
                    sscanf (val, "%d", &(rc->auto_connect));
                else if (strcmp (key, "client_name") == 0)
                    strcpy (rc->client_name, val);
                else if (strcmp (key, "audio_output") == 0)
                    strcpy (rc->audio_output, val);
                else if (strcmp (key, "audio_sample_rate") == 0)
                    sscanf (val, "%d", &(rc->audio_sample_rate));
                else if (strcmp (key, "jack_auto_start") == 0)
                    sscanf (val, "%d", &(rc->jack_auto_start));
            }
        }
        fclose (fd);
        if (stat (path, &b) == 0 && S_ISREG (b.st_mode))
        {
            g_print ("Migrating rc settings from file to directory (.jackbeat)\n");
            unlink (path);
            util_mkdir (path, 0755);
            rc_write (rc);
        }
    }
    else
    {
        int test;
        test = util_mkdir (path, 0755);
        if (test)
        {
            printf ("Unable to create .jackbeat\n");
            perror ("file_read_rc");
        }
    }
}

void
rc_write (rc_t * rc)
{
    FILE *fd;
    char s[256];
    int i;

    sprintf (s, "%s/config", util_settings_dir ());
    util_path (s);
    if ((fd = fopen (s, "w")))
    {
        if (strlen (rc->sample_wdir)) fprintf (fd, "sample_wdir = \"%s\"\n", rc->sample_wdir);
        if (strlen (rc->sequence_wdir)) fprintf (fd, "pattern_wdir = \"%s\"\n", rc->sequence_wdir);
        for (i = 0; i < rc->sample_history_num; i++)
            fprintf (fd , "sample_history = \"%s\"\n", rc->sample_history[i]);
        for (i = 0; i < rc->sequence_history_num; i++)
            fprintf (fd , "sequence_history = \"%s\"\n", rc->sequence_history[i]);
        if (rc->default_resampler_type != -1)
            fprintf (fd, "default_resampler_type = %s\n",
                     util_resampler_type_to_str (rc->default_resampler_type));
        fprintf (fd, "transport_aware = %d\n", rc->transport_aware);
        fprintf (fd, "transport_query = %d\n", rc->transport_query);
        fprintf (fd, "auto_connect = %d\n", rc->auto_connect);
        fprintf (fd, "client_name = \"%s\"\n", rc->client_name);
        fprintf (fd, "audio_output = \"%s\"\n", rc->audio_output);
        fprintf (fd, "audio_sample_rate = %d\n", rc->audio_sample_rate);
        fprintf (fd, "jack_auto_start = %d\n", rc->jack_auto_start);
        fclose (fd);
    }
    else perror ("file_write_rc");
}

static void
rc_add_to_history (history_t history, int *num, char *item)
{
    char buf[512];
    int in_hist = 0;
    int i, j;
    for (i = 0; i < *num; i++)
    {
        if (strcmp (history[i], item) == 0)
        {
            in_hist = 1;
            if (i != 0)
            {
                strcpy (buf, history[i]);
                for (j = i - 1; j >= 0; j--) strcpy (history[j + 1], history[j]);

                strcpy (history[0], buf);
            }
        }
    }
    if (!in_hist)
    {
        int jj = (*num == 10) ? 8 : *num - 1;
        for (j = jj; j >= 0; j--)
            strcpy (history[j + 1], history[j]);

        strcpy (history[0], item);
        if (*num < 10) (*num)++;
    }
}

void
rc_add_sample (rc_t *rc, char *filename)
{
    DEBUG ("adding sample to history at index %d: %s", rc->sample_history_num, filename);
    rc_add_to_history (rc->sample_history, &(rc->sample_history_num), filename);
}

void
rc_add_sequence (rc_t *rc, char *filename)
{
    DEBUG ("adding sequence to history at index %d: %s", rc->sequence_history_num, filename);
    rc_add_to_history (rc->sequence_history, &(rc->sequence_history_num), filename);
}
