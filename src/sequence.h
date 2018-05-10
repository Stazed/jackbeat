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
 *   SVN:$Id: sequence.h 643 2009-11-23 19:38:59Z olivier $
 */

#ifndef JACKBEAT_SEQUENCE_H
#define JACKBEAT_SEQUENCE_H

#include "sample.h"
#include "types.h"
#include "core/event.h"
#include "core/pool.h"
#include "stream/stream.h"

#define SEQUENCE_SINC   1
#define SEQUENCE_LINEAR 2

#define SEQUENCE_SUSTAIN_LOOP     1
#define SEQUENCE_SUSTAIN_TRUNCATE 2
#define SEQUENCE_SUSTAIN_KEEP     3

#define SEQUENCE_DBMAX 10.0f
#define SEQUENCE_DBMIN -70.0f
#define SEQUENCE_DB2GAIN(db) powf(10.0f, (double) db * 0.05f)
#define SEQUENCE_GAIN2DB(gain) (20.0f * log10f (gain))

// see: http://www.dr-lex.be/info-stuff/volumecontrols.html
#define SEQUENCE_SLIDE2GAIN(slide) (exp((double) slide * 6.908f) / 1000.0f * SEQUENCE_DB2GAIN (SEQUENCE_DBMAX))
#define SEQUENCE_GAIN2SLIDE(gain) (log((double) gain * 1000.0f / SEQUENCE_DB2GAIN (SEQUENCE_DBMAX)) / 6.908f)
#define SEQUENCE_SLIDE2DB(slide) SEQUENCE_GAIN2DB(SEQUENCE_SLIDE2GAIN(slide))
#define SEQUENCE_DB2SLIDE(db) SEQUENCE_GAIN2SLIDE(SEQUENCE_DB2GAIN(db))

typedef struct sequence_t sequence_t;

typedef enum sequence_track_type_t {
    EMPTY,
    SAMPLE,
    NESTED
} sequence_track_type_t;

typedef struct sequence_position_t {
    int track;
    int beat;
} sequence_position_t;

/* Sequence object construction and destruction */
sequence_t * sequence_new(stream_t *stream, char *name, int *error);
void sequence_activate(sequence_t *sequence, pool_t *pool);
void sequence_destroy(sequence_t *sequence);
//void          sequence_process_events (sequence_t *sequence);
void sequence_wait(sequence_t *sequence);

/* Waveform export */
void sequence_export(sequence_t *sequence, char *filename, int framerate,
        int sustain_type,
        progress_callback_t progress_callback, void *progress_data);

/* Global settings and informations */
char * sequence_get_name(sequence_t *sequence);
unsigned long sequence_get_framerate(sequence_t *sequence);
void sequence_set_resampler_type(sequence_t * sequence, int type);
int sequence_get_resampler_type(sequence_t *sequence);
int sequence_get_error(sequence_t * sequence);
void sequence_normalize_name(char *name);

/* Transport control */
void sequence_set_transport(sequence_t *sequence, int respond, int query);
void sequence_get_transport(sequence_t *sequence, int *respond, int *query);
void sequence_start(sequence_t *sequence);
void sequence_stop(sequence_t *sequence);
void sequence_rewind(sequence_t *sequence);
int sequence_is_playing(sequence_t *sequence);

/* Dimensions, BPM and looping */
int sequence_resize(sequence_t *sequence, int tracks_num, int beats_num,
        int measure_len, int duplicate_beats);
void sequence_remove_track(sequence_t *sequence, int track);
int sequence_get_tracks_num(sequence_t *sequence);
int sequence_get_beats_num(sequence_t *sequence);
int sequence_get_measure_len(sequence_t *sequence);
float sequence_get_bpm(sequence_t *sequence);
void sequence_set_bpm(sequence_t *sequence, float bpm);
void sequence_set_looping(sequence_t *sequence);
void sequence_unset_looping(sequence_t *sequence);
int sequence_is_looping(sequence_t *sequence);
void sequence_swap_tracks(sequence_t *sequence, int track1, int track2);

/* Track informations */
char * sequence_get_track_name(sequence_t *sequence, int track);
int sequence_set_track_name(sequence_t *sequence, int track, char *name);
int sequence_force_track_name(sequence_t *sequence, int track, char *name);
int sequence_track_name_exists(sequence_t *sequence, char *name);
sequence_track_type_t sequence_get_track_type(sequence_t *sequence, int track);

/* Samples */
sample_t * sequence_get_sample(sequence_t *sequence, int track);
int sequence_set_sample(sequence_t *sequence, int track, sample_t *sample);
int sequence_get_sample_usage(sequence_t *sequence, sample_t *sample);
unsigned long sequence_get_sample_position(sequence_t *sequence, int track);

/* Track volume, pitch and smoothing */
void sequence_set_pitch(sequence_t *sequence, int track, double pitch);
double sequence_get_pitch(sequence_t *sequence, int track);
void sequence_set_volume(sequence_t *sequence, int track, double volume);
double sequence_get_volume(sequence_t *sequence, int track);
void sequence_multiply_volume(sequence_t *sequence, int track, double ratio);
void sequence_set_volume_db(sequence_t *sequence, int track, double volume);
double sequence_get_volume_db(sequence_t *sequence, int track);
void sequence_increase_volume_db(sequence_t *sequence, int track, double delta);
void sequence_mute_track(sequence_t *sequence, char status, int track);
char sequence_track_is_muted(sequence_t *sequence, int track);
void sequence_solo_track(sequence_t *sequence, char status, int track);
char sequence_track_is_solo(sequence_t *sequence, int track);
void sequence_set_smoothing(sequence_t *sequence, int track, int status);
int sequence_get_smoothing(sequence_t *sequence, int track);

/* Track-masking */
void sequence_enable_mask(sequence_t *sequence, int track);
void sequence_disable_mask(sequence_t *sequence, int track);
int sequence_is_enabled_mask(sequence_t *sequence, int track);
void sequence_set_mask_beat(sequence_t * sequence, int track, int beat, char status);
char sequence_get_mask_beat(sequence_t * sequence, int track, int beat);
int sequence_get_active_mask_beat(sequence_t *sequence, int track);

/* Beat operations */
void sequence_set_beat(sequence_t *sequence, int track, int beat, char status);
char sequence_get_beat(sequence_t *sequence, int track, int beat);
int sequence_get_active_beat(sequence_t *sequence, int track);
float sequence_get_level(sequence_t * sequence, int track);

#endif
