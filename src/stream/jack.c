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
 *   SVN:$Id: jack.c 294 2008-09-08 23:20:07Z olivier $
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <jack/jack.h>
#include "jack.h"
#include "driver.h"
#include "../gui.h"

#define CLASSNAME "JackStreamDriver"
#define CAST(self) stream_driver_cast (self, CLASSNAME)
#define BIND(self, parent, data) \
  stream_driver_t *_self = CAST(self); \
  stream_driver_t *parent = _self->parent; \
  stream_driver_jack_data_t *data = (stream_driver_jack_data_t *) _self->data; 
#define BIND_DATA(self, data) stream_driver_jack_data_t *data = (stream_driver_jack_data_t *) (CAST(self)->data)
#define BIND_PARENT(self, parent) stream_driver_t *parent = CAST(self)->parent; 
#define PORTDATA(port) ((jack_port_t *) port->data)

typedef struct stream_driver_jack_data_t
{
    jack_client_t * client;
    char *          client_name;
    int volatile    process_called;
    unsigned long   position;
    int             auto_start;
    int             auto_connect;
} stream_driver_jack_data_t;

static jack_port_t *
port_register (stream_driver_t *self, char *name, stream_port_flags_t flags)
{
    BIND_DATA (self, data);
    int _flags = (flags & STREAM_INPUT) ? JackPortIsInput : JackPortIsOutput;

    jack_port_t *jack_port = jack_port_register (data->client, name,
                                                 JACK_DEFAULT_AUDIO_TYPE, _flags, 0);
    if (jack_port)
        DEBUG ("Registered jack port: %s", name);
    else
        DEBUG ("Failed to register jack port: %s", name);
    return jack_port;
}

static void
port_connect_physical (stream_driver_t *self, stream_driver_port_t *port)
{
    BIND_DATA (self, data);
    const char **out_ports;

    if (!data->client)
        return;

    const char **connections = jack_port_get_connections (PORTDATA (port));
    if (!connections)
    {
        if ((out_ports = jack_get_ports (data->client, NULL, NULL,
                                         JackPortIsPhysical | JackPortIsInput)))
        {
            // Left channel:
            if (out_ports[0])
            {
                if (port->flags & STREAM_LEFT)
                    jack_connect (data->client, jack_port_name (PORTDATA (port)),
                                  out_ports[0]);
                // Right channel:
                if (out_ports[1])
                {
                    if (port->flags & STREAM_RIGHT)
                        jack_connect (data->client, jack_port_name (PORTDATA (port)),
                                      out_ports[1]);
                }
                    // Mono (connect right to unique output port):
                else
                {
                    if (port->flags & STREAM_RIGHT)
                        jack_connect (data->client, jack_port_name (PORTDATA (port)),
                                      out_ports[0]);
                }
            }

            free (out_ports);
        }
    }
}

static stream_driver_port_t *
port_add (stream_driver_t *self, char *name, stream_port_flags_t flags, void *port_data)
{
    BIND (self, parent, data);
    stream_driver_port_t *port = NULL;
    if (data->client)
    {
        jack_port_t *jack_port = port_register (self, name, flags);
        if (jack_port)
            port = parent->interface->port_add (self, name, flags, (void *) jack_port);

        if (data->auto_connect)
            port_connect_physical (self, port);
    }
    return port;
}

static void
port_touch (stream_driver_t *self, stream_driver_port_t *port)
{
    BIND (self, parent, data);
    parent->interface->port_touch (self, port);
    if (data->auto_connect)
        port_connect_physical (self, port);
}

static void
port_rename (stream_driver_t *self, stream_driver_port_t *port, char *name)
{
    BIND (self, parent, data);
    parent->interface->port_rename (self, port, name);
    if (data->client && PORTDATA (port))
//        jack_port_set_name (PORTDATA (port), name); // Depreciated in newer versions but some distro's still use older version
        jack_port_rename (data->client, PORTDATA (port), name);   // The newer version - not all support this
}

static void
port_remove (stream_driver_t *self, stream_driver_port_t *port)
{
    BIND (self, parent, data);
    jack_port_t *jack_port = PORTDATA (port);

    parent->interface->port_remove (self, port);

    if (data->client && jack_port)
        jack_port_unregister (data->client, jack_port);
}

#ifdef JACK_GET_LATENCY
static int
port_get_latency (stream_driver_t *self, stream_driver_port_t *port)
{
    /*
     * 
    void jack_port_get_latency_range (jack_port_t *port, jack_latency_callback_mode_t mode, jack_latency_range_t *range);
    */
    BIND (self, parent, data);
    if (data->client)
        return jack_port_get_total_latency (data->client, PORTDATA (port));
    else
        return parent->interface->port_get_latency (self, port);
}
#endif // JACK_GET_LATENCY

static void
auto_connect (stream_driver_t *self, int active)
{
    BIND (self, parent, data);
    parent->interface->auto_connect (self, active);
    data->auto_connect = active;
    /*
    if (!data->client)
      return;

    stream_driver_port_t **ports = stream_driver_get_ports(self);
    int nports = stream_driver_get_ports_num(self);
    int i;

    parent->interface->connect_physical (self);

    for (i = 0; i < nports; i++)
      self->interface->port_connect_physical (self, ports[i]);
     */
}

static int
get_sample_rate (stream_driver_t *self)
{
    BIND (self, parent, data);
    if (data->client)
        return jack_get_sample_rate (data->client);
    else
        return parent->interface->get_sample_rate (self);
}

static unsigned long
get_position (stream_driver_t *self)
{
    BIND_DATA (self, data);
    return data->position;
}

static int
is_started (stream_driver_t *self)
{
    BIND_DATA (self, data);
    if (data->client)
        return (jack_transport_query (data->client, NULL) == JackTransportRolling);
    else
        return 0;
}

static void
start (stream_driver_t *self)
{
    BIND_DATA (self, data);
    if (data->client)
        jack_transport_start (data->client);
}

static void
stop (stream_driver_t *self)
{
    BIND_DATA (self, data);
    if (data->client)
        jack_transport_stop (data->client);
}

static void
seek (stream_driver_t *self, unsigned long position)
{
    BIND_DATA (self, data);
    if (data->client)
        jack_transport_locate (data->client, position);
}

static void
destroy (stream_driver_t *self)
{
    BIND (self, parent, data);

    free (data->client_name);
    free (data);

    parent->interface->destroy (self);

    self = CAST (self);
    free (self->classname);
    free (self->interface);
    free (self);
}

static void
shutdown (void *arg)
{
    stream_driver_t *self = (stream_driver_t *) arg;
    BIND_DATA (self, data);
    data->client = NULL;
    self->interface->on_shutdown (self);
}

static int
process (jack_nframes_t nframes, void *_data)
{
    stream_driver_t *self = (stream_driver_t *) _data;
    BIND_DATA (self, data);

    self->interface->sync (self);

    if (data->client)
    {
        jack_position_t info;
        
        jack_transport_state_t state = jack_transport_query (data->client, &info);
        if(jack_transport)
        {
            if(state == JackTransportRolling )
            {
                start_sequence();
            }
        }
        
        data->position = info.frame;

        stream_driver_port_t **ports = stream_driver_get_ports (self);
        int nports = stream_driver_get_ports_num (self);
        float *jack_buffer;

        int j, ofs, len;
        for (ofs = 0; ofs < nframes; ofs += len)
        {
            if ((len = self->interface->iterate (self, nframes - ofs)))
            {
                for (j = 0; j < nports; j++)
                {
                    jack_buffer = jack_port_get_buffer (PORTDATA (ports[j]), nframes);
                    memcpy (jack_buffer + ofs, ports[j]->buffer, len * sizeof (float));
                }
                data->position += len;
            }
        }
    }

    data->process_called = 1;
    return 0;
}

static int
activate (stream_driver_t *self)
{
    //FIXME: trigger ERR_SEQUENCE_JACK_ACTIVATE and ERR_SEQUENCE_JACK_CONNECT ?
    BIND (self, parent, data);
    jack_status_t status;
    int activated = 0, success = 0, i;

    if (!parent->interface->activate (self))
        return 0;

    DEBUG ("Connecting to JACK");
    int options = data->auto_start ? 0 : JackNoStartServer;
    data->client = jack_client_open (data->client_name, options, &status);

    if (data->client != NULL)
    {
        jack_on_shutdown (data->client, shutdown, (void *) self);
        jack_set_process_callback (data->client, process, (void *) self);

        // Re-registering jack ports in case of reactivation
        stream_driver_port_t **ports = stream_driver_get_ports (self);
        int nports = stream_driver_get_ports_num (self);
        int ok = 1;
        for (i = 0; i < nports; i++)
        {
            ports[i]->data = (void *) port_register (self, ports[i]->name, ports[i]->flags);
            if (ports[i]->data == NULL)
            {
                ok = 0;
                break;
            }
        }

        if (ok)
        {
            data->process_called = 0;

            int errcode = jack_activate (data->client);
            if (!errcode)
            {
                activated = 1;
                // Checking that the process callback is really called by Jack:
                for (i = 0; (i < 1500) && !data->process_called; i++)
                    usleep (1000);
                if (data->process_called)
                {
                    success = 1;
                    DEBUG ("Success");
                }
                else
                    DEBUG ("ERROR: process callback isn't called by Jack (waited 1.5s)");
            }
            else
                DEBUG ("Can't activate JACK client ; error code: %d", errcode);
        }
        else
            DEBUG ("ERROR: unable to register jack ports after reactivation");
    }
    else
        DEBUG ("Can't register as a JACK client");

    if (!success)
    {
        if (activated)
            jack_deactivate (data->client);
        if (data->client != NULL)
            jack_client_close (data->client);
        data->client = NULL;
        parent->interface->deactivate (self);
    }

    return (data->client != NULL);
}

static int
deactivate (stream_driver_t *self)
{
    BIND (self, parent, data);
    if (data->client != NULL)
    {
        jack_deactivate (data->client);
        jack_client_close (data->client);
        parent->interface->deactivate (self);
    }
    return 1;
}

stream_driver_t *
stream_driver_jack_new (const char *client_name, int auto_start)
{


    stream_driver_t *self = stream_driver_subclass (CLASSNAME, stream_driver_new ());
    self->data = malloc (sizeof (stream_driver_jack_data_t));
    BIND_DATA (self, data);
    data->position = 0;
    data->client_name = strdup (client_name);
    data->auto_start = auto_start;
    data->auto_connect = 0;

    self->interface->port_add          = port_add;
    self->interface->port_rename       = port_rename;
    self->interface->port_remove       = port_remove;
    self->interface->port_touch        = port_touch;
#ifdef JACK_GET_LATENCY
    self->interface->port_get_latency  = port_get_latency;
#endif
    self->interface->auto_connect      = auto_connect;
    self->interface->get_sample_rate   = get_sample_rate;
    self->interface->get_position      = get_position;
    self->interface->is_started        = is_started;
    self->interface->start             = start;
    self->interface->stop              = stop;
    self->interface->seek              = seek;
    self->interface->destroy           = destroy;
    self->interface->activate          = activate;
    self->interface->deactivate        = deactivate;

    return self;
}
