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
 *   SVN:$Id: null.c 591 2009-06-24 16:49:03Z olivier $
 */

#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include "null.h"
#include "driver.h"
#include "core/compat.h"

#define CLASSNAME "NullStreamDriver"
#define CAST(self) stream_driver_cast (self, CLASSNAME)
#define BIND_PARENT(self, parent) stream_driver_t *parent = CAST(self)->parent; 

static void
destroy (stream_driver_t *self)
{
    BIND_PARENT (self, parent);

    parent->interface->destroy (self);

    self = CAST (self);
    free (self->classname);
    free (self->interface);
    free (self);
}

static void
thread_process (stream_driver_t *self)
{
    // FIXME: processing 0 frames may be lighter, but could cause a
    // division by zero somewhere in external code
    self->interface->sync (self);
    self->interface->iterate (self, STREAM_BUFFER_SIZE);
    int rate = self->interface->get_sample_rate (self);
    compat_sleep (1000.0 / ((float) rate / STREAM_BUFFER_SIZE));
}

stream_driver_t *
stream_driver_null_new (char *client_name)
{
    stream_driver_t *self = stream_driver_subclass (CLASSNAME, stream_driver_new ());

    self->interface->destroy    = destroy;
    self->interface->thread_process = thread_process;

    return self;
}
