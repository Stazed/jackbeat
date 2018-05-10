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
 *   SVN:$Id: sample.h 600 2009-06-24 23:16:41Z olivier $
 */


#ifndef JACKBEAT_SAMPLE_H
#define JACKBEAT_SAMPLE_H

#include <sndfile.h>
#include <sys/types.h>

#include "types.h"

typedef struct sample_t {
    float * data;
    int channels_num;
    sf_count_t frames;
    int framerate;
    int orig_format;
    char name[256];
    char filename[1024];
    FILE * cache;
    off_t last_file_size;
    time_t last_file_ctime;
    float peak;
    int ref_num;
} sample_t;

sample_t * sample_new(char *filename, progress_callback_t progress_callback,
        void *progress_data);
int sample_write(sample_t *sample, char *path, progress_callback_t progress_callback,
        void *progress_data);
char * sample_storage_basename(sample_t *sample);
int sample_compare(sample_t * sample, char *filename);
void sample_ref(sample_t *sample);
void sample_unref(sample_t *sample);
char ** sample_list_known_extensions();

#endif
