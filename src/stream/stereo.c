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
 *   SVN:$Id: stereo.c 230 2008-08-02 13:46:28Z olivier $
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include "stereo.h"
#include "driver.h"

#define CLASSNAME "StereoStreamDriver"
#define CAST(self) stream_driver_cast (self, CLASSNAME)
#define BIND(self, parent, data) \
  stream_driver_t *_self = CAST(self); \
  stream_driver_t *parent = _self->parent; \
  stream_driver_stereo_data_t *data = (stream_driver_stereo_data_t *) _self->data; 
#define BIND_DATA(self, data) stream_driver_stereo_data_t *data = (stream_driver_stereo_data_t *) (CAST(self)->data)
#define BIND_PARENT(self, parent) stream_driver_t *parent = CAST(self)->parent; 

typedef struct stream_driver_stereo_data_t 
{
  unsigned long position;
  int           playing;
} stream_driver_stereo_data_t;

static unsigned long   
get_position (stream_driver_t *self)
{
  BIND_DATA(self, data);
  return data->position;
}

static int           
is_started (stream_driver_t *self)
{
  BIND_DATA(self, data);
  return data->playing;
}

static void            
start (stream_driver_t *self)
{
  BIND_DATA(self, data);
  data->playing = 1;
}

static void            
stop (stream_driver_t *self)
{
  BIND_DATA(self, data);
  data->playing = 0;
}

static void            
seek (stream_driver_t *self, unsigned long position)
{
  BIND_DATA(self, data);
  data->position = position;
}

static void
destroy (stream_driver_t *self)
{
  BIND (self, parent, data);

  free (data);

  parent->interface->destroy (self);

  self = CAST (self);
  free (self->classname);
  free (self->interface);
  free (self);
}

static int
activate(stream_driver_t *self)
{
  BIND (self, parent, data);
  data->position = 0;
  data->playing = 0;
  return parent->interface->activate (self);
}

static int 
iterate (stream_driver_t *self, int nframes, ...)
{
  BIND (self, parent, data);
  va_list ap;
  va_start (ap, nframes);
  float *output = va_arg (ap, float *);
  va_end (ap);

  int len = parent->interface->iterate (self, nframes);

  stream_driver_port_t **ports = stream_driver_get_ports(self);
  int nports = stream_driver_get_ports_num(self);

  int j, k;
  for (j = 0; j < len * 2; j++)
    output[j] = 0;

  for (j = 0; j < nports; j++)
  {
    stream_driver_port_t *port = ports[j];
    if (port->flags & STREAM_LEFT)
      for (k = 0; k < len; k++)
        output[k * 2] += port->buffer[k];

    if (port->flags & STREAM_RIGHT)
      for (k = 0; k < len; k++)
        output[k * 2 + 1] += port->buffer[k];
  }
  if (data->playing)
    data->position += len;

  return len;    
}  

stream_driver_t *
stream_driver_stereo_new (char *client_name)
{
  stream_driver_t *self = stream_driver_subclass (CLASSNAME, stream_driver_new ());
  self->data = malloc (sizeof (stream_driver_stereo_data_t));
  BIND_DATA (self, data);

  data->position = 0;
  data->playing = 0;

  self->interface->get_position      = get_position;
  self->interface->is_started        = is_started;
  self->interface->start             = start;
  self->interface->stop              = stop;
  self->interface->seek              = seek;
  self->interface->destroy           = destroy;
  self->interface->iterate           = iterate;
  self->interface->activate          = activate;

  return self;
}
