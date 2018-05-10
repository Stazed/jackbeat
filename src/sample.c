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
 *   SVN:$Id: sample.c 600 2009-06-24 23:16:41Z olivier $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>
#include <config.h>
#include <math.h>
#include <assert.h>

#include "sample.h"
#include "util.h"
#include "core/event.h"

#ifdef MEMDEBUG
#include "memdebug.h"
#endif

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define DEBUG(M, ...) { printf("SAM  %s(): ", __func__); printf(M, ## __VA_ARGS__); printf("\n"); }

#define SAMPLE_BLOCK_SIZE 0xffff // Number of frames to read/write at once from/to sample files

char **
sample_list_known_extensions ()
{
    int i, ii, j, n;
    SF_FORMAT_INFO info;
    sf_command (NULL, SFC_GET_FORMAT_MAJOR_COUNT, &ii, sizeof (ii));

    n = 0;
    char **list = NULL;

    DEBUG ("Supported extensions: ");
    for (i = 0; i < ii; i++)
    {
        info.format = i;
        sf_command (NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof (info));
        for (j = 0; j < n; j++)
        {
            if (!strcmp ((char *) info.extension, list[j]))
            {
                break;
            }
        }
        if (j == n)
        {
            list = realloc (list, (n + 1) * sizeof (char *));
            list[n++] = (char *) info.extension;
            printf ("%s ", info.extension);
        }
    }
    printf ("\n");

    list = realloc (list, (n + 1) * sizeof (char *));
    list[n] = NULL;
    return list;
}

void static
sample_strip_filename_extension (char *filename)
{
    char **extensions = sample_list_known_extensions ();

    int len_fname = strlen (filename);
    char *_ext, ext[] = ".        ";
    int i = 0;
    while ((_ext = extensions[i++]) != NULL)
    {
        strcpy (ext + 1, _ext);
        int len_ext = strlen (ext);

        if ((len_fname > len_ext)
            && (strncasecmp (filename + len_fname - len_ext,
                             ext, len_ext) == 0))
        {
            filename[len_fname - len_ext] = '\0';
            break;
        }
    }

    free (extensions);
}

sample_t *
sample_new (char *filename, progress_callback_t progress_callback,
            void *progress_data)
{
    SNDFILE *fd;
    SF_INFO info;
    struct stat statd;

    info.format = 0;
    if ((fd = sf_open (filename, SFM_READ, &info)))
    {
        char status[128];
        sprintf (status, "Opened %s", basename (filename));
        progress_callback (status, 0, progress_data);

        sample_t * sample      = calloc (1, sizeof (sample_t));
        event_register (sample, "destroy");

        sample->channels_num   = info.channels;
        sample->framerate = info.samplerate;
        sample->orig_format    = info.format;
        sample->frames         = info.frames;
        strcpy (sample->filename, filename);
        stat (filename, &statd);
        sample->last_file_ctime = statd.st_ctime;
        sample->last_file_size = statd.st_size;
        sample->ref_num = 0;


        char *y = strdup (filename);
        strcpy (sample->name, basename (y));
        free (y);
        sample_strip_filename_extension (sample->name);

        DEBUG ("Sample name : %s", sample->name);
        DEBUG ("Channels : %d", sample->channels_num);
        DEBUG ("Frame rate: %d", sample->framerate);
        DEBUG ("Total number of frames : %ld", (long int) sample->frames);

        sample->data = calloc (sample->channels_num * sample->frames, sizeof (float));

        sprintf (status, "Importing %s", basename (filename));
        progress_callback (status, 0, progress_data);

        sf_count_t read = 0, shift = 0, space = 0;

        do
        {
            space = (shift + SAMPLE_BLOCK_SIZE > sample->frames)
                    ? sample->frames - shift
                    : SAMPLE_BLOCK_SIZE;
            DEBUG ("About to read %d frames", (int) space);
            read = sf_readf_float (fd, sample->data + shift *
                                   sample->channels_num, space);
            shift += read;
            DEBUG ("Read %d frames", (int) read);
            progress_callback (status, (double) shift / sample->frames,
                               progress_data);
        }
        while (read == SAMPLE_BLOCK_SIZE);

        sample->peak = 0;
        float level;
        sf_count_t i, ii = sample->channels_num * sample->frames;

        for (i = 0; i < ii; i++)
        {
            level = fabsf (sample->data[i]);
            if (level > sample->peak) sample->peak = level;
        }

        DEBUG ("Sample peak: %f", sample->peak);

        sf_close (fd);

        return sample;
    }
    else
    {
        DEBUG ("sf_open() failed on: %s", filename);
        return NULL;
    }
}

char *
sample_storage_basename (sample_t *sample)
{
    char *fname = malloc (strlen (sample->name) + 6);
    strcpy (fname, sample->name);
    util_strcanon (fname,
                   "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-_0123456789",
                   '_');

    SF_FORMAT_INFO info;
    info.format = sample->orig_format;
    sf_command (NULL, SFC_GET_FORMAT_INFO, &info, sizeof (info));
    strcat (fname, ".");
    strcat (fname, info.extension);
    return fname;
}

int
sample_write (sample_t *sample, char *path,
              progress_callback_t progress_callback,
              void *progress_data)
{
    SF_INFO info;
    SNDFILE *fd;
    sf_count_t i;
    char status[128];

    sprintf (status, "Exporting %s", basename (path));
    progress_callback (status, 0, progress_data);

    info.samplerate = sample->framerate;
    info.channels = sample->channels_num;
    info.format = (sample->orig_format) ? sample->orig_format : 0x010002;

    DEBUG ("Saving sample \"%s\" to \"%s\" [frames: %d, channels: %d, rate: %d]",
           sample->name, path, (int) (sample->frames), sample->channels_num,
           sample->framerate)

    if (!(fd = sf_open (path, SFM_WRITE, &info))) return 0;

    sf_count_t wr_frames = SAMPLE_BLOCK_SIZE;

    int ii = sample->frames / SAMPLE_BLOCK_SIZE + 1;
    for (i = 0; i < ii; i++)
    {
        if (i == ii - 1) wr_frames = sample->frames % SAMPLE_BLOCK_SIZE;
        DEBUG ("  Writing %d frames", (int) wr_frames)
        sf_writef_float (fd, sample->data + i * SAMPLE_BLOCK_SIZE *
                         sample->channels_num, wr_frames);
        progress_callback (status, ((double) i + 1) / ii, progress_data);
    }

    sf_close (fd);
    progress_callback (status,  1, progress_data);
    return 1;
}

int
sample_compare (sample_t * sample, char *filename)
{
    struct stat statd;
    return ((strcmp (filename, sample->filename) == 0)
            && (stat (filename, &statd) == 0)
            && (statd.st_ctime == sample->last_file_ctime)
            && (statd.st_size == sample->last_file_size))
            ? 0 : 1;
}

void
sample_ref (sample_t *sample)
{
    sample->ref_num++;
}

void
sample_unref (sample_t *sample)
{
    DEBUG ("sample '%s' is referenced %d time(s)", sample->name, sample->ref_num);
    if (sample->ref_num-- <= 1)
    {
        event_fire (sample, "destroy", NULL, NULL);
        event_remove_source (sample);
        DEBUG ("freeing memory");
        free (sample->data);
        free (sample);
    }
    else
    {
        DEBUG ("not freeing memory: still in use");
    }
}

