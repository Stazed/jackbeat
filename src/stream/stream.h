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
 *   SVN:$Id: stream.h 529 2009-04-19 20:50:12Z olivier $
 */

#ifndef JACKBEAT_STREAM_H
#define JACKBEAT_STREAM_H

typedef enum 
{
  STREAM_OUTPUT = 1,
  STREAM_INPUT = 2,
  STREAM_LEFT = 4,
  STREAM_RIGHT = 8
} stream_port_flags_t;

typedef int  (* stream_process_t) (unsigned long nframes, void *arg);

typedef struct stream_t stream_t;
typedef struct stream_driver_t stream_driver_t;
typedef struct stream_port_t stream_port_t;

stream_t *      stream_new ();
int             stream_set_driver (stream_t *, stream_driver_t *driver);
stream_port_t * stream_port_add (stream_t *, char *name, stream_port_flags_t flags);
void            stream_port_rename (stream_t *, stream_port_t *, char *name);
float *         stream_port_get_buffer (stream_t *, stream_port_t *, int nframes);
void            stream_port_remove (stream_t *, stream_port_t *port);
void            stream_port_touch (stream_t *, stream_port_t *port);
int             stream_port_get_latency (stream_t *, stream_port_t *port);
void            stream_auto_connect (stream_t *, int active);
int             stream_get_buffer_size (stream_t *);
int             stream_get_sample_rate (stream_t *);
int             stream_add_process (stream_t *, char * name, stream_process_t callback, void *data); 
int             stream_rename_process (stream_t *self, char *old_name, char *new_name);
void            stream_remove_process (stream_t *, char * name); 
void            stream_process_exists (stream_t *, char * name); 
unsigned long   stream_get_position (stream_t *);
int             stream_is_started (stream_t *);
void            stream_start (stream_t *);
void            stream_stop (stream_t *);
void            stream_rewind (stream_t *);
void            stream_seek (stream_t *, unsigned long position);
void            stream_destroy (stream_t *);
int             stream_is_connected (stream_t *);
const char *    stream_get_driver_classname (stream_t *);
stream_driver_t * stream_get_driver (stream_t *);
void            stream_disconnect (stream_t *);

#endif
