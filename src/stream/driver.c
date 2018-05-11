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
 *   SVN:$Id: driver.c 599 2009-06-24 21:24:58Z olivier $
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "driver.h"
#include "core/msg.h"
#include "core/array.h"

#define CLASSNAME "StreamDriver"
#define CAST(self) stream_driver_cast (self, CLASSNAME)
#define BIND(self, parent, data) \
  stream_driver_t *_self = CAST(self); \
  stream_driver_t *parent = _self->parent; \
  stream_driver_data_t *data = (stream_driver_data_t *) _self->data
#define BIND_DATA(self, data) stream_driver_data_t *data = (stream_driver_data_t *) (CAST(self)->data)
#define BIND_PARENT(self, parent) stream_driver_t *parent = CAST(self)->parent; 

enum
{
    STREAM_PORTS_REPLACE,
    STREAM_PROCESS_REPLACE
} ;

typedef struct stream_driver_process_t
{
    stream_process_t    callback;
    void *              data;
    char *              name;
} stream_driver_process_t;

typedef struct stream_driver_data_t
{
    stream_driver_process_t * processes;
    int                 nprocesses;
    stream_driver_port_t **    ports;
    int                 nports;
    msg_t *             msg;
    void                (* shutdown_callback) (void *) ;
    void *              shutdown_data;

    pthread_t           thread;
    int volatile        thread_terminate;
    int                 thread_running;
} stream_driver_data_t;

static stream_driver_port_t *
port_add (stream_driver_t *self, char *name, stream_port_flags_t flags, void *port_data)
{
    BIND_DATA (self, data);
    stream_driver_port_t *port = malloc (sizeof (stream_driver_port_t));
    port->buffer = calloc (STREAM_BUFFER_SIZE, sizeof (float));
    port->data = port_data;
    port->flags = flags;
    port->name = strdup (name);

    stream_driver_port_t **oldports = data->ports;
    stream_driver_port_t **newports = calloc (data->nports + 1, sizeof (stream_driver_port_t *));
    memcpy (newports, data->ports, data->nports * sizeof (stream_driver_port_t *));
    newports[data->nports] = port;

    msg_call (data->msg, STREAM_PORTS_REPLACE, MSG_ACK, "ports=%p nports=%d",
              newports, data->nports + 1);

    free (oldports);

    return port;
}

static void
port_rename (stream_driver_t *self, stream_driver_port_t *port, char *name)
{
    char *old_name = port->name;
    port->name = strdup (name);
    free (old_name);
}

static float *
port_get_buffer (stream_driver_t *self, stream_driver_port_t *port, int nframes)
{
    return port->buffer;
}

static void
port_remove (stream_driver_t *self, stream_driver_port_t *port)
{
    BIND_DATA (self, data);
    stream_driver_port_t **oldports = data->ports;
    stream_driver_port_t **newports = calloc (data->nports, sizeof (stream_driver_port_t *));
    memcpy (newports, data->ports, data->nports * sizeof (stream_driver_port_t *));
    int newports_num = data->nports;
    ARRAY_REMOVE (stream_driver_port_t, newports, newports_num, port);
    msg_call (data->msg, STREAM_PORTS_REPLACE, MSG_ACK, "ports=%p nports=%d",
              newports, newports_num);
    free (port->buffer);
    free (port->name);
    free (port);
    free (oldports);
}

static void
auto_connect (stream_driver_t *self, int active) { }

static void
port_touch (stream_driver_t *self, stream_driver_port_t *port) { }

#ifdef JACK_GET_LATENCY
static int
port_get_latency (stream_driver_t *self, stream_driver_port_t *port)
{
    return 0;
}
#endif // JACK_GET_LATENCY

static int
get_buffer_size (stream_driver_t *self)
{
    return STREAM_BUFFER_SIZE;
}

static int
get_sample_rate (stream_driver_t *self)
{
    return 44100;
}

static int
process_exists (stream_driver_t *self, char *name)
{
    BIND_DATA (self, data);

    int i, exists = 0;
    for (i = 0; i < data->nprocesses; i++)
        if (!strcmp (data->processes[i].name, name))
        {
            exists = 1;
            break;
        }

    return exists;
}

static int
add_process (stream_driver_t *self, char *name, stream_process_t callback, void *userdata)
{
    BIND_DATA (self, data);

    int exists = self->interface->process_exists (self, name);

    if (!exists)
    {
        stream_driver_process_t *old = data->processes;
        stream_driver_process_t *new = calloc (data->nprocesses + 1, sizeof (stream_driver_process_t));
        memcpy (new, old, data->nprocesses * sizeof (stream_driver_process_t));
        new[data->nprocesses].callback = callback;
        new[data->nprocesses].data     = userdata;
        new[data->nprocesses].name     = strdup (name);

        msg_call (data->msg, STREAM_PROCESS_REPLACE, MSG_ACK,
                  "processes=%p nprocesses=%d",
                  new, data->nprocesses + 1);

        free (old);
    }
    else
    {
        DEBUG ("Warning: process '%s' already exists", name);
    }

    return !exists;
}

static int
rename_process (stream_driver_t *self, char *old_name, char *new_name)
{
    BIND_DATA (self, data);

    int exists = self->interface->process_exists (self, old_name);
    if (exists)
    {
        int i;
        for (i = 0; i < data->nprocesses; i++)
        {
            if (!strcmp (data->processes[i].name, old_name))
            {
                char *p                 = data->processes[i].name;
                data->processes[i].name = strdup (new_name);
                free (p);
                break;
            }
        }
    }
    else
    {
        DEBUG ("Warning: no such process: '%s'", old_name);
    }
    return exists;
}

static void
remove_process (stream_driver_t *self, char *name)
{
    BIND_DATA (self, data);

    if (self->interface->process_exists (self, name))
    {
        stream_driver_process_t *old = data->processes;
        stream_driver_process_t *new = calloc (data->nprocesses - 1, sizeof (stream_driver_process_t));

        int i, j = 0;
        char *old_name = NULL;
        for (i = 0; i < data->nprocesses; i++)
        {
            if (strcmp (old[i].name, name))
                memcpy (new + j++, old + i, sizeof (stream_driver_process_t));
            else
                old_name = old[i].name;
        }

        msg_call (data->msg, STREAM_PROCESS_REPLACE, MSG_ACK,
                  "processes=%p nprocesses=%d",
                  new, data->nprocesses - 1);

        if(old_name != NULL)
            free (old_name);
        free (old);
    }
    else
    {
        DEBUG ("Warning: no such process: '%s'", name);
    }
}

static unsigned long
get_position (stream_driver_t *self)
{
    return 0;
}

static int
is_started (stream_driver_t *self)
{
    return 0;
}

static void
start (stream_driver_t *self) { }

static void
stop (stream_driver_t *self) { }

static void
seek (stream_driver_t *self, unsigned long position) { }

static void
destroy (stream_driver_t *self)
{
    BIND_DATA (self, data);

    if (data->msg)
        msg_destroy (data->msg);

    int i;
    for (i = 0; i < data->nports; i++)
    {
        free (data->ports[i]->buffer);
        free (data->ports[i]);
    }
    free (data->ports);

    free (data->processes);

    self = CAST (self);

    free (self->classname);
    free (self->interface);
    free (self->data);
    free (self);
}

static void
on_shutdown (stream_driver_t *self)
{
    BIND_DATA (self, data);
    if (data->shutdown_callback)
        data->shutdown_callback (data->shutdown_data);
}

static void
sync (stream_driver_t *self)
{
    BIND_DATA (self, data);
    msg_call_t call;

    while (msg_receive (data->msg, &call))
    {
        switch (call.feature)
        {
            case STREAM_PORTS_REPLACE:
                sscanf (call.params, "ports=%p nports=%d", (void **) &data->ports, &data->nports);
                break;
            case STREAM_PROCESS_REPLACE:
                sscanf (call.params, "processes=%p nprocesses=%d", (void **) &data->processes,
                        &data->nprocesses);
                break;
        }
    }

    msg_sync (data->msg);
}

static int
iterate (stream_driver_t *self, int nframes, ...)
{
    BIND_DATA (self, data);

    if (nframes > STREAM_BUFFER_SIZE)
        nframes = STREAM_BUFFER_SIZE;

    int i;
    for (i = 0; i < data->nprocesses; i++)
        data->processes[i].callback (nframes, data->processes[i].data);

    return nframes;
}

void
stream_driver_copy_processes (stream_driver_t *self, stream_driver_t *other)
{
    BIND_DATA (self, data);
    int i;

    for (i = 0; i < data->nprocesses; i++)
        other->interface->add_process (other, data->processes[i].name,
                                       data->processes[i].callback, data->processes[i].data);
}

static void *
thread_start (void * arg)
{
    stream_driver_t *self = (stream_driver_t *) arg;
    BIND_DATA (self, data);
    DEBUG ("Thread entered");
    while (!data->thread_terminate)
        self->interface->thread_process (self);

    return NULL;
}

static int
activate (stream_driver_t *self)
{
    BIND_DATA (self, data);
    int res = 1;
    if (self->interface->thread_process)
    {
        int test = pthread_create (&data->thread, NULL, thread_start,
                                   (void *) self);

        if (test == 0)
        {
            DEBUG ("Created thread");
        }
        else
        {
            DEBUG ("Failed to create thread");
        }

        res = data->thread_running = !test;
    }
    return res;
}

static int
deactivate (stream_driver_t *self)
{
    BIND_DATA (self, data);
    int res = 1;
    if (data->thread_running)
    {
        data->thread_terminate = 1;
        int test = pthread_join (data->thread, NULL);
        if (!test)
            DEBUG ("Stopped thread");
        else
            DEBUG ("Failed to stop thread");

        res = !test;
        data->thread_running = test;
        data->thread_terminate = 0;
    }
    return res;
}

stream_driver_t *
stream_driver_cast (stream_driver_t *self, char *classname)
{
    //FIXME: check for unexistent classname
    while (strcmp (self->classname, classname))
        self = self->parent;
    return self;
}

stream_driver_t *
stream_driver_subclass (char *classname, stream_driver_t *parent)
{
    stream_driver_t *child = malloc (sizeof (stream_driver_t));
    child->interface = malloc (sizeof (stream_driver_interface_t));
    child->parent = parent;
    if (parent)
        memcpy (child->interface, parent->interface, sizeof (stream_driver_interface_t));
    child->data = NULL;
    child->classname = strdup (classname);
    return child;
}

stream_driver_port_t **
stream_driver_get_ports (stream_driver_t *self)
{
    BIND_DATA (self, data);
    return data->ports;
}

int
stream_driver_get_ports_num (stream_driver_t *self)
{
    BIND_DATA (self, data);
    return data->nports;
}

void
stream_driver_set_shutdown_callback (stream_driver_t *self, void (* callback) (void *) , void *arg)
{
    BIND_DATA (self, data);
    data->shutdown_callback = callback;
    data->shutdown_data = arg;
}

stream_driver_t *
stream_driver_new ()
{
    stream_driver_t *self = stream_driver_subclass (CLASSNAME, NULL);
    self->data = malloc (sizeof (stream_driver_data_t));
    self->parent = NULL;

    BIND_DATA (self, data);
    data->processes = NULL;
    data->nprocesses = 0;
    data->ports = NULL;
    data->nports = 0;
    data->msg = msg_new (4096, sizeof (msg_call_t));
    data->shutdown_callback = NULL;
    data->shutdown_data = NULL;
    data->thread_terminate = 0;
    data->thread_running = 0;

    self->interface->port_add          = port_add;
    self->interface->port_rename       = port_rename;
    self->interface->port_get_buffer   = port_get_buffer;
    self->interface->port_remove       = port_remove;
    self->interface->port_touch        = port_touch;
#ifdef JACK_GET_LATENCY
    self->interface->port_get_latency  = port_get_latency;
#endif
    self->interface->auto_connect      = auto_connect;
    self->interface->get_buffer_size   = get_buffer_size;
    self->interface->get_sample_rate   = get_sample_rate;
    self->interface->add_process       = add_process;
    self->interface->rename_process    = rename_process;
    self->interface->remove_process    = remove_process;
    self->interface->process_exists    = process_exists;
    self->interface->get_position      = get_position;
    self->interface->is_started        = is_started;
    self->interface->start             = start;
    self->interface->stop              = stop;
    self->interface->seek              = seek;
    self->interface->destroy           = destroy;
    self->interface->sync              = sync;
    self->interface->iterate           = iterate;
    self->interface->on_shutdown       = on_shutdown;
    self->interface->activate          = activate;
    self->interface->deactivate        = deactivate;
    self->interface->thread_process    = NULL;

    return self;
}

