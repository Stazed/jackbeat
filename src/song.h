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
 *   SVN:$Id: song.h 530 2009-04-19 21:32:58Z olivier $
 */

#ifndef JACKBEAT_SONG_H
#define JACKBEAT_SONG_H

#include "sample.h"
#include "sequence.h"

typedef struct song_t song_t;

song_t *      song_new (pool_t * pool);
void          song_destroy (song_t * song);
void          song_register_sequence (song_t *song, sequence_t *sequence);
void          song_register_sequence_samples (song_t *song, sequence_t *sequence);
int           song_count_sequences (song_t * song);
sequence_t ** song_list_sequences (song_t * song);
void          song_register_sample (song_t *song, sample_t *sample);
sample_t *    song_try_reuse_sample (song_t *song, char *filename);
  
#endif /* JACKBEAT_SONG_H */
