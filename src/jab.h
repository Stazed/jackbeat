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
 *   SVN:$Id: jab.h 282 2008-09-03 12:26:13Z olivier $
 */

#ifndef JACKBEAT_JAB_H
#define JACKBEAT_JAB_H

#include "types.h"
#include "core/event.h"
#include "stream/stream.h"

#define JAB_WRITE 1
#define JAB_READ 2

typedef struct jab_t jab_t;

jab_t * jab_open(char *path, int mode, progress_callback_t progress_callback, void *progress_data, int *error);
void jab_add_sequence(jab_t *jab, sequence_t *sequence);
sequence_t * jab_retrieve_sequence(jab_t *jab, stream_t *stream, char *sequence_name, int *error);
int jab_close(jab_t *jab);

#endif
