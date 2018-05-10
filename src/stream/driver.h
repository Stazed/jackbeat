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
 *   SVN:$Id: driver.h 529 2009-04-19 20:50:12Z olivier $
 */

#ifndef JACKBEAT_STREAM_DRIVER_H
#define JACKBEAT_STREAM_DRIVER_H

#include "stream.h"

#define STREAM_BUFFER_SIZE 2048

typedef struct stream_driver_port_t
{
  float *             buffer;
  char *              name;
  stream_port_flags_t flags;
  void *              data;
} stream_driver_port_t;

typedef struct stream_driver_interface_t 
{
  stream_driver_port_t * (* port_add) (stream_driver_t *, char *name, 
                                       stream_port_flags_t flags, void *port_data);
  void            (* port_rename) (stream_driver_t *, stream_driver_port_t *, char *name);
  float *         (* port_get_buffer) (stream_driver_t *, stream_driver_port_t *, int nframes);
  void            (* port_remove) (stream_driver_t *, stream_driver_port_t *port);
  void            (* port_touch) (stream_driver_t *, stream_driver_port_t *port);
  int             (* port_get_latency) (stream_driver_t *, stream_driver_port_t *port);
  void            (* auto_connect) (stream_driver_t *, int active);
  int             (* get_buffer_size) (stream_driver_t *);
  int             (* get_sample_rate) (stream_driver_t *);
  int             (* add_process) (stream_driver_t *, char *name, stream_process_t callback, void *data); 
  void            (* remove_process) (stream_driver_t *, char *name); 
  int             (* rename_process) (stream_driver_t *, char *old_name, char *new_name); 
  int             (* process_exists) (stream_driver_t *, char *name); 
  unsigned long   (* get_position) (stream_driver_t *);
  int             (* is_started) (stream_driver_t *);
  void            (* start) (stream_driver_t *);
  void            (* stop) (stream_driver_t *);
  void            (* rewind) (stream_driver_t *);
  void            (* seek) (stream_driver_t *, unsigned long position);
  void            (* destroy) (stream_driver_t *);
  void            (* sync) (stream_driver_t *);
  int             (* iterate) (stream_driver_t *, int nframes, ...);
  void            (* on_shutdown) (stream_driver_t *);
  int             (* activate) (stream_driver_t *);
  int             (* deactivate) (stream_driver_t *);
  void            (* thread_process) (stream_driver_t *);
} stream_driver_interface_t;  

struct stream_driver_t 
{
  char *                      classname;
  stream_driver_t *           parent;
  void *                      data;
  stream_driver_interface_t * interface;
};

stream_driver_t *   stream_driver_new ();
stream_driver_t *   stream_driver_cast (stream_driver_t *, char *classname);
stream_driver_t *   stream_driver_subclass (char *classname, stream_driver_t *parent);
stream_driver_port_t **    stream_driver_get_ports (stream_driver_t *);
int                 stream_driver_get_ports_num (stream_driver_t *);
void                stream_driver_set_shutdown_callback (stream_driver_t *, void (* callback) (void *), void *arg);
void                stream_driver_copy_processes (stream_driver_t *, stream_driver_t *other);

#define DEBUG(M, ...) ({ printf("STM  %s(): ", __func__); printf(M, ## __VA_ARGS__); printf("\n"); })

#endif
