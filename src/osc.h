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
 *   SVN:$Id: jab.h 136 2008-04-07 19:25:50Z olivier $
 */

#ifndef JACKBEAT_OSC_H
#define JACKBEAT_OSC_H

#include "song.h"

typedef struct osc_t osc_t;

typedef enum {
    OSC_IN,
    OSC_OUT
} osc_method_type_t;

typedef struct {
    osc_method_type_t type;
    char * path;
    char * name;
    char * typespec;
    char ** parameters;
    char * comment;
} osc_method_desc_t;

osc_t * osc_new(song_t *song);
void osc_destroy(osc_t *osc);
void osc_print_interface();
int osc_set_port(osc_t *osc, int port);
int osc_get_port(osc_t *osc);
void osc_set_sequence_target(osc_t *osc, sequence_t *sequence, const char *host,
        int port, const char *prefix);
void osc_get_sequence_target(osc_t *osc, sequence_t *sequence, char **host,
        int *port, char **prefix);
void osc_set_sequence_input_prefix(osc_t *osc, sequence_t *sequence,
        const char *prefix);
int osc_get_sequence_input_prefix(osc_t *osc, sequence_t *sequence,
        char **prefix);
osc_method_desc_t ** osc_reflect_sequence_methods(osc_t *osc, sequence_t *sequence);
void osc_reflect_free(osc_method_desc_t **descs);
#endif
