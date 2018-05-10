/*
 *   Jackbeat - Audio Sequencer
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
 *   SVN:$Id: sequence.c 643 2009-11-23 19:38:59Z olivier $
 */

#define SEQUENCE_MASK_ATTACK_DELAY 3.0 // Miliseconds

#include <samplerate.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>
#include <config.h>
#include <semaphore.h>

#include "sequence.h"
#include "error.h"
#include "core/msg.h"
#include "util.h"

#ifdef MEMDEBUG
#include "memdebug.h"
#endif

#ifdef DMALLOC
#include "dmalloc.h"
#endif


/********************************
 *    Main data structures      *
 ********************************/

/* SEQUENCE-------------------------------------+
   |                                            |
   | TRACK[0]---------------+       TRACK[n]-+  |
   | |                      |       |        |  |
   | | > frame position     |       |        |  |
   | | > pattern ("beats")  |       |        |  |
   | |                      |       |        |  |
   | | SAMPLE-------------+ |       |        |  |
   | | | (see sample.h)   | |       |        |  |
   | | +------------------+ |       |        |  |
   | |                      | [...] |        |  |
   | | CHANNEL[0]---------+ |       |        |  |
   | | | > stream port    | |       |        |  |
   | | | > stream buffer  | |       |        |  |
   | | +------------------+ |       |        |  |
   | |                      |       |        |  |
   | |       [...]          |       |        |  |
   | |                      |       |        |  |
   | | CHANNEL[n]---------+ |       |        |  |
   | | | > stream port    | |       |        |  |
   | | | > stream buffer  | |       |        |  |
   | | +------------------+ |       |        |  |
   | |                      |       |        |  |
   | +----------------------+       +--------+  |
   |                                            |
   +--------------------------------------------+
*/

typedef struct sequence_track_t
{
  char *          beats;
  char *          mask;

  sample_t *      sample;

  stream_port_t ** channels;
  int             channels_num;
  unsigned long * sample_input_pos;
  unsigned long * sample_output_pos;
  float **        buffers;
  unsigned long   buffers_ofs;

  int             active_beat;
  int             active_mask_beat;
  char            name[256];

  char            enabled;
  char            solo;
  char            lock;
  SRC_STATE *     sr_converter;
  int             sr_converter_type;
  float *         sr_converter_buffer;
  double          sr_converter_ratio;
  double          pitch;
  double          volume;
  double          mask_envelope;
  int             smoothing;
  float volatile  current_level;
  float           level_peak;
} sequence_track_t;


typedef enum sequence_status_t
{
  SEQUENCE_DISABLED,
  SEQUENCE_ENABLED,
} sequence_status_t;

struct sequence_t
{
  stream_t *        stream;
  pool_t *          pool;
  sequence_track_t *tracks;
  int               tracks_num;
  int               beats_num;
  int               measure_len;
  float             bpm;
  sequence_status_t status;
  int               looping;
  int               transport_aware;
  int               transport_query;
  unsigned long     framerate;
  unsigned long     buffer_size;
  char              name[32];
  int               sr_converter_default_type;
  msg_t *           msg;
  int               error;
  sem_t             mutex;
};

/****************************************************************************
 * Type and macros for messages sent from the main loop to the audio thread *
 ****************************************************************************/

typedef struct sequence_msg_t
{
  long int type;
  char text[128];
} sequence_msg_t;

#define SEQUENCE_MSG_SET_BPM        2
#define SEQUENCE_MSG_SET_TRANSPORT  4
#define SEQUENCE_MSG_SET_LOOPING    5
#define SEQUENCE_MSG_UNSET_LOOPING  6
#define SEQUENCE_MSG_START          7
#define SEQUENCE_MSG_STOP           8
#define SEQUENCE_MSG_REWIND         9
#define SEQUENCE_MSG_SET_SR_RATIO   10
#define SEQUENCE_MSG_SET_VOLUME     11
#define SEQUENCE_MSG_MUL_VOLUME     12
#define SEQUENCE_MSG_SET_SMOOTHING  13
#define SEQUENCE_MSG_MUTE_TRACK     14
#define SEQUENCE_MSG_SOLO_TRACK     15
#define SEQUENCE_MSG_SWAP_TRACKS    16

#define SEQUENCE_MSG_NO_ACK -32
#define SEQUENCE_MSG_ACK    33

#define SEQUENCE_MSG_LOCK_TRACKS          34
#define SEQUENCE_MSG_UNLOCK_TRACKS        35
#define SEQUENCE_MSG_LOCK_SINGLE_TRACK    36
#define SEQUENCE_MSG_UNLOCK_SINGLE_TRACK  37
#define SEQUENCE_MSG_RESIZE               38
#define SEQUENCE_MSG_SET_SAMPLE           39
#define SEQUENCE_MSG_ENABLE_MASK          41
#define SEQUENCE_MSG_DISABLE_MASK         42
#define SEQUENCE_MSG_ACK_STOP             43
#define SEQUENCE_MSG_ACK_ENABLE           44
#define SEQUENCE_MSG_ACK_DISABLE          45


/*******************************
 *   Various type and macros   *
 *******************************/

#define SEQUENCE_NESTED 1
#define SEQUENCE_SILENT 2

typedef struct sequence_resize_data_t
{
  sequence_track_t *tracks;
  int tracks_num;
  int beats_num;
  int measure_len;
} sequence_resize_data_t;

#define FDEBUG(F, M, ...) ({ printf("SEQ %s  %s(): ",sequence->name, F); printf(M, ## __VA_ARGS__); printf("\n"); fflush(stdout); })
#define DEBUG(M, ...) FDEBUG(__func__, M, ## __VA_ARGS__)
#define VDEBUG(M, ...) 

int dump_i;
char *dump_s;
#define DUMP(V,S) \
  { \
   dump_s = (char *)V; \
   for (dump_i=0; dump_i < S; dump_i++) printf("%.2X ", dump_s[dump_i]); \
   printf ("\n"); \
  }

#define SEQUENCE_SAFE_GETTER(T, V) { sequence_lock (sequence); T value = (V); \
                                  sequence_unlock (sequence); return value; }
#define SEQUENCE_LOCK_CALL(C) { sequence_lock (sequence); C; sequence_unlock (sequence); }
#define SEQUENCE_UNLOCK_CALL(C) { sequence_unlock (sequence); C; sequence_lock (sequence); }

#define SEQUENCE_VALID_NAME "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-._"

static int sequence_process_events (void *data);

/************************************************************
 *   Private functions running inside the realtime thread   *
 ************************************************************/

/**
 * Initialize audio stream data buffers for a given track. 
 */
static void
sequence_get_buffers (sequence_t * sequence, int track,
                      unsigned long nframes)
{
  int j;
  sequence_track_t *t = sequence->tracks + track;
  for (j = 0; j < t->channels_num; j++)
    t->buffers[j] = stream_port_get_buffer (sequence->stream, t->channels[j], nframes);
}

/**
 * Initialize audio stream data buffers for all but locked tracks.
 */
static void 
sequence_get_all_buffers (sequence_t * sequence, unsigned long nframes)
{
  
  int i;
  for (i=0; i < sequence->tracks_num; i++)
    if ((sequence->tracks+i)->channels_num && !(sequence->tracks+i)->lock)
      sequence_get_buffers (sequence, i, nframes);
}

/**
 * Copies sample data to output buffers.
 *
 * Copies nframes frames from a given track's sample to the corresponding stream
 * audio buffers, and increases the internal track pointer (sample_input_pos)
 * accordingly. If there's not enough sample data, this function will zero-fill
 * the remaining space in the stream buffers
 */
static int
sequence_copy_sample_data (sequence_t * sequence, int track,
                           unsigned long nframes, char mask,
                           long unsigned int offset_next,
                           float *max_level)
{
  int j, k;
  //char ret = 1;
  SRC_DATA src_data;
  int src_error;
  float *filtered_data;
  unsigned long nframes_avail;
  unsigned long nframes_used;
  unsigned long nframes_filtered;
  unsigned long nframes_required;

  sequence_track_t *t = sequence->tracks + track;

  // What we have to produce and what is available
  nframes_required = nframes;
  nframes_avail = (*(t->sample_input_pos) < t->sample->frames)
    ? t->sample->frames - *(t->sample_input_pos) : 0;

  // Performing sample rate conversion
  if (t->sr_converter_type == SEQUENCE_SINC)
   {
    if (*(t->sample_input_pos) == 0) src_reset (t->sr_converter);
    src_data.data_in = t->sample->data + *(t->sample_input_pos) * t->channels_num;
    filtered_data = src_data.data_out = t->sr_converter_buffer;
    src_data.input_frames = nframes_avail;
    src_data.output_frames = nframes_required;
    src_data.src_ratio = t->sr_converter_ratio;
    src_data.end_of_input = nframes_avail ? 0 : 1;
    src_error = src_process (t->sr_converter, &src_data);
    if (src_error)
     {
      DEBUG ("FATAL: Failed to convert the sample rate : %s", 
             src_strerror (src_error));
      exit (1);
     }
    nframes_filtered = src_data.output_frames_gen;
    nframes_used = src_data.input_frames_used;
   }
  else
   {
    for (j=0, k=0; (j < nframes_required) && (k < nframes_avail); j++)
     {
      memcpy (t->sr_converter_buffer + j * t->channels_num,
              t->sample->data + (*(t->sample_input_pos) + k) * t->channels_num,
              t->channels_num * sizeof (float));
      k = (double) j / t->sr_converter_ratio;
     }
    nframes_filtered = j;
    nframes_used = k;
    filtered_data = t->sr_converter_buffer;
   }

  double mask_env_interval = ((double) sequence->framerate * ((double) SEQUENCE_MASK_ATTACK_DELAY / 1000));
  double mask_env_delta = (double) 1 / mask_env_interval;
  double mask_env = t->mask_envelope;  
 
  // Filling stream buffers
  float level;
  for (j = 0; j < t->channels_num; j++)
    {
      mask_env = t->mask_envelope;
      for (k = 0 ; k < nframes_filtered ; k++)
       {
        if (mask && ((offset_next == 0) || (offset_next - k >
                                            mask_env_interval)))
         {
          if ((*(t->sample_input_pos) == 0)) mask_env = 1;
          else if (mask_env < 1)
           {
            mask_env += mask_env_delta;
            if (mask_env > 1) mask_env = 1;
           }
         }
        else
         {
          if ((*(t->sample_input_pos) == 0)) mask_env = 0;
          else if (mask_env > 0) 
           {
            mask_env -= mask_env_delta;
            if (mask_env < 0) mask_env = 0;
           }
         }
         
        t->buffers[j][k + t->buffers_ofs] = filtered_data[k * t->channels_num + j] * t->volume * mask_env;
        level = fabsf (filtered_data[k * t->channels_num + j] * mask_env);
        if (level > (*max_level)) (*max_level) = level;
       }
      for (k = nframes_filtered; k < nframes_required; k++) t->buffers[j][k + t->buffers_ofs] = 0;

    }

  t->buffers_ofs += nframes_required; 
  t->mask_envelope = mask_env;

  // Increasing this track's frame counter
  *(t->sample_input_pos) += nframes_used;
  *(t->sample_output_pos) += nframes_filtered / t->sr_converter_ratio;

  // Returning the number of frames in the output buffers that really got
  // filled with sample data (versus: zero-filled)
  return nframes_filtered;
}

/** 
 * zero-fills stream buffers with a length of n frames, for a given track. 
 */
static void
sequence_zero_fill (sequence_t * sequence, int track, unsigned long nframes)
{
  int j, k;
  sequence_track_t *t = sequence->tracks + track;

  for (j = 0; j < t->channels_num; j++)
    for (k = 0; k < nframes; k++) t->buffers[j][k] = 0;

  t->buffers_ofs += nframes; 
}

static void
sequence_msg_event_fire_pos (sequence_t *sequence, char *event_name, int beat, int track)
{
  sequence_position_t pos;
  pos.beat = beat;
  pos.track = track;
  msg_event_fire (sequence->msg, event_name, &pos, sizeof (sequence_position_t), free);
}

float
sequence_limit_volume (float volume) 
{
    float min = SEQUENCE_DB2GAIN (SEQUENCE_DBMIN);
    float max = SEQUENCE_DB2GAIN (SEQUENCE_DBMAX);
    return volume > max ? max : (volume < min ? min : volume);
}

/**
 * Receive IPC messages.
 */
static void
sequence_receive_messages (sequence_t * sequence)
{
  sequence_msg_t msg;
  sequence_track_t *t;
  int i,j;
  sample_t *samq;
  float f;
  int *tl, st;
  char *mask;

  while (msg_receive (sequence->msg, &msg))
  {
    switch (msg.type)
    {
      case SEQUENCE_MSG_RESIZE:
        sscanf (msg.text, "tracks=%p tracks_num=%d beats_num=%d measure_len=%d",
                &(sequence->tracks), 
                &(sequence->tracks_num),
                &(sequence->beats_num),
                &(sequence->measure_len));
        break;
      case SEQUENCE_MSG_LOCK_TRACKS:
        sscanf (msg.text, "tracks=%p", &tl);
        while (*tl != -1)
          sequence->tracks[*(tl++)].lock = 1;
        DEBUG ("Tracks locks data received and stored");
        break;
      case SEQUENCE_MSG_UNLOCK_TRACKS:
        sscanf (msg.text, "tracks=%p", &tl);
        while (*tl != -1)
          sequence->tracks[*(tl++)].lock = 0;
        break;
      case SEQUENCE_MSG_LOCK_SINGLE_TRACK:
        sscanf (msg.text, "track=%d", &st);
        sequence->tracks[st].lock = 1;
        break;
      case SEQUENCE_MSG_UNLOCK_SINGLE_TRACK:
        sscanf (msg.text, "track=%d", &st);
        sequence->tracks[st].lock = 0;
        break;
      case SEQUENCE_MSG_SET_SAMPLE:
        sscanf (msg.text, "track=%d sample=%p", &i, &samq); 
        t = sequence->tracks + i;
        t->sample = samq;
        *(t->sample_input_pos) = t->sample->frames;
        *(t->sample_output_pos) = t->sample->frames;
        t->lock = 0;
        break;
      case SEQUENCE_MSG_MUTE_TRACK:
        sscanf (msg.text, "track=%d mute=%d", &i, &j);
        if (i >= 0 && i < sequence->tracks_num) {
          sequence->tracks[i].enabled = !j;
          sequence_msg_event_fire_pos (sequence, "track-mute-changed", 0, i);
        }
        break;
      case SEQUENCE_MSG_SOLO_TRACK:
        sscanf (msg.text, "track=%d solo=%d", &i, &j);
        if (i >= 0 && i < sequence->tracks_num) {
          sequence->tracks[i].solo = j;
          sequence_msg_event_fire_pos (sequence, "track-solo-changed", 0, i);
        }
        break;
      case SEQUENCE_MSG_ENABLE_MASK:
        sscanf (msg.text, "mask=%p track=%d", &mask, &i);
        sequence->tracks[i].mask = mask;
        break;
      case SEQUENCE_MSG_DISABLE_MASK:
        sscanf (msg.text, "track=%d", &i);
        sequence->tracks[i].mask = NULL;
        break;
      case SEQUENCE_MSG_ACK_STOP:
        sequence->status = SEQUENCE_DISABLED;
        // Not stopping stream
        break;
      case SEQUENCE_MSG_ACK_ENABLE:
        sequence->status = SEQUENCE_ENABLED;
        break;
      case SEQUENCE_MSG_ACK_DISABLE:
        sequence->status = SEQUENCE_DISABLED;
        break;
      case SEQUENCE_MSG_SET_BPM:
        sscanf (msg.text, "bpm=%f", &f);
        if ((f > 0) && (f <= 1000)) 
        {
          DEBUG ("Setting bpm to %f", f);
          sequence->bpm = f;
          msg_event_fire (sequence->msg, "bpm-changed", NULL, 0, NULL);
        }
        break;
      case SEQUENCE_MSG_SET_TRANSPORT:
        sscanf (msg.text, "aware=%d query=%d",
                &(sequence->transport_aware),
                &(sequence->transport_query));
        msg_event_fire (sequence->msg, "transport-changed", NULL, 0, NULL);
        break;
      case SEQUENCE_MSG_SET_LOOPING:
        if (!sequence->looping)
        {
          sequence->looping = 1;
          msg_event_fire (sequence->msg, "looping-changed", NULL, 0, NULL);
        }          
        break;
      case SEQUENCE_MSG_UNSET_LOOPING:
        if (sequence->looping)
        {
          sequence->looping = 0;
          msg_event_fire (sequence->msg, "looping-changed", NULL, 0, NULL);
        }          
        break;
      case SEQUENCE_MSG_START:
        sequence->status = SEQUENCE_ENABLED;
        if (sequence->transport_query)
          stream_start (sequence->stream);
        break;
      case SEQUENCE_MSG_STOP:
        sequence->status = SEQUENCE_DISABLED;
        if (sequence->transport_query)
          stream_stop (sequence->stream);
        break;
      case SEQUENCE_MSG_REWIND:
        if (sequence->transport_query)
          stream_seek (sequence->stream, 0);
        break;
      case SEQUENCE_MSG_SET_SR_RATIO:
        sscanf (msg.text, "track=%d ratio=%f", &i, &f);
        t = sequence->tracks + i;
        t->sr_converter_ratio = f;
        sequence_msg_event_fire_pos (sequence, "track-pitch-changed", 0, i);
        break;
      case SEQUENCE_MSG_SET_VOLUME:
        sscanf (msg.text, "track=%d volume=%f", &i, &f);
        sequence->tracks[i].volume = sequence_limit_volume (f);
        sequence_msg_event_fire_pos (sequence, "track-volume-changed", 0, i);
        break;
      case SEQUENCE_MSG_MUL_VOLUME:
        sscanf (msg.text, "track=%d ratio=%f", &i, &f);
        f = sequence->tracks[i].volume * f;
        sequence->tracks[i].volume = sequence_limit_volume (f);
        sequence_msg_event_fire_pos (sequence, "track-volume-changed", 0, i);
        break;
      case SEQUENCE_MSG_SET_SMOOTHING:
        sscanf (msg.text, "track=%d status=%d", &i, &j);
        sequence->tracks[i].smoothing = j;
        break;
      case SEQUENCE_MSG_SWAP_TRACKS:
        sscanf (msg.text, "track1=%d track2=%d", &i, &j);
        sequence_track_t tmp = sequence->tracks[i];
        sequence->tracks[i]  = sequence->tracks[j];
        sequence->tracks[j]  = tmp;
        msg_event_fire (sequence->msg, "reordered", NULL, 0, NULL);
        break;
    }
  }

  msg_sync(sequence->msg);
}

/**
 * Compute the duration (number of frames) of a beat. 
 */
static unsigned long
sequence_get_beat_nframes (sequence_t *sequence)
{
  return 60 * sequence->framerate / sequence->bpm / sequence->measure_len;
}

/**
 * Tell whether a track is unmuted and/or solo if in solo mode
 */
static char
sequence_track_is_playing(sequence_t *sequence, int track)
{
    int i;
    for (i = 0; i < sequence->tracks_num; i++) {
        if (sequence->tracks[i].solo)
            break;
    }
    if (i < sequence->tracks_num) {
        return sequence->tracks[track].solo;
    } else {
        return sequence->tracks[track].enabled;
    }
}

/**
 * Perform sequencing.
 */
static unsigned long
sequence_do_process (sequence_t *sequence, unsigned long position, unsigned long nframes)
{
  int i;
  int current_beat;
  int previous_beat;
  int current_offset;
  unsigned long beat_nframes; // should be a double ?
  unsigned long footer = 0;
  int beat_trigger = 0;
 
  beat_nframes = sequence_get_beat_nframes (sequence);

  current_beat = position / beat_nframes;
  current_offset = position % beat_nframes;

  if (sequence->looping) 
   {
    if (current_offset == 0) beat_trigger = 1;
    else if (current_offset + nframes > beat_nframes)
      {
        current_beat++;
        footer = beat_nframes - current_offset;
        current_offset = 0;
        beat_trigger = 1;
      }
    
    if (current_beat >= sequence->beats_num)
      current_beat = current_beat % sequence->beats_num;
   }
  else if (current_beat < sequence->beats_num && current_offset == 0)
   {
      beat_trigger = 1;
   }
  else if (current_offset + nframes > beat_nframes && current_beat <
           (sequence->beats_num - 1))
    {
      current_beat++;
      footer = beat_nframes - current_offset;
      current_offset = 0;
      beat_trigger = 1;
    }

  sequence_track_t *t;
  char mask;
  unsigned long nframes_played = 0;
  unsigned long nframes_copied, n;
  for (i = 0; i < sequence->tracks_num; i++)
    {
      t = sequence->tracks + i;
      if (!t->lock)
        {
          float current_level = 0;
          char playing        = sequence_track_is_playing(sequence, i);
          if (t->sample != NULL)
            {
              t->buffers_ofs = 0;
              nframes_copied = 0;
              if (beat_trigger)
                {
                  previous_beat = (current_beat > 0) 
                   ? current_beat - 1 : sequence->beats_num - 1;
                  mask = (t->mask) ? t->mask[previous_beat] : 1;
                  nframes_copied = sequence_copy_sample_data (sequence, i, footer, 
                                    mask & playing, footer, &current_level); 

                  if ((!nframes_copied) && (t->active_beat != -1)) 
                  {
                    if (playing)
                      sequence_msg_event_fire_pos (sequence, "beat-off", t->active_beat, i);

                    t->active_beat = -1;
                  }

                  if (t->beats[current_beat])
                    {
                      if (playing)
                        {
                          if (t->active_beat != -1)
                          {
                            sequence_msg_event_fire_pos (sequence, "beat-off", t->active_beat, i);
                          }                            

                          sequence_msg_event_fire_pos (sequence, "beat-on", current_beat, i);
                        }
                      *(t->sample_input_pos) = 0;
                      *(t->sample_output_pos) = 0;
                      t->active_beat = current_beat;
                    }
                }
             
              // Looking up next active beat
              int dist;
              if ((!t->smoothing) || t->active_beat == -1) dist = -1;
              else
               {
                for (dist = 1; 
                     (current_beat + dist < sequence->beats_num) 
                      && (!t->beats[current_beat + dist]);
                     dist++)
                  ;
                if (current_beat + dist == sequence->beats_num)
                 {
                  if (!sequence->looping) dist = -1;
                  else
                   {
                    for ( ; (dist <= sequence->beats_num) 
                              && (!t->beats[current_beat + dist - sequence->beats_num]);
                          dist++)
                      ;
                    if (!t->beats[current_beat + dist - sequence->beats_num])
                      dist = -1;
                   }
                 }
               }
              
              long unsigned int offset_next = 0;
              if (dist != -1)
               {
                offset_next = beat_nframes - current_offset + (dist - 1) * beat_nframes; 
               }
                   
              if ((t->active_beat != -1) && (!t->beats[t->active_beat])) 
               {
                *(t->sample_input_pos) = t->sample->frames;
                *(t->sample_output_pos) = t->sample->frames;
                // FIXME: What if there is no sr_converter because we're using the 
                // SEQUENCE_LINEAR one ?
                if (t->sr_converter != NULL) src_reset (t->sr_converter);
               }
              
              mask = ((current_beat < sequence->beats_num) && (t->mask != NULL)) 
                      ? t->mask[current_beat] : 1;

              n = sequence_copy_sample_data (sequence, i, nframes - footer, 
                    mask & playing, offset_next, &current_level);
              
              if ((!n) && (t->active_beat != -1)) 
              {
                if (playing)
                  sequence_msg_event_fire_pos (sequence, "beat-off", t->active_beat, i);
                t->active_beat = -1;
              }

              nframes_copied += n;
              if (nframes_copied > nframes_played) nframes_played = nframes_copied;

              t->active_mask_beat = ((t->active_beat != -1) && t->mask && mask
                                     && (current_beat < sequence->beats_num)) ? current_beat : -1;
            }
          else
           {
            if (beat_trigger && playing)
              {
                if (t->active_beat != -1) 
                  {
                    sequence_msg_event_fire_pos (sequence, "beat-off", t->active_beat, i);
                  }

                if (t->beats[current_beat] && playing)
                  {
                    t->active_beat = current_beat;
                    current_level = 1;
                    sequence_msg_event_fire_pos (sequence, "beat-on", current_beat, i);
                  }
                else
                  {
                    t->active_beat = -1;
                    current_level = 0;
                  }
              }

            sequence_zero_fill (sequence, i, nframes);

           }
          t->current_level = current_level; 
        }
    }

  return nframes_played;
}

/**
 * Process a given number of frames
 *
 * This is the stream callback function.
 */
static int
sequence_process (unsigned long nframes, void *data)
{
  int i;
  sequence_t *sequence = (sequence_t *) data;
  sequence_receive_messages (sequence);

  if (!sequence->tracks_num) return 0;
  
  sequence_get_all_buffers (sequence, nframes);

  if (stream_is_started (sequence->stream) && (sequence->status == SEQUENCE_ENABLED))
   {
    sequence_do_process (sequence, stream_get_position (sequence->stream), nframes);
   }
  else
   {
     for (i=0; i < sequence->tracks_num; i++)
       if ((sequence->tracks+i)->channels_num && !(sequence->tracks+i)->lock) 
        {
          sequence_zero_fill (sequence, i, nframes);
        }
   }
  return 0;
}

/******************************************************
 *   Private functions running inside the main loop   *
 *   (these are called by the public functions)       *
 ******************************************************/

static void
sequence_init (sequence_t * sequence)
{
  sequence->tracks = NULL;
  sequence->tracks_num = 0;
  sequence->beats_num = 0;
  sequence->measure_len = 0;
  sequence->bpm = 100;
  sequence->status = SEQUENCE_DISABLED;
  sequence->looping = 1;
  sequence->transport_aware = 1;
  sequence->transport_query = 1;
  sequence->framerate = 0;
  sequence->name[0] = '\0';
  sequence->msg = NULL;
  sequence->sr_converter_default_type = SEQUENCE_LINEAR;
  sequence->error = 0;

  event_register (sequence, "destroy");
  event_register (sequence, "transport-changed");
  event_register (sequence, "bpm-changed");
  event_register (sequence, "looping-changed");
  event_register (sequence, "track-mute-changed");
  event_register (sequence, "track-solo-changed");
  event_register (sequence, "beat-changed");
  event_register (sequence, "beat-on");
  event_register (sequence, "beat-off");
  event_register (sequence, "track-pitch-changed");
  event_register (sequence, "track-volume-changed");
  event_register (sequence, "track-name-changed");
  event_register (sequence, "resized");
  event_register (sequence, "reordered");
  event_register (sequence, "resampler-type-changed");
}

static void
sequence_track_init (sequence_track_t *track)
{
  track->beats                = NULL;
  track->mask                 = NULL;
  track->sample               = NULL;
  track->channels             = NULL;
  track->channels_num         = 1;
  track->sample_input_pos     = NULL;
  track->sample_output_pos    = NULL;
  track->buffers              = NULL;
  track->active_beat          = -1;
  track->active_mask_beat     = -1;
  track->current_level        = 0;
  track->name[0]              = '\0';
  track->enabled              = 1;
  track->solo                 = 0;
  track->lock                 = 0;
  track->sr_converter         = NULL;
  track->sr_converter_type    = -1;
  track->sr_converter_buffer  = NULL;
  track->sr_converter_ratio   = 1;
  track->pitch                = 0;
  track->volume               = 1;
  track->mask_envelope        = 1;
  track->smoothing            = 1;
  track->level_peak           = 1;
}

static void
sequence_destroy_track (sequence_t *sequence, int track, int destroy_beats)
{
  sequence_track_t *t = sequence->tracks + track;

  int j;
  for (j = 0; j < t->channels_num; j++)
    stream_port_remove (sequence->stream, t->channels[j]);

  free (t->sample_input_pos);
  free (t->sample_output_pos);
  free (t->channels);
  free (t->buffers);
  if (t->sr_converter != NULL)
   {
    src_delete (t->sr_converter);
    t->sr_converter = NULL;
    free (t->sr_converter_buffer);
    t->sr_converter_buffer = NULL;
   }

  if (destroy_beats) 
   {
      free (t->beats);
      if (t->mask) free (t->mask);
   }

  if (t->sample != NULL)
    sample_unref(t->sample);
}

static char * 
sequence_make_port_name (sequence_t *sequence, sequence_track_t *track, int channel)
{
  char *s = malloc (strlen (sequence->name) + strlen (track->name) + 6);

  switch (track->channels_num)
   {
    case 1: 
      sprintf (s, "%s_%s", sequence->name, track->name);
      break;
    case 2: 
      if (channel == 0) sprintf (s, "%s_%s_L", sequence->name, track->name);
      else sprintf (s, "%s_%s_R", sequence->name, track->name);
      break;
    default: 
      sprintf (s, "%s_%s_%d", sequence->name, track->name, channel + 1); 
      break;
   }

  return s;
}

static int 
sequence_register_track (sequence_t *sequence, sequence_track_t *track)
{
  int j, flags;
  sequence->error = 0;

  for (j = 0; j < track->channels_num; j++) {
    char *name = sequence_make_port_name (sequence, track, j);
    flags = STREAM_OUTPUT;
    if (j == 0)
      flags |= STREAM_LEFT;
    if ((j == 1) || (track->channels_num == 1))      
      flags |= STREAM_RIGHT;
      
    track->channels[j] = stream_port_add (sequence->stream, name, flags);
    free(name);
    if (!track->channels[j]) {
      DEBUG("Failed to register stream port");
      // Rolling back
      for (j--; j >= 0; j--) 
        stream_port_remove(sequence->stream, track->channels[j]);

      sequence->error = ERR_SEQUENCE_JACK_REGPORT; // FIXME: s/jack/stream/
      break;
    }
  }
  return sequence->error ? 0 : 1;
}

static void
sequence_stop_ack (sequence_t * sequence)
{
  sequence_msg_t msg;
  msg.type = SEQUENCE_MSG_ACK_STOP;
  msg.text[0] = '\0';
  msg_send (sequence->msg, &msg, MSG_ACK);
}

static long int
sequence_get_sustain_nframes (sequence_t *sequence)
{
  int i, j;
  int beat_nframes = sequence_get_beat_nframes (sequence);
  long int sustain = 0, track_sustain;

  for (i = 0; i < sequence->tracks_num; i++)
   {
    sequence_track_t *track = sequence->tracks + i;
    if (track->sample) 
     {
      long int sample_nframes = track->sample->frames * track->sr_converter_ratio;
      for (j = sequence->beats_num; (j >= 0) && (!track->beats[j]) ; j--)
        ;
      if (j != -1)
       {
        track_sustain = sample_nframes - (sequence->beats_num - j) * beat_nframes;
        if (track_sustain > sustain)
          sustain = track_sustain;
       }
     }
   }
   return sustain;
}

void static 
sequence_export_mix (sequence_t *sequence, float *output, int nframes)
{
  int i, j, k;

  for (k = 0; k < nframes * 2; k++) 
    output[k] = 0;

  for (i = 0; i < sequence->tracks_num; i++) 
   {
    sequence_track_t *track = sequence->tracks + i;

    for (j = 0; j < track->channels_num; j++)
      for (k = 0; k < nframes; k++)
        output[k * 2 + (j % 2)] += track->buffers[j][k];

    for ( ; j < 2; j++)
      for (k = 0; k < nframes; k++)
        output[k * 2 + j] += track->buffers[j % track->channels_num][k];
   }
}

static void 
sequence_lock (sequence_t *sequence)
{
  sem_wait(&sequence->mutex);
}

static void 
sequence_unlock (sequence_t *sequence)
{
  sem_post(&sequence->mutex);
}

static int
_sequence_track_name_exists (sequence_t *sequence, char *name)
{
  int i;
  for (i=0; i < sequence->tracks_num; i++)
   {
    if (!strcmp ((sequence->tracks + i)->name, name))
      return 1;
   }
  return 0;
}

#define sequence_check_pos(S,T,B) _sequence_check_pos(S, T, B, __func__)
static int
_sequence_check_pos (sequence_t *sequence, int track, int beat, const char *caller)
{
  int result = 1;

  if (track < 0 || track >= sequence->tracks_num) {
    FDEBUG(caller, "Warning: track %d doesn't exist (tracks_num = %d)", track, sequence->tracks_num);
    result = 0;
  }

  if (beat < 0 || beat >= sequence->beats_num) {
    FDEBUG(caller, "Warning: beat %d doesn't exist (beats_num = %d)", beat, sequence->beats_num);
    result = 0;
  }

  return result;
}

static void
sequence_event_fire_pos (sequence_t *sequence, char *event_name, int beat, int track)
{
  sequence_position_t *pos = malloc (sizeof (sequence_position_t));
  pos->beat = beat;
  pos->track = track;
  event_fire (sequence, event_name, pos, free);
}

/*********************************
 *       Public functions        *
 *********************************/

sequence_t *
sequence_new (stream_t *stream,char *name, int *error) 
{
  sequence_t *sequence;

  sequence = calloc (1, sizeof (sequence_t));
  sequence_init (sequence);
  sequence->stream = stream;
  sequence->pool = NULL;
  DEBUG ("name : %s", name);
  strcpy (sequence->name, name);

  sequence->msg = msg_new (4096, sizeof(sequence_msg_t));
  sem_init(&sequence->mutex, 0, 1);

  if (stream_add_process (sequence->stream, sequence->name, sequence_process, 
                          (void *) sequence))
  {
    sequence->buffer_size = stream_get_buffer_size (sequence->stream);
    //FIXME: new stream layer might cause arbitrary sample rate changes, 
    //shouldn't be cached:
    sequence->framerate = stream_get_sample_rate (sequence->stream);
    DEBUG ("Stream framerate : %ld", sequence->framerate);
  }
  else
  {
    DEBUG("Can't add stream process");
    sequence_destroy (sequence);
    *error = ERR_SEQUENCE_JACK_CONNECT; // FIXME s/jack connect/stream add process/
    return NULL;
  }
 
  DEBUG("sizeof(float): %d", sizeof(float));

  return sequence;
}

void          
sequence_activate (sequence_t *sequence, pool_t *pool)
{
  sequence->pool = pool;
  pool_add_process (pool, sequence_process_events, (void *) sequence);
}

int
sequence_get_error (sequence_t * sequence)
{
  SEQUENCE_SAFE_GETTER (int, sequence->error);
}

void
sequence_rewind (sequence_t * sequence)
{
  sequence_msg_t msg;
  msg.type = SEQUENCE_MSG_REWIND;
  msg.text[0] = '\0';
  SEQUENCE_LOCK_CALL (msg_send (sequence->msg, &msg, 0));
}

void
sequence_start (sequence_t * sequence)
{
  sequence_msg_t msg;
  msg.type = SEQUENCE_MSG_START;
  msg.text[0] = '\0';
  SEQUENCE_LOCK_CALL (msg_send (sequence->msg, &msg, 0));
}

void
sequence_stop (sequence_t * sequence)
{
  sequence_msg_t msg;
  msg.type = SEQUENCE_MSG_STOP;
  msg.text[0] = '\0';
  SEQUENCE_LOCK_CALL (msg_send (sequence->msg, &msg, 0));
}

void
sequence_destroy (sequence_t * sequence)
{
  int i;
  event_fire (sequence, "destroy", NULL, NULL);
  event_remove_source (sequence);
  if (sequence->pool)
    pool_remove_process (sequence->pool, sequence_process_events, (void *) sequence);
  sem_wait (&sequence->mutex);
  sem_destroy (&sequence->mutex);
  sequence_stop_ack (sequence);
  stream_remove_process (sequence->stream, sequence->name);
  // FIXME: May need to sync in here
  msg_destroy (sequence->msg);
  for (i=0; i < sequence->tracks_num; i++)
    sequence_destroy_track (sequence, i, 1);
  if (sequence->tracks != NULL) 
    free (sequence->tracks);
  free (sequence);
}

int
sequence_resize (sequence_t * sequence, int tracks_num, int beats_num,
                 int measure_len, int duplicate_beats)
{
  int i, j;
  sequence_msg_t msg;
  
  sequence_lock (sequence);

  if ((sequence->tracks_num == tracks_num) && (sequence->beats_num == beats_num)
      && (sequence->measure_len == measure_len))
  {
    sequence_unlock (sequence);
    return 1;
  }

  sequence->error = 0;
  
  /* Asks the audio thread to avoid playing (lock) tracks whose stream ports are
     about to be unregistered */
  if (tracks_num < sequence->tracks_num)
    {
      int *tl = calloc (sequence->tracks_num - tracks_num + 1, sizeof (int));
      for (i = tracks_num, j = 0; i < sequence->tracks_num; i++)
        tl[j++] = i;
      tl[j] = -1;
      DEBUG("Requiring tracks locks");
      msg.type = SEQUENCE_MSG_LOCK_TRACKS;
      sprintf (msg.text, "tracks=%p", tl);
      msg_send (sequence->msg, &msg, MSG_ACK);
      DEBUG("Tracks locks ack'ed. Freeing temporary data.");
      free (tl);
    }

  /* Duplicates all tracks data so that we don't interfer with the audio thread */
  sequence_track_t *new_tracks = calloc (tracks_num, sizeof
                                         (sequence_track_t));
  int tn = (sequence->tracks_num > tracks_num) ? tracks_num :
    sequence->tracks_num;
  memcpy (new_tracks, sequence->tracks, tn * sizeof (sequence_track_t));

  /* Unregister stream ports and wipes associated tracks data if tracks_num decreases. */
  sequence_track_t *t;
  for (i = tracks_num; i < sequence->tracks_num; i++)
    sequence_destroy_track (sequence, i, 0);

  /* Register stream ports and allocate new tracks data if tracks_num increases */
  for (i = sequence->tracks_num; i < tracks_num; i++)
    {
      DEBUG("Initializing track %d",i);
      t = new_tracks + i;
      sequence_track_init (t);
      t->channels = calloc (t->channels_num, sizeof (stream_port_t *));
      t->buffers = calloc (t->channels_num, sizeof (float *));
      t->sample_input_pos = malloc (sizeof (unsigned long));
      *(t->sample_input_pos) = 0;
      t->sample_output_pos = malloc (sizeof (unsigned long));
      *(t->sample_output_pos) = 0;

      j = i;
      do sprintf (t->name, "track%d", ++j);
      while (_sequence_track_name_exists (sequence, t->name));

      if (!sequence_register_track (sequence, t)) 
      {
        DEBUG("Couldn't not register track, stopping resize");
        free(t->channels);
        free(t->buffers);
        free(t->sample_input_pos);
        free(t->sample_output_pos);
        if ((tracks_num = i) > 0) {
          new_tracks = realloc (new_tracks, tracks_num * sizeof(sequence_track_t));
        } else {
          free (new_tracks);
          new_tracks = NULL;
        }
        break;
      }
    }

  /* Resize beats memory */
  for (i = 0; i < tracks_num; i++)
    {
      (new_tracks + i)->beats = calloc (beats_num, 1);
      (new_tracks + i)->mask = calloc (beats_num, 1);
      memset ((new_tracks + i)->mask, 1, beats_num);
      if (i < sequence->tracks_num)
       {
        if (sequence->beats_num > beats_num)
         {
          memcpy ((new_tracks + i)->beats, (sequence->tracks + i)->beats, beats_num);
          memcpy ((new_tracks + i)->mask, (sequence->tracks + i)->mask, beats_num);
         }
        else
         {
          if (duplicate_beats)
           {
            int bn;
            for (j=0; j < beats_num; j += sequence->beats_num)
             {
              bn = (beats_num - j) < sequence->beats_num 
                 ? beats_num - j
                 : sequence->beats_num;
              memcpy ((new_tracks + i)->beats + j, 
                      (sequence->tracks + i)->beats, bn);
                memcpy ((new_tracks + i)->mask + j, (sequence->tracks + i)->mask, bn);
             }
           }
          else
            memcpy ((new_tracks + i)->beats, (sequence->tracks + i)->beats, 
                    sequence->beats_num);
            memcpy ((new_tracks + i)->mask, (sequence->tracks + i)->mask, 
                    sequence->beats_num);
         }
       }
    }

  /* Packs all that and sends it to the audio thread */
  sprintf (msg.text, "tracks=%p tracks_num=%d beats_num=%d measure_len=%d",
            new_tracks, tracks_num, beats_num, measure_len);

  DEBUG("Sending resize message : %s", msg.text);
  sequence_track_t *old_tracks = sequence->tracks;
  int old_tracks_num = sequence->tracks_num;

  msg.type = SEQUENCE_MSG_RESIZE;
  msg_send (sequence->msg, &msg, MSG_ACK);

  /* Cleans up old data */
  if (old_tracks)
    {
      DEBUG("Cleaning up old tracks data at %p", old_tracks);
      for (i = 0; i < old_tracks_num; i++)
       {
        free ((old_tracks + i)->beats);
        if (old_tracks[i].mask) free ((old_tracks + i)->mask);
       }
      free (old_tracks);
    }
 
  int success = sequence->error ? 0 : 1;// sequence->error might get set by sequence_register_track()
  sequence_unlock (sequence); 
  if (success)
    event_fire (sequence, "resized", NULL, NULL);
  return success;
}

void
sequence_remove_track (sequence_t *sequence, int track)
{
  sequence_msg_t msg;
  int resized = 0;

  sequence_lock (sequence);

  if (sequence_check_pos (sequence, track, 0)) {
    msg.type = SEQUENCE_MSG_LOCK_SINGLE_TRACK;
    sprintf (msg.text, "track=%d", track);
    msg_send (sequence->msg, &msg, MSG_ACK);

    sequence_destroy_track (sequence, track, 1);
    
    sequence_track_t *new_tracks = calloc (sequence->tracks_num - 1, sizeof (sequence_track_t));
    memcpy (new_tracks, sequence->tracks, track * sizeof (sequence_track_t));
    memcpy (new_tracks + track, sequence->tracks + track + 1, 
            (sequence->tracks_num - track - 1) * sizeof (sequence_track_t));

    sequence_track_t *old_tracks = sequence->tracks;

    msg.type = SEQUENCE_MSG_RESIZE;
    sprintf (msg.text, "tracks=%p tracks_num=%d beats_num=%d measure_len=%d",
              new_tracks, sequence->tracks_num - 1, sequence->beats_num, sequence->measure_len);
    msg_send (sequence->msg, &msg, MSG_ACK);

    free (old_tracks);
    resized = 1;
  }    

  sequence_unlock (sequence);
  if (resized)
    event_fire (sequence, "resized", NULL, NULL);
}

char *
sequence_get_name (sequence_t * sequence)
{
  sequence_lock (sequence);
  char *name = malloc (strlen (sequence->name) + 1);
  strcpy (name, sequence->name);
  sequence_unlock (sequence);
  return name;
}

void
sequence_set_transport (sequence_t * sequence, int respond, int query)
{
  sequence_msg_t msg;
  msg.type = SEQUENCE_MSG_SET_TRANSPORT;
  sprintf (msg.text, "aware=%d query=%d", respond, query); 
  SEQUENCE_LOCK_CALL (msg_send (sequence->msg, &msg, 0));
}

void
sequence_get_transport (sequence_t * sequence, int *respond, int *query)
{
  sequence_lock (sequence);
  *respond = sequence->transport_aware;
  *query = sequence->transport_query;
  sequence_unlock (sequence);
}

int
sequence_is_playing (sequence_t * sequence)
{
  SEQUENCE_SAFE_GETTER (int, 
    (sequence->status == SEQUENCE_ENABLED 
     && (!sequence->transport_aware || stream_is_started (sequence->stream))));
}

int
sequence_get_active_beat (sequence_t * sequence, int track)
{
  SEQUENCE_SAFE_GETTER (int, sequence_check_pos (sequence, track, 0) 
                             ? sequence->tracks[track].active_beat : -1);
}

float
sequence_get_level (sequence_t * sequence, int track)
{
  SEQUENCE_SAFE_GETTER (float, sequence_check_pos (sequence, track, 0) 
                               ?  sequence->tracks[track].current_level 
                                  / sequence->tracks[track].level_peak
                               : 0);
}

int
sequence_get_active_mask_beat (sequence_t * sequence, int track)
{
  SEQUENCE_SAFE_GETTER (int, sequence_check_pos (sequence, track, 0) 
                             ? sequence->tracks[track].active_mask_beat : -1);
}

void
sequence_unset_looping (sequence_t * sequence)
{
  sequence_msg_t msg;
  msg.type = SEQUENCE_MSG_UNSET_LOOPING;
  msg.text[0] = '\0';
  SEQUENCE_LOCK_CALL (msg_send (sequence->msg, &msg, 0));
}

int
sequence_is_looping (sequence_t * sequence)
{
  SEQUENCE_SAFE_GETTER (int, sequence->looping);
}

void
sequence_set_looping (sequence_t * sequence)
{
  sequence_msg_t msg;
  msg.type = SEQUENCE_MSG_SET_LOOPING;
  msg.text[0] = '\0';
  SEQUENCE_LOCK_CALL (msg_send (sequence->msg, &msg, 0));
}

float
sequence_get_bpm (sequence_t * sequence)
{
  SEQUENCE_SAFE_GETTER (float, sequence->bpm);
}

void
sequence_set_bpm (sequence_t * sequence, float bpm)
{
  DEBUG("Setting bpm to : %f", bpm);
  sequence_msg_t msg;
  msg.type = SEQUENCE_MSG_SET_BPM;
  sprintf (msg.text, "bpm=%f", bpm);
  SEQUENCE_LOCK_CALL (msg_send (sequence->msg, &msg, 0));
}

void
sequence_set_beat (sequence_t * sequence, int track, int beat, char status)
{
  sequence_lock (sequence);
  int changed = 0;
  if (sequence_check_pos (sequence, track, beat))
  {
    //DEBUG ("sequence: %p, track: %d, beat:%d, status: %d", sequence, track,
    //       beat, status);
    sequence->tracks[track].beats[beat] = status;
    changed = 1;
  }
  sequence_unlock (sequence);
  if (changed)
    sequence_event_fire_pos (sequence, "beat-changed", beat, track);
}

int
sequence_get_tracks_num (sequence_t * sequence)
{
  SEQUENCE_SAFE_GETTER (int, sequence->tracks_num);
}

int
sequence_get_beats_num (sequence_t * sequence)
{
  SEQUENCE_SAFE_GETTER (int, sequence->beats_num);
}

int
sequence_get_measure_len (sequence_t * sequence)
{
  SEQUENCE_SAFE_GETTER (int, sequence->measure_len);
}

char
sequence_get_beat (sequence_t * sequence, int track, int beat)
{
  SEQUENCE_SAFE_GETTER (char, sequence_check_pos (sequence, track, beat) 
                              ? (sequence->tracks + track)->beats[beat] : 0);
}

char *
sequence_get_track_name (sequence_t * sequence, int track)
{
  sequence_lock (sequence);
  char *name;
  if (sequence_check_pos (sequence, track, 0)) {
      char *src = (sequence->tracks + track)->name;
      name = malloc (strlen (src) + 1);
      strcpy (name, src);
  } else {
      name = NULL;
  }
  sequence_unlock (sequence);
  return name;
}

int
sequence_track_name_exists (sequence_t *sequence, char *name)
{
  SEQUENCE_SAFE_GETTER (int, _sequence_track_name_exists (sequence, name));
}

sample_t *
sequence_get_sample (sequence_t * sequence, int track)
{
  SEQUENCE_SAFE_GETTER (sample_t *, sequence_check_pos(sequence, track, 0) 
                                    ? (sequence->tracks + track)->sample : NULL);
}

int 
sequence_set_sample (sequence_t * sequence, int track, sample_t * sample)
{
  sequence_lock (sequence);
  if (!sequence_check_pos (sequence, track, 0)) {
      sequence_unlock (sequence);
      return 0;
  }
  sequence_msg_t msg;
  sequence_track_t *t = sequence->tracks + track;
  sequence_track_t old_track;
  int j, sr_converter_error;
  sequence->error = 0;
  int success = 1;

  if (t->channels_num != sample->channels_num)
   {
    msg.type = SEQUENCE_MSG_LOCK_SINGLE_TRACK;
    sprintf (msg.text, "track=%d", track);
    msg_send (sequence->msg, &msg, MSG_ACK);

    memcpy(&old_track, t, sizeof (sequence_track_t));
    t->buffers = calloc (t->channels_num, sizeof (float *));
    t->channels_num = sample->channels_num;
    t->channels = calloc (t->channels_num, sizeof (stream_port_t *));

    if (sequence_register_track (sequence, t))
     {

      if (old_track.channels_num) 
       {
        for (j = 0; j < old_track.channels_num; j++)
          stream_port_remove (sequence->stream, old_track.channels[j]);
        free (old_track.channels);
        free (old_track.buffers);
       }

      if (t->sr_converter != NULL) 
       {
        src_delete (t->sr_converter);
        t->sr_converter = NULL;
        free (t->sr_converter_buffer);
        t->sr_converter_buffer = NULL;
       }
     } 
    else 
     {
      DEBUG("Couldn't register track");
      free(t->channels);
      free(t->buffers);
      t->channels = old_track.channels;
      t->channels_num = old_track.channels_num;
      t->buffers = old_track.buffers;
      msg.type = SEQUENCE_MSG_UNLOCK_SINGLE_TRACK;
      sprintf (msg.text, "track=%d", track);
      msg_send (sequence->msg, &msg, MSG_ACK);
      success = 0;
     }
   }
 
  if (success)
  {
    if (t->sr_converter_type == -1) 
      t->sr_converter_type = sequence->sr_converter_default_type;
    
    if (t->sr_converter_type == SEQUENCE_SINC)
     {
      if (t->sr_converter == NULL) 
       {
        DEBUG("Creating SR converter for %d channels", t->channels_num);
        t->sr_converter = src_new (SRC_SINC_FASTEST, t->channels_num,
                                     &sr_converter_error);
        if (t->sr_converter == NULL)
         {
          DEBUG ("FATAL: Cannot create the sample rate converter : %s", 
                 src_strerror (sr_converter_error));
          exit(1);
         }
       }
      else 
       {
        sr_converter_error = src_reset (t->sr_converter);
        if (sr_converter_error)
          DEBUG ("Cannot reset the sample rate converter : %s", 
                 src_strerror (sr_converter_error));
       }
     }
    if (t->sr_converter_buffer == NULL)
      t->sr_converter_buffer = calloc (sequence->buffer_size * t->channels_num,
                                       sizeof (float));
    t->sr_converter_ratio = (double) sequence->framerate 
                            / (double) sample->framerate 
                            / pow (2, t->pitch / 12);
                        
    t->level_peak = sample->peak > 0 ? sample->peak : 1;

    sample_t * old_sample = t->sample;
    msg.type = SEQUENCE_MSG_SET_SAMPLE;
    sprintf (msg.text, "track=%d sample=%p", track, sample);
    msg_send (sequence->msg, &msg, MSG_ACK);
    sample_ref (sample);
    if (old_sample != NULL)
      sample_unref (old_sample);
  }

  sequence_unlock (sequence);
  return success;
}

static int
sequence_do_set_track_name (sequence_t * sequence, int track, char *name, int force)
{
  sequence_lock (sequence);
  int j, error = 0;
  if (sequence_check_pos (sequence, track, 0)) {
    sequence_track_t *t = sequence->tracks + track;
    if (strcmp (t->name, name))
    {
      if (force || !_sequence_track_name_exists (sequence, name)) {
        if (strlen(name) > 0 && strspn (name, SEQUENCE_VALID_NAME) == strlen (name)) {
          strcpy (t->name, name);
          for (j = 0; j < t->channels_num; j++)
          {
            char *name = sequence_make_port_name (sequence, t, j);
            stream_port_rename (sequence->stream, t->channels[j], name);
            free(name);
          }
          sequence_event_fire_pos (sequence, "track-name-changed", 0, track);
        } else {
          error = ERR_SEQUENCE_INVALID_NAME;
        }
      } else {
        error = ERR_SEQUENCE_DUPLICATE_TRACK_NAME;
      }
    }
  } else {
    error = ERR_SEQUENCE_INTERNAL;
  }
  sequence_unlock (sequence);
  return error;
}

int
sequence_set_track_name(sequence_t * sequence, int track, char *name) {
    return sequence_do_set_track_name(sequence, track, name, 0);
}

int
sequence_force_track_name(sequence_t * sequence, int track, char *name) {
    return sequence_do_set_track_name(sequence, track, name, 1);
}

int
sequence_get_sample_usage (sequence_t * sequence, sample_t * sample)
{
  sequence_lock (sequence);
  int i;
  int r = 0;
  for (i = 0; i < sequence->tracks_num; i++) 
   {
    if ((sequence->tracks + i)->sample) {
      DEBUG ("Track %d has sample : %s", i, (sequence->tracks +
                                           i)->sample->name);
    }
    if ((sequence->tracks + i)->sample == sample) r++;
   }
  DEBUG ("Usage for sample \"%s\" is : %d", sample->name, r);
  sequence_unlock (sequence);
  return r;
}

sequence_track_type_t
sequence_get_track_type (sequence_t * sequence, int track)
{
  sequence_lock (sequence);
  sequence_track_type_t r = EMPTY;
  if (sequence_check_pos (sequence, track, 0)) {
    sequence_track_t *t = (sequence->tracks + track);
    if (t->sample)
      r = SAMPLE;
  }
  sequence_unlock (sequence);
  return r;
}

void
sequence_mute_track (sequence_t * sequence, char status, int track)
{
  sequence_lock (sequence);
  if (sequence_check_pos (sequence, track, 0)) {
    sequence_msg_t msg;
    msg.type = SEQUENCE_MSG_MUTE_TRACK;
    sprintf (msg.text, "track=%d mute=%d", track, (status) ? 1 : 0);
    msg_send (sequence->msg, &msg, 0);
  }
  sequence_unlock (sequence);
}

char
sequence_track_is_muted (sequence_t * sequence, int track)
{
  SEQUENCE_SAFE_GETTER (char, sequence_check_pos (sequence, track, 0) 
                              ? !sequence->tracks[track].enabled : 0);
}

void
sequence_solo_track (sequence_t * sequence, char status, int track)
{
  sequence_lock (sequence);
  if (sequence_check_pos (sequence, track, 0)) {
    sequence_msg_t msg;
    msg.type = SEQUENCE_MSG_SOLO_TRACK;
    sprintf (msg.text, "track=%d solo=%d", track, (status) ? 1 : 0);
    msg_send (sequence->msg, &msg, 0);
  }
  sequence_unlock (sequence);
}

char
sequence_track_is_solo (sequence_t * sequence, int track)
{
  SEQUENCE_SAFE_GETTER (char, sequence_check_pos (sequence, track, 0) 
                              ? sequence->tracks[track].solo : 0);
}

unsigned long 
sequence_get_framerate (sequence_t *sequence)
{
  SEQUENCE_SAFE_GETTER (unsigned long, sequence->framerate);
}

void
sequence_enable_mask (sequence_t *sequence, int track)
{
  sequence_lock (sequence);
  if (sequence_check_pos (sequence, track, 0) && !sequence->tracks[track].mask)
   {
    char *mask = calloc (sequence->beats_num, 1);
    memset (mask, 1, sequence->beats_num);
  
    sequence_msg_t msg;
    msg.type = SEQUENCE_MSG_ENABLE_MASK;
    sprintf (msg.text, "mask=%p track=%d", mask, track);
    msg_send (sequence->msg, &msg, MSG_ACK);
   }
  sequence_unlock (sequence);
}

void
sequence_disable_mask (sequence_t *sequence, int track)
{
  sequence_lock (sequence);
  if (sequence_check_pos (sequence, track, 0) && sequence->tracks[track].mask)
   {
    char *old_mask = sequence->tracks[track].mask;
  
    sequence_msg_t msg;
    msg.type = SEQUENCE_MSG_DISABLE_MASK;
    sprintf (msg.text, "track=%d", track);
    msg_send (sequence->msg, &msg, MSG_ACK);
    free (old_mask);
   }
  sequence_unlock (sequence);
}


void
sequence_set_mask_beat (sequence_t * sequence, int track, int beat, char status)
{
  sequence_lock (sequence);
  int changed = 0;
  if (sequence_check_pos (sequence, track, beat))
  {
    //DEBUG ("(mask)sequence: %p, track: %d, beat:%d, status: %d", sequence, track,
    //       beat, status);
    if (sequence->tracks[track].mask)
    {
      sequence->tracks[track].mask[beat] = status;
      changed = 1;
    }
  }
  sequence_unlock (sequence);
  if (changed)
    sequence_event_fire_pos(sequence, "beat-changed", beat, track);
}

char
sequence_get_mask_beat (sequence_t * sequence, int track, int beat)
{
  SEQUENCE_SAFE_GETTER (char, sequence_check_pos (sequence, track, 0) 
                              ? sequence->tracks[track].mask[beat] : 0);
}

int
sequence_is_enabled_mask (sequence_t *sequence, int track)
{
  SEQUENCE_SAFE_GETTER (int, sequence_check_pos (sequence, track, 0) 
                             ? (sequence->tracks[track].mask ? 1 : 0) 
                             : 0);
}

void
sequence_set_pitch (sequence_t *sequence, int track, double pitch)
{
  sequence_lock (sequence);
  if (sequence_check_pos (sequence, track, 0))
  {
    if (sequence->tracks[track].pitch != pitch)
    {
      DEBUG ("Setting pitch on track %d to : %f (was: %f)", track, pitch,
             sequence->tracks[track].pitch);
      sequence_msg_t msg;
      sequence->tracks[track].pitch = pitch;
      if (sequence->tracks[track].sample != NULL)
      {
        double ratio = (double) sequence->framerate 
                       / (double) sequence->tracks[track].sample->framerate 
                       / pow (2, pitch / 12);
        msg.type = SEQUENCE_MSG_SET_SR_RATIO;
        sprintf (msg.text, "track=%d ratio=%f", track, ratio);
        msg_send (sequence->msg, &msg, 0);
      }
      else
      {
        // When there's a sample the event is fired by sequence_receive_messages()
        // otherwise we must fire it here:
        sequence_event_fire_pos (sequence, "track-pitch-changed", 0, track);
      }
    }       
  }
  sequence_unlock (sequence);
}

double
sequence_get_pitch (sequence_t *sequence, int track)
{
  SEQUENCE_SAFE_GETTER (double, sequence_check_pos (sequence, track, 0) 
                                ? sequence->tracks[track].pitch : 0);
}

void
sequence_set_volume (sequence_t *sequence, int track, double volume)
{
  DEBUG ("Setting volume on track %d to : %f", track, volume);
  sequence_lock (sequence);
  if (sequence_check_pos (sequence, track, 0)) {
    sequence_msg_t msg;
    msg.type = SEQUENCE_MSG_SET_VOLUME;
    sprintf (msg.text, "track=%d volume=%f", track, volume);
    msg_send (sequence->msg, &msg, 0);
  }
  sequence_unlock (sequence);
}

void
sequence_multiply_volume (sequence_t *sequence, int track, double ratio)
{
  DEBUG ("Multiplying volume on track %d by : %f", track, ratio);
  sequence_lock (sequence);
  if (sequence_check_pos (sequence, track, 0)) {
    sequence_msg_t msg;
    msg.type = SEQUENCE_MSG_MUL_VOLUME;
    sprintf (msg.text, "track=%d ratio=%f", track, ratio);
    msg_send (sequence->msg, &msg, 0);
  }
  sequence_unlock (sequence);
}

void
sequence_set_volume_db (sequence_t *sequence, int track, double volume)
{
  sequence_set_volume (sequence, track, SEQUENCE_DB2GAIN (volume));
}

void
sequence_increase_volume_db (sequence_t *sequence, int track, double delta)
{
  sequence_multiply_volume(sequence, track, SEQUENCE_DB2GAIN(delta));
}

double
sequence_get_volume (sequence_t *sequence, int track)
{
  SEQUENCE_SAFE_GETTER (double, sequence_check_pos (sequence, track, 0) 
                                ? sequence->tracks[track].volume : 0);
}

double
sequence_get_volume_db (sequence_t *sequence, int track)
{
  return SEQUENCE_GAIN2DB (sequence_get_volume (sequence, track));
}

void
sequence_set_smoothing (sequence_t *sequence, int track, int status)
{
  DEBUG ("Setting smoothing on track %d to : %d", track, status);
  sequence_lock (sequence);
  if (sequence_check_pos (sequence, track, 0)) {
    sequence_msg_t msg;
    msg.type = SEQUENCE_MSG_SET_SMOOTHING;
    sprintf (msg.text, "track=%d status=%d", track, status);
    msg_send (sequence->msg, &msg, 0);
  }
  sequence_unlock (sequence);
}

int
sequence_get_smoothing (sequence_t *sequence, int track)
{
  SEQUENCE_SAFE_GETTER (int, sequence_check_pos (sequence, track, 0) 
                             ? sequence->tracks[track].smoothing : 0);
}


void 
sequence_set_resampler_type (sequence_t * sequence, int type)
{
  int changed = 0;
  sequence_lock (sequence);
  if (type != -1 && sequence->sr_converter_default_type != type) 
  {
    DEBUG ("Setting resampler type to : %d", type);
    sequence_msg_t msg;
    int restart = 0;
    if (sequence->status == SEQUENCE_ENABLED)
     {
      msg.type = SEQUENCE_MSG_ACK_DISABLE;
      msg_send (sequence->msg, &msg, MSG_ACK);
      restart = 1;
     }

    sequence->sr_converter_default_type = type;
    int i;
    for (i=0; i < sequence->tracks_num; i++)
     {
      sequence_track_t *t = sequence->tracks + i;
      t->sr_converter_type = type;
      if (t->sample != NULL)
       {
        int sr_converter_error;
        if (t->sr_converter != NULL) 
         {
          DEBUG ("Destroying SR converter on track %d", i);
          src_delete (t->sr_converter);
          t->sr_converter = NULL;
         }
        if (type == SEQUENCE_SINC)
         {
          DEBUG ("Creating SR converter of type %d with %d channel(s) on track %d",
                 SRC_SINC_FASTEST, t->channels_num, i);
          t->sr_converter = src_new (SRC_SINC_FASTEST, t->channels_num, &sr_converter_error);
         }
       }
     }

    if (restart)
     {
      msg.type = SEQUENCE_MSG_ACK_ENABLE;
      msg_send (sequence->msg, &msg, MSG_ACK);
     }
    changed = 1;
  }
  sequence_unlock (sequence);
  if (changed)
    event_fire(sequence, "resampler-type-changed", NULL, NULL);
}

int
sequence_get_resampler_type (sequence_t *sequence)
{
  SEQUENCE_SAFE_GETTER (int, sequence->sr_converter_default_type);
}

void
sequence_export (sequence_t *sequence, char *filename, int framerate, int sustain_type,
                 progress_callback_t progress_callback, void *progress_data)
{
  sequence_lock (sequence);
  unsigned long bufsize;
  SF_INFO info;
  SNDFILE *fd;
  int i, j;
  float *output;
  unsigned long nframes, pos, nframes_played, sequence_nframes;
  sequence_msg_t msg;
  sequence_t *sequence_tmp;
  sequence_track_t *track;

  DEBUG("Exporting sequence to file: %s", filename);

  SEQUENCE_UNLOCK_CALL(progress_callback ("Preparing to export...", 0, progress_data));

  bufsize = sequence->buffer_size;

  // Stopping sequence
  if (sequence->status == SEQUENCE_ENABLED)
   {
    msg.type = SEQUENCE_MSG_ACK_DISABLE;
    msg_send (sequence->msg, &msg, MSG_ACK);
   }

  sequence_tmp = malloc (sizeof (sequence_t));
  memcpy (sequence_tmp, sequence, sizeof (sequence_t));

  sequence_tmp->tracks = calloc (sequence->tracks_num, sizeof (sequence_track_t));
  memcpy (sequence_tmp->tracks, sequence->tracks, sequence->tracks_num * sizeof (sequence_track_t));

  sequence_tmp->looping = (sustain_type == SEQUENCE_SUSTAIN_LOOP);

  // Allocating temporary buffers
  for (i = 0; i < sequence->tracks_num; i++) 
   {
    track = sequence_tmp->tracks + i;
    track->buffers = calloc (track->channels_num, sizeof (float *));
    for (j = 0; j < track->channels_num; j++)
      track->buffers[j] = calloc (bufsize, sizeof(float));
    if (track->sample)
    {
      *(track->sample_input_pos) = track->sample->frames;
      *(track->sample_output_pos) = track->sample->frames;
    }      
    track->active_beat = -1;
    if (track->sample) 
      track->sr_converter_ratio = track->sr_converter_ratio * framerate / sequence_tmp->framerate;
   }

  sequence_tmp->framerate = framerate;
  sequence_nframes = sequence->beats_num * sequence_get_beat_nframes (sequence_tmp);

  output = calloc (2 * bufsize, sizeof(float));

  // Opening file for writing
  info.samplerate = sequence_tmp->framerate;
  info.channels = 2;
  info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
  fd = sf_open (filename, SFM_WRITE, &info); // Needs error checking
  if (fd) { DEBUG("Successfully opened outfile"); }
  else { DEBUG("FAILED to open outfile"); sf_perror(fd); }

  long int process_total_nframes = 0, process_pos = 0;
  switch (sustain_type)
   {
    case SEQUENCE_SUSTAIN_LOOP:
      process_total_nframes = sequence_nframes * 4;
      break;
    case SEQUENCE_SUSTAIN_TRUNCATE:
      process_total_nframes = sequence_nframes * 3;
      break;
    case SEQUENCE_SUSTAIN_KEEP:
      process_total_nframes = (sequence_nframes + sequence_get_sustain_nframes (sequence)) * 3;
      break;
   } 

  // Dry playing once for loop mixing
  if (sustain_type == SEQUENCE_SUSTAIN_LOOP) 
    for (pos = 0; pos < sequence_nframes; pos += bufsize) 
     {
      SEQUENCE_UNLOCK_CALL(progress_callback ("Preparing loop", 
                                              0.1 + (float) process_pos / process_total_nframes * 0.9, 
                                              progress_data));
      nframes = pos + bufsize < sequence_nframes ? bufsize : sequence_nframes - pos;
      sequence_do_process (sequence_tmp, pos, nframes);
      process_pos += nframes;
     }

  // Finding level peak
  float peak = 0;
  for (pos = 0; pos < sequence_nframes; pos += bufsize) 
   {
    SEQUENCE_UNLOCK_CALL(progress_callback ("Computing level peak", 
                                            0.1 + (float) process_pos / process_total_nframes * 0.9, 
                                            progress_data));
    nframes = pos + bufsize < sequence_nframes ? bufsize : sequence_nframes - pos;
    sequence_do_process (sequence_tmp, pos, nframes);
    sequence_export_mix (sequence_tmp, output, nframes);
    for (i = 0; i < (nframes * 2); i++) if (fabsf (output[i]) > peak) peak = fabsf(output[i]);
    process_pos += nframes;
   }
 
  if (sustain_type == SEQUENCE_SUSTAIN_KEEP)
   {
    for (; (nframes_played = sequence_do_process (sequence_tmp, pos, bufsize)); pos += bufsize) 
     {
      SEQUENCE_UNLOCK_CALL(progress_callback ("Computing level peak", 
                                              0.1 + (float) process_pos / process_total_nframes * 0.9, 
                                              progress_data));
      sequence_export_mix (sequence_tmp, output, nframes_played);
      for (i = 0; i < (nframes_played * 2); i++) if (fabsf (output[i]) > peak) peak = fabsf(output[i]);
      process_pos += nframes_played;
     }
   }

  if (sustain_type != SEQUENCE_SUSTAIN_LOOP)
    for (i = 0; i < sequence_tmp->tracks_num; i++) 
     {
      track = sequence_tmp->tracks + i;
      if (track->sample)
      {
        *(track->sample_input_pos) = track->sample->frames;
        *(track->sample_output_pos) = track->sample->frames;
      }
      track->active_beat = -1;
     }

  DEBUG ("level peak is: %f", peak);

  // Writing to file
  for (pos = 0; pos < sequence_nframes; pos += bufsize) 
   {
    SEQUENCE_UNLOCK_CALL(progress_callback ("Mixing and writing to file", 
                                            0.1 + (float) process_pos / process_total_nframes * 0.9, 
                                            progress_data));
    nframes = pos + bufsize < sequence_nframes ? bufsize : sequence_nframes - pos;
    sequence_do_process (sequence_tmp, pos, nframes);
    sequence_export_mix (sequence_tmp, output, nframes);
    for (i = 0; i < (nframes * 2); i++) output[i] /= peak;
    sf_writef_float (fd, output, nframes);
    process_pos += nframes * 2;
   }

  if (sustain_type == SEQUENCE_SUSTAIN_KEEP)
    for (; (nframes_played = sequence_do_process (sequence_tmp, pos, bufsize)); pos += bufsize) 
     {
      SEQUENCE_UNLOCK_CALL(progress_callback ("Mixing and writing to file", 
                                              0.1 + (float) process_pos / process_total_nframes * 0.9, 
                                              progress_data));
      sequence_export_mix (sequence_tmp, output, nframes_played);
      for (i = 0; i < (nframes_played * 2); i++) output[i] /= peak;
      sf_writef_float (fd, output, nframes_played);
      process_pos += nframes_played * 2;
     }

  DEBUG("Processed %ld frames on an estimated total of %ld", process_pos, process_total_nframes);

  sf_close (fd);

  // Freeing temporary buffers
  free(output);
  for (i = 0; i < sequence->tracks_num; i++) 
   {
    track = sequence_tmp->tracks + i;
    for (j = 0; j < track->channels_num; j++) {
      free (track->buffers[j]);
    }
    free (track->buffers);
   }
  free (sequence_tmp->tracks);
  free (sequence_tmp);

  SEQUENCE_UNLOCK_CALL(progress_callback ("Done", 1, progress_data));
  sequence_unlock (sequence);

}

static int
sequence_process_events (void *data)
{
  sequence_t *sequence = (sequence_t *) data;
  sequence_lock (sequence);
  msg_process_events (sequence->msg, sequence);
  sequence_unlock (sequence);
  return 0;
}

void
sequence_wait (sequence_t *sequence)
{
  sequence_msg_t msg;
  msg.type = SEQUENCE_MSG_ACK;
  SEQUENCE_LOCK_CALL (msg_send (sequence->msg, &msg, MSG_ACK));
}

unsigned long 
sequence_get_sample_position (sequence_t *sequence, int track)
{
  unsigned long pos = -1;
  sequence_lock (sequence);
  if (sequence_check_pos (sequence, track, 0))
  {
    sequence_track_t *t = sequence->tracks + track;
    if (t->sample && sequence_track_is_playing(sequence, track) && (*(t->sample_output_pos) < t->sample->frames))
      pos = *(t->sample_output_pos);
  }
  sequence_unlock (sequence);
  return pos;
}

void
sequence_normalize_name (char *name)
{
    util_strcanon(name, SEQUENCE_VALID_NAME, '_');
}

void
sequence_swap_tracks(sequence_t *sequence, int track1, int track2)
{
  sequence_lock (sequence);
  if (sequence_check_pos(sequence, track1, 0) && sequence_check_pos(sequence, track2, 0) 
          && track1 != track2) {
      sequence_msg_t msg;
      msg.type = SEQUENCE_MSG_SWAP_TRACKS;
      sprintf (msg.text, "track1=%d track2=%d", track1, track2);
      msg_send (sequence->msg, &msg, 0);
  }
  sequence_unlock (sequence);
}
