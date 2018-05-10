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
 *   SVN:$Id: msg.c 642 2009-11-23 19:35:07Z olivier $
 */

#include <stdlib.h>
#include <unistd.h>
#include <ringbuffer.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>

#include "msg.h"
#include "event.h"
#include "compat.h"

#define DEBUG(M, ...) { printf("MSG  %s(): ", __func__); printf(M, ## __VA_ARGS__); printf("\n"); fflush(stdout); }
#define MSG_EVENT_DATA_MAX_SIZE 64

struct msg_t
{
    ringbuffer_t  *rb_msg, *rb_ack, *rb_event;
    int synced;
    int item_size;
} ;

typedef struct msg_event_t
{
    char  name[32];
    int   x;
    int   y;
    char  copied_data[MSG_EVENT_DATA_MAX_SIZE];
    void  *data_ptr;
    int   data_size;
    void  (* free_data) (void *) ;
} msg_event_t;

msg_t *
msg_new (int buffer_size, int item_size)
{
    msg_t *msg = malloc (sizeof (msg_t));
    msg->rb_msg = ringbuffer_create (buffer_size);
    msg->rb_ack = ringbuffer_create (buffer_size);
    msg->rb_event = ringbuffer_create (buffer_size);
    msg->synced = 1;
    msg->item_size = item_size;
    return msg;
}

void
msg_destroy (msg_t *msg)
{
    ringbuffer_free (msg->rb_msg);
    ringbuffer_free (msg->rb_ack);
    ringbuffer_free (msg->rb_event);
    free (msg);
}

void
msg_send (msg_t *msg, void *data, int flags)
{
    while (ringbuffer_write_space (msg->rb_msg) < msg->item_size + sizeof (int))
        compat_sleep (2);

    ringbuffer_write (msg->rb_msg, (char *) &flags, sizeof (int));
    ringbuffer_write (msg->rb_msg, (char *) data, msg->item_size);

    if (flags & MSG_ACK)
    {
        char ack;
        while (!ringbuffer_read (msg->rb_ack, &ack, 1))
            compat_sleep (2);
    }
}

void
msg_sync (msg_t *msg)
{
    if (!msg->synced)
    {
        char ack = 1;
        msg->synced = ringbuffer_write (msg->rb_ack, &ack, 1) ? 1 : 0;
    }
}

int
msg_receive (msg_t *msg, void *data)
{
    if (msg->synced)
    {
        int flags;
        if (ringbuffer_read_space (msg->rb_msg) >= msg->item_size + sizeof (int))
        {
            ringbuffer_read (msg->rb_msg, (char *) &flags, sizeof (int));
            ringbuffer_read (msg->rb_msg, (char *) data, msg->item_size);

            if (flags & MSG_ACK)
            {
                msg->synced = 0;
                //msg_sync(msg);
            }
            return 1;
        }
    }
    return 0;
}

void
_msg_event_fire (msg_t *msg, char *name, void *data, int data_size, void (*free_data) (void *) ,
                 int line, const char *func)
{
    //DEBUG("from %s() at line %d: event: %s", func, line, name);
    if (data_size <= MSG_EVENT_DATA_MAX_SIZE)
    {
        msg_event_t event;
        strcpy (event.name, name);
        event.data_size = data_size;
        event.free_data = free_data;
        if (free_data)
            memcpy (event.copied_data, data, data_size);
        else
            event.data_ptr = data;
        int size = sizeof (msg_event_t);
        if (ringbuffer_write (msg->rb_event, (char *) &event, size) < size)
        {
            DEBUG ("WARNING: event ringbuffer overrun");
        }
    }
    else
    {
        DEBUG ("ERROR: data size is: %d, which is higher than the maximum: %d", data_size, MSG_EVENT_DATA_MAX_SIZE);
    }
}

void
msg_process_events (msg_t * msg, void *source)
{
    msg_event_t event;
    void *data;
    while (ringbuffer_read (msg->rb_event, (char *) &event, sizeof (msg_event_t)) > 0)
    {
        //DEBUG("Read event %s", event.name);
        if (event.free_data)
        {
            data = malloc (event.data_size);
            memcpy (data, event.copied_data, event.data_size);
        }
        else
        {
            data = event.data_ptr;
        }
        event_fire (source, event.name, data, event.free_data);
    }
}
