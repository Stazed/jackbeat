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
 *   SVN:$Id: stream.c 533 2009-04-20 16:28:51Z olivier $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "core/event.h"
#include "core/array.h"
#include "driver.h"
#include "null.h"

struct stream_t
{
    stream_driver_t * driver;
    stream_port_t **  ports;
    int               nports;
    int               auto_connect;
} ;

struct stream_port_t
{
    stream_driver_port_t *driver_port;
} ;

stream_port_t *
stream_port_add (stream_t *self, char *name, stream_port_flags_t flags)
{
    stream_port_t *port = NULL;
    stream_driver_port_t *driver_port;
    driver_port = self->driver->interface->port_add (self->driver, name, flags, NULL);
    if (driver_port)
    {
        port = malloc (sizeof (stream_port_t));
        port->driver_port = driver_port;
        ARRAY_ADD (stream_port_t, self->ports, self->nports, port);
    }
    return port;
}

void
stream_port_rename (stream_t *self, stream_port_t *port, char *name)
{
    self->driver->interface->port_rename (self->driver, port->driver_port, name);
}

float *
stream_port_get_buffer (stream_t *self, stream_port_t *port, int nframes)
{
    return self->driver->interface->port_get_buffer (self->driver, port->driver_port, nframes);
}

void
stream_port_remove (stream_t *self, stream_port_t *port)
{
    self->driver->interface->port_remove (self->driver, port->driver_port);
    ARRAY_REMOVE (stream_port_t, self->ports, self->nports, port);
    free (port);
}

/*
void            
stream_connect_physical (stream_t *self)
{
  self->driver->interface->connect_physical (self->driver);
}
 */

void
stream_port_touch (stream_t *self, stream_port_t *port)
{
    self->driver->interface->port_touch (self->driver, port->driver_port);
}

#ifdef USE_DEPRECIATED
int
stream_port_get_latency (stream_t *self, stream_port_t *port)
{
    return self->driver->interface->port_get_latency (self->driver, port->driver_port);
}
#endif // USE_DEPRECIATED

int
stream_get_buffer_size (stream_t *self)
{
    return self->driver->interface->get_buffer_size (self->driver);
}

int
stream_get_sample_rate (stream_t *self)
{
    return self->driver->interface->get_sample_rate (self->driver);
}

int
stream_add_process (stream_t *self, char *name, stream_process_t callback, void *data)
{
    return self->driver->interface->add_process (self->driver, name, callback, data);
}

int
stream_rename_process (stream_t *self, char *old_name, char *new_name)
{
    return self->driver->interface->rename_process (self->driver, new_name, old_name);
}

void
stream_remove_process (stream_t *self, char *name)
{
    self->driver->interface->remove_process (self->driver, name);
}

void
stream_process_exists (stream_t *self, char *name)
{
    self->driver->interface->process_exists (self->driver, name);
}

unsigned long
stream_get_position (stream_t *self)
{
    return self->driver->interface->get_position (self->driver);
}

int
stream_is_started (stream_t *self)
{
    return self->driver->interface->is_started (self->driver);
}

void
stream_start (stream_t *self)
{
    self->driver->interface->start (self->driver);
}

void
stream_stop (stream_t *self)
{
    self->driver->interface->stop (self->driver);
}

void
stream_seek (stream_t *self, unsigned long position)
{
    self->driver->interface->seek (self->driver, position);
}

void
stream_destroy (stream_t *self)
{
    if (self->driver)
    {
        self->driver->interface->deactivate (self->driver);
        self->driver->interface->destroy (self->driver);
        self->driver = NULL;
    }
    ARRAY_DESTROY (self->ports, self->nports);
    event_remove_source (self);
    free (self);
}

static void
copy_driver_resources (stream_t *self, stream_driver_t *src)
{
    int i;
    for (i = 0; i < self->nports; i++)
    {
        stream_driver_port_t *dport = self->ports[i]->driver_port;
        self->ports[i]->driver_port = self->driver->interface->port_add (self->driver, dport->name, dport->flags, NULL);
    }

    stream_driver_copy_processes (src, self->driver);
}

static void
on_shutdown (void *arg)
{
    stream_t *self = (stream_t *) arg;
    DEBUG ("Warning: %s is shutting down", self->driver->classname);

    stream_driver_t *null_driver = stream_driver_null_new ();
    null_driver->interface->activate (null_driver);
    stream_driver_t *old_driver = self->driver;
    self->driver = null_driver;
    copy_driver_resources (self, old_driver);
    old_driver->interface->destroy (old_driver);
    event_fire (self, "connection-changed", NULL, NULL);
    event_fire (self, "connection-lost", NULL, NULL);
}

int
stream_is_connected (stream_t *self)
{
    return (self->driver && strcmp (self->driver->classname, "NullStreamDriver")) ? 1 : 0;
}

const char *
stream_get_driver_classname (stream_t *self)
{
    if (self->driver)
        return (const char *) self->driver->classname;
    else
        return NULL;
}

stream_driver_t *
stream_get_driver (stream_t *self)
{
    return self->driver;
}

stream_t *
stream_new ()
{
    stream_t *self = malloc (sizeof (stream_t));
    self->driver = NULL;
    self->ports = NULL;
    self->nports = 0;
    event_register (self, "connection-changed");
    event_register (self, "connection-lost");
    stream_set_driver (self, stream_driver_null_new ());
    return self;
}

/*
int
stream_set_driver (stream_t *self, stream_driver_t *driver)
{
  int res = 0;
  stream_driver_t *old_driver = self->driver;
  if (old_driver)
    old_driver->interface->deactivate (old_driver);
  if (driver->interface->activate(driver))    
  {
    DEBUG("driver activated");
    if (old_driver)
      copy_driver_resources (self, driver);
      
    self->driver = driver;
    stream_driver_set_shutdown_callback (self->driver, on_shutdown, (void *) self);

    if (old_driver)
      old_driver->interface->destroy (old_driver);

    res = 1;      
  }
  else
  {
    DEBUG("can't activate driver");
    driver->interface->destroy(driver);
    if (old_driver)
      old_driver->interface->activate (old_driver);
  }
  return res;
}
 */

int
stream_set_driver (stream_t *self, stream_driver_t *driver)
{
    int res = 0;
    stream_driver_t *old_driver = self->driver;
    if (old_driver)
        old_driver->interface->deactivate (old_driver);
    if (driver->interface->activate (driver))
    {
        DEBUG ("driver activated");
        res = 1;
    }
    else
    {
        DEBUG ("Failed to activate driver, loading Null driver");
        driver->interface->destroy (driver);
        driver = stream_driver_null_new ();
        DEBUG ("-1");
        driver->interface->activate (driver);
    }

    self->driver = driver;
    self->driver->interface->auto_connect (self->driver, self->auto_connect);

    if (old_driver)
        copy_driver_resources (self, old_driver);

    stream_driver_set_shutdown_callback (self->driver, on_shutdown, (void *) self);

    if (old_driver)
        old_driver->interface->destroy (old_driver);

    event_fire (self, "connection-changed", NULL, NULL);
    return res;
}

void
stream_disconnect (stream_t *self)
{
    stream_set_driver (self, stream_driver_null_new ());
}

void
stream_auto_connect (stream_t *self, int active)
{
    self->auto_connect = active;
    if (self->driver)
        self->driver->interface->auto_connect (self->driver, active);
}
