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
 *   SVN:$Id: msg.c 121 2008-01-10 00:17:40Z olivier $
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <semaphore.h>

#include "event.h"
#include "array.h"

#define DEBUG(M, ...) { printf("EVT  %s(): ", __func__); printf(M, ## __VA_ARGS__); printf("\n"); }
#define _sem_wait(mutex) DEBUG("lock"); sem_wait (mutex)
#define _sem_post(mutex) DEBUG("unlock"); sem_post (mutex)

typedef struct event_subject_t
{
    void *  source;
    char    name[32];
} event_subject_t;

typedef struct event_call_t
{
    event_t *           event;
    event_callback_t    callback;
    int                 processing;
} event_call_t;

typedef struct event_queue_t
{
    void *          self;
    void **         scope;
    int             scope_num;
    event_call_t ** calls;
    int             calls_num;
    int             processing;
} event_queue_t;

typedef struct event_subscriber_t
{
    void  *           self;
    event_callback_t  callback;
    event_subject_t * subject;
} event_subscriber_t;

typedef struct event_gc_item_t
{
    void *            data;
    void              (* free_data) (void *) ;
} event_gc_item_t;

static sem_t                  event_mutex;
static event_subject_t **     event_subjects = NULL;
static int                    event_subjects_num = 0;
static event_subscriber_t **  event_subscribers = NULL;
static int                    event_subscribers_num = 0;
static event_queue_t **       event_queues = NULL;
static int                    event_queues_num = 0;
static event_gc_item_t **     event_gc_items = NULL;
static int                    event_gc_items_num = 0;

void
event_init ()
{
    sem_init (&event_mutex, 0, 1);
}

void
event_cleanup ()
{
    sem_wait (&event_mutex);
    sem_destroy (&event_mutex);
    ARRAY_DESTROY (event_subjects, event_subjects_num);
    ARRAY_DESTROY (event_subscribers, event_subscribers_num);
    ARRAY_DESTROY (event_queues, event_queues_num);
    ARRAY_DESTROY (event_gc_items, event_gc_items_num);
}

static event_subject_t *
event_subject_find (void * source, char * name)
{
    int i;
    for (i = 0; i < event_subjects_num; i++)
        if (!strcmp (name, event_subjects[i]->name) && (event_subjects[i]->source
                                                        == source))
            return event_subjects[i];
    return NULL;
}

static event_queue_t *
event_queue_find (void * self)
{
    int i;
    for (i = 0; i < event_queues_num; i++)
        if (event_queues[i]->self == self)
            return event_queues[i];
    return NULL;
}

static void
event_gc_add (void *data, void (* free_data) (void *) )
{
    if (free_data)
    {
        int i, found = 0;
        for (i = 0; i < event_gc_items_num; i++)
            if (event_gc_items[i]->data == data)
            {
                found = 1;
                break;
            }
        if (!found)
        {
            event_gc_item_t *item = malloc (sizeof (event_gc_item_t));
            item->data = data;
            item->free_data = free_data;
            ARRAY_ADD (event_gc_item_t, event_gc_items, event_gc_items_num, item);
        }
    }
}

static void
event_gc ()
{
    int i, j, k;
    for (i = 0; i < event_gc_items_num; i++)
    {
        int used = 0;
        for (j = 0; !used && (j < event_queues_num); j++)
            for (k = 0; !used && (k < event_queues[j]->calls_num); k++)
                if (event_queues[j]->calls[k]->event->data == event_gc_items[i]->data)
                    used = 1;
        if (!used)
        {
            event_gc_item_t *item = event_gc_items[i];
            item->free_data (item->data);
            free (item);
            ARRAY_REMOVE (event_gc_item_t, event_gc_items, event_gc_items_num, item);
            if (i > 0) i--;
        }
    }
}

void
_event_fire (const char *caller, void *source, char *name, void *data, void (* free_data) (void *) )
{
    sem_wait (&event_mutex);
    int i, j;
    //DEBUG("Event '%s' fired by %s()", name, caller);
    event_subject_t *subject = event_subject_find (source, name);
    if (subject == NULL)
    {
        DEBUG ("Warning: unregistered event '%s' fired by %s()", name, caller);
    }
    else
    {
        for (i = 0; i < event_subscribers_num; i++)
        {
            event_subscriber_t *subscriber = event_subscribers[i];
            if (subscriber->subject == subject)
            {
                event_t *event = malloc (sizeof (event_t));
                strcpy (event->name, subject->name);
                event->self = subscriber->self;
                event->source = subject->source;
                event->data = data;
                event_queue_t *queue = event_queue_find (subscriber->self);

                int in_scope = 0;
                if (queue)
                    for (j = 0; j < queue->scope_num; j++)
                        if (queue->scope[j] == source)
                        {
                            //DEBUG("event %s in scope", event->name);
                            in_scope = 1;
                            break;
                        }

                if (!queue || in_scope)
                {
                    // DEBUG("Notify %s", event->name);
                    sem_post (&event_mutex);
                    subscriber->callback (event); // event_subscribers can change here
                    sem_wait (&event_mutex);
                    free (event);
                }
                else
                {
                    // DEBUG("On queue %s", event->name);
                    event_call_t *call = malloc (sizeof (event_call_t));
                    call->event = event;
                    call->callback = subscriber->callback;
                    call->processing = 0;
                    ARRAY_ADD (event_call_t, queue->calls, queue->calls_num, call);
                }
            }
        }

        if (free_data)
            event_gc_add (data, free_data);

        event_gc ();
    }
    sem_post (&event_mutex);
}

void
_event_subscribe (const char *caller, void *source, char *name, void *self, event_callback_t callback)
{
    sem_wait (&event_mutex);
    event_subject_t *subject = event_subject_find (source, name);
    if (subject == NULL)
    {
        DEBUG ("Warning: unregistered event '%s' subscribed by %s()", name, caller);
    }
    else
    {
        int subscribed = 0, i;
        for (i = 0; i < event_subscribers_num; i++)
        {
            event_subscriber_t *subscriber = event_subscribers[i];
            if ((subscriber->subject == subject) && (subscriber->self == self))
            {
                subscribed = 1;
                DEBUG ("Warning: from %s(): object %p already subscribed to event '%s' (source: %p)",
                       caller, self, subject->name, subject->source);
                //FIXME: should allow subscribing several times
                break;
            }
        }
        if (!subscribed)
        {
            event_subscriber_t *subscriber = malloc (sizeof (event_subscriber_t));
            subscriber->subject = subject;
            subscriber->callback = callback;
            subscriber->self = self;
            ARRAY_ADD (event_subscriber_t, event_subscribers, event_subscribers_num,
                       subscriber);
        }
    }
    sem_post (&event_mutex);
}

void
_event_register (const char *caller, void *source, char *name)
{
    sem_wait (&event_mutex);
    if (event_subject_find (source, name) != NULL)
    {
        DEBUG ("Warning: from %s(): event '%s' (source: %p) is already registered", caller, name,
               source);
    }
    else
    {
        event_subject_t *subject = malloc (sizeof (event_subject_t));
        strcpy (subject->name, name);
        subject->source = source;
        ARRAY_ADD (event_subject_t, event_subjects, event_subjects_num,
                   subject);
    }
    sem_post (&event_mutex);
}

void
_event_remove_source (const char *caller, void *source)
{
    sem_wait (&event_mutex);
    int i, j;
    event_subject_t *subject = NULL;
    int source_found = 0;
    for (i = 0; i < event_subjects_num; i++)
    {
        while ((i < event_subjects_num) && (event_subjects[i]->source == source))
        {
            source_found = 1;
            subject = event_subjects[i];
            for (j = 0; j < event_subscribers_num; j++)
            {
                while ((j < event_subscribers_num)
                       && (event_subscribers[j]->subject == subject))
                {
                    event_subscriber_t *subscriber = event_subscribers[j];
                    ARRAY_REMOVE (event_subscriber_t, event_subscribers,
                                  event_subscribers_num, subscriber);
                    free (subscriber);
                }
            }
            ARRAY_REMOVE (event_subject_t, event_subjects,
                          event_subjects_num, subject);
            free (subject);
        }
    }
    if (!source_found)
    {
        DEBUG ("Trying to remove unknwown source %p from %s()", source, caller);
    }
    for (i = 0; i < event_queues_num; i++)
        for (j = 0; j < event_queues[i]->calls_num; j++)
        {
            event_call_t *call = event_queues[i]->calls[j];
            if (call->event->source == source)
            {
                ARRAY_REMOVE (event_call_t, event_queues[i]->calls, event_queues[i]->calls_num, call);
                free (call->event);
                free (call);
                if (j > 0)
                    j--;
            }
        }
    event_gc ();
    sem_post (&event_mutex);
}

void
event_enable_queue (void *self)
{
    sem_wait (&event_mutex);
    if (event_queue_find (self) == NULL)
    {
        event_queue_t *queue = malloc (sizeof (event_queue_t));
        queue->self = self;
        queue->calls_num = 0;
        queue->calls = NULL;
        queue->scope_num = 0;
        queue->scope = NULL;
        queue->processing = 0;
        ARRAY_ADD (event_queue_t, event_queues, event_queues_num, queue);
    }
    sem_post (&event_mutex);
}

static void
event_do_disable_queue (void *self)
{
    event_queue_t *queue = event_queue_find (self);
    if (queue != NULL)
    {
        while (queue->calls_num > 0)
        {
            event_call_t *call = queue->calls[0];
            ARRAY_REMOVE (event_call_t, queue->calls, queue->calls_num, call);
            free (call->event);
            free (call);
        }
        ARRAY_REMOVE (event_queue_t, event_queues, event_queues_num, queue);
        free (queue->scope);
        free (queue);
        event_gc ();
    }
}

void
event_disable_queue (void *self)
{
    sem_wait (&event_mutex);
    event_do_disable_queue (self);
    sem_post (&event_mutex);
}

void
event_process_queue (void *self)
{
    sem_wait (&event_mutex);
    event_queue_t *queue = event_queue_find (self);
    if (queue && !queue->processing)
    {
        queue->processing = 1;
        while (queue->calls_num > 0)
        {
            event_call_t *call = queue->calls[0];
            if (!call->processing)
            {
                call->processing = 1;
                sem_post (&event_mutex);
                call->callback (call->event); // may the queue change here ?
                sem_wait (&event_mutex);
                if ((queue = event_queue_find (self)))
                { // The queue may be gone here
                    ARRAY_REMOVE (event_call_t, queue->calls, queue->calls_num, call);
                    free (call->event);
                    free (call);
                }
                else
                {
                    break;
                }
            }
        }
        event_gc ();
        if (queue)
            queue->processing = 0;
    }
    sem_post (&event_mutex);
}

void
event_scope_add (void *self, void *source)
{
    sem_wait (&event_mutex);
    event_queue_t *queue = event_queue_find (self);
    if (queue != NULL)
    {
        ARRAY_ADD (void, queue->scope, queue->scope_num, source);
    }
    sem_post (&event_mutex);
}

void
event_unsubscribe_all (void *self)
{
    sem_wait (&event_mutex);
    event_do_disable_queue (self);
    int j;
    for (j = 0; j < event_subscribers_num; j++)
    {
        while ((j < event_subscribers_num)
               && (event_subscribers[j]->self == self))
        {
            event_subscriber_t *subscriber = event_subscribers[j];
            ARRAY_REMOVE (event_subscriber_t, event_subscribers,
                          event_subscribers_num, subscriber);
            free (subscriber);
        }
    }

    sem_post (&event_mutex);
}

int
event_has_subscribers (void *source, char *name)
{
    int i, ret = 0;
    sem_wait (&event_mutex);

    event_subject_t *subject = event_subject_find (source, name);
    if (subject == NULL)
    {
        DEBUG ("Warning: no such event: '%s'", name);
    }
    else
    {
        for (i = 0; i < event_subscribers_num; i++)
        {
            event_subscriber_t *subscriber = event_subscribers[i];
            if (subscriber->subject == subject)
                ret = 1;
            break;
        }
    }
    sem_post (&event_mutex);
    return ret;
}
