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
 *   SVN:$Id: rc.h 291 2008-09-08 17:25:49Z olivier $
 */

#ifndef JACKBEAT_RC_H
#define JACKBEAT_RC_H

typedef char history_t[10][512];

typedef struct rc_t {
    char sample_wdir[512];
    char sequence_wdir[512];
    history_t sample_history;
    int sample_history_num;
    history_t sequence_history;
    int sequence_history_num;
    int default_resampler_type;
    int transport_aware;
    int transport_query;
    int auto_connect;
    int osc_input_port;
    char client_name[512];
    char audio_output[512];
    int audio_sample_rate;
    int jack_auto_start;
} rc_t;

void rc_write(rc_t * rc);
void rc_read(rc_t *rc);
void rc_add_sample(rc_t *rc, char *filename);
void rc_add_sequence(rc_t *rc, char *filename);


#endif
