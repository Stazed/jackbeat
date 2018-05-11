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
 *   SVN:$Id: song.c 517 2009-04-11 15:26:45Z olivier $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <config.h>
#include <semaphore.h>

#include "song.h"
#include "core/event.h"
#include "core/pool.h"
#include "core/array.h"

#ifdef MEMDEBUG
#include "memdebug.h"
#endif

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define DEBUG(M, ...) { printf("SNG  %s(): ", __func__); printf(M, ## __VA_ARGS__); printf("\n"); }
#define FREE_AND_RETURN(P,V) { free(P); return V; }
#define JACK_CLIENT_NAME "jackbeat"

struct song_t
{
    pool_t *      pool;
    sequence_t ** sequences;
    int           sequences_num;
    sample_t **   samples;
    int           samples_num;
    sem_t         mutex;
} ;

static void song_on_sample_destroy (event_t *event);
static void song_on_sequence_destroy (event_t *event);

song_t *
song_new (pool_t *pool)
{
    song_t *song = calloc (1, sizeof (song_t));
    event_register (song, "sequence-registered");

    song->pool = pool;
    song->sequences = NULL;
    song->sequences_num = 0;
    song->samples = NULL;
    song->samples_num = 0;
    sem_init (&(song->mutex), 0, 1);
    return song;

}

void
song_destroy (song_t * song)
{
    sem_wait (&(song->mutex));
    sem_destroy (&(song->mutex));
    if (song->sequences) free (song->sequences);
    if (song->samples) free (song->samples);
    free (song);
}

void
song_register_sequence (song_t *song, sequence_t *sequence)
{
    ARRAY_ADD (sequence_t, song->sequences, song->sequences_num, sequence)
    event_subscribe (sequence, "destroy", song, song_on_sequence_destroy);
    sequence_activate (sequence, song->pool);
    event_fire (song, "sequence-registered", sequence, NULL);
}

void
song_register_sequence_samples (song_t *song, sequence_t *sequence)
{
    int tn = sequence_get_tracks_num (sequence);
    int i;
    sample_t *sample;
    for (i = 0; i < tn; i++)
        if ((sample = sequence_get_sample (sequence, i)))
            song_register_sample (song, sample);
}

int
song_count_sequences (song_t * song)
{
    return song->sequences_num;
}

sequence_t **
song_list_sequences (song_t * song)
{
    return song->sequences;
}

void
song_register_sample (song_t *song, sample_t *sample)
{
    int i;
    for (i = 0; i < song->samples_num; i++)
        if (song->samples[i] == sample)
        {
            DEBUG ("Warning: sample '%s' is already registered", sample->name);
            return;
        }
    ARRAY_ADD (sample_t, song->samples, song->samples_num, sample)
    event_subscribe (sample, "destroy", song, song_on_sample_destroy);
}

int
song_get_sequence_index (song_t *song)
{
    return song->sequences_num;
}

sample_t *
song_try_reuse_sample (song_t *song, char *filename)
{
    int i;
    for (i = 0; i < song->samples_num; i++)
        if (sample_compare (song->samples[i], filename) == 0)
            return song->samples[i];

    return NULL;
}

// Event handlers

static void
song_on_sample_destroy (event_t *event)
{
    song_t * song = (song_t *) event->self;
    sample_t * sample = (sample_t *) event->source;
    DEBUG ("Unregistering sample: %s", sample->name);
    ARRAY_REMOVE (sample_t, song->samples, song->samples_num, sample)
}

static void
song_on_sequence_destroy (event_t *event)
{
    song_t * song = (song_t *) event->self;
    sequence_t * sequence = (sequence_t *) event->source;
    char *name = sequence_get_name (sequence);
    DEBUG ("Unregistering sequence: %s", name);
    free (name);
    assert (song->sequences);
    ARRAY_REMOVE (sequence_t, song->sequences, song->sequences_num, sequence)
}

/* Currently song_lock and song_unlock are unused */
/*
static void
song_lock (song_t *song)
{
    sem_wait (&(song->mutex));
}

static void
song_unlock (song_t *song)
{
    sem_post (&(song->mutex));
}
*/

pool_t *
song_pool (song_t *song)
{
    return song->pool;
}
