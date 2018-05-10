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
 *   SVN:$Id: paudio.c 234 2008-08-02 22:13:09Z olivier $
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <dlfcn.h>
#include "pulse.h"
#include "driver.h"
#include "stereo.h"

#define CLASSNAME "PulseAudioStreamDriver"
#define CAST(self) stream_driver_cast (self, CLASSNAME)
#define BIND(self, parent, data) \
  stream_driver_t *_self = CAST(self); \
  stream_driver_t *parent = _self->parent; \
  stream_driver_pulse_data_t *data = (stream_driver_pulse_data_t *) _self->data; 
#define BIND_DATA(self, data) stream_driver_pulse_data_t *data = (stream_driver_pulse_data_t *) (CAST(self)->data)
#define BIND_PARENT(self, parent) stream_driver_t *parent = CAST(self)->parent; 

typedef struct stream_driver_pulse_data_t 
{
  char *      app_name;
  pa_simple * connection;
  float *     buffer;
} stream_driver_pulse_data_t;

static void
destroy (stream_driver_t *self)
{
  BIND (self, parent, data);

  free (data->app_name);
  free (data);

  parent->interface->destroy (self);

  self = CAST (self);
  free (self->classname);
  free (self->interface);
  free (self);
}

static void
thread_process (stream_driver_t *self)
{
  BIND_DATA (self, data);
  self->interface->sync (self);
  self->interface->iterate (self, 256, data->buffer);
  int err;
  //DEBUG("Pulse audio latency: %.3f ms", (double) pa_simple_get_latency (data->connection, NULL) / 1000);
  //DEBUG("Pulse audio latency: %.3f ms", pa_simple_get_latency (data->connection, NULL) / 1000);
  pa_simple_write (data->connection, data->buffer, sizeof(float) * 256 * 2, &err);
}

static int
activate(stream_driver_t *self)
{
  BIND(self, parent, data);

  pa_sample_spec spec;
  spec.format = PA_SAMPLE_FLOAT32;
  spec.channels = 2;
  spec.rate = self->interface->get_sample_rate(self);

  DEBUG ("Connecting to PulseAudio");
  int err;
  data->connection = pa_simple_new(NULL,               // Use the default server.
                                   data->app_name,     // Our application's name.
                                   PA_STREAM_PLAYBACK,
                                   NULL,               // Use the default device.
                                   "Music",            // Description of our stream.
                                   &spec,              // Our sample format.
                                   NULL,               // Use default channel map
                                   NULL,               // Use default buffering attributes.
                                   &err                // Ignore error code.
                                   );

  if (data->connection)
  {
    //DEBUG("Pulse audio latency: %.3f ms", (double) pa_simple_get_latency (data->connection, NULL) / 1000);
    if (!parent->interface->activate (self))
    {
      pa_simple_free (data->connection);
      data->connection = NULL;
    }
  } 
  else 
    DEBUG("PulseAudio error: %s", pa_strerror (err));
 

  return (data->connection != NULL);
}

static int
deactivate (stream_driver_t *self)
{
  BIND(self, parent, data);
  int res = 1;
  if (data->connection != NULL) 
  {
    parent->interface->deactivate (self);
    pa_simple_free (data->connection);
  }
  return res;
}
static int
port_get_latency (stream_driver_t *self, stream_driver_port_t *port)
{
  BIND_DATA(self, data);
  unsigned long latency = pa_simple_get_latency (data->connection, NULL);
  if (latency >= 0)
    return (float) self->interface->get_sample_rate (self) * (float) latency / 1000000.0f;
  else
    return 0;  
  return 0;
}


stream_driver_t *
stream_driver_pulse_new (char *app_name)
{
  stream_driver_t *self = stream_driver_subclass (CLASSNAME, stream_driver_stereo_new ());
  self->data = malloc (sizeof (stream_driver_pulse_data_t));
  BIND_DATA (self, data);
  data->connection = NULL;
  data->app_name = strdup (app_name);
  data->buffer = malloc (STREAM_BUFFER_SIZE * sizeof (float) * 2);

  self->interface->destroy           = destroy;
  self->interface->activate          = activate;
  self->interface->deactivate        = deactivate;
  self->interface->thread_process    = thread_process;
  self->interface->port_get_latency  = port_get_latency;

  return self;
}
