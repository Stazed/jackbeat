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
 *   SVN:$Id: msg.h 121 2008-01-10 00:17:40Z olivier $
 */

#ifndef JACKBEAT_EVENT_H
#define JACKBEAT_EVENT_H

#include <stdarg.h>

typedef struct event_t
{
  void *            self;
  void *            source;
  char              name[32];
  void *            data;
} event_t;

typedef void(* event_callback_t) (event_t *event);

void      event_init ();
void      event_cleanup ();
#define   event_register(source, name) _event_register (__func__, source, name) 
void      _event_register (const char *caller, void *source, char *name);
#define   event_fire(source, name, data, free_data) _event_fire (__func__, source, name, data, free_data)
void      _event_fire (const char *caller, void *source, char *name, void *data, void (* free_data) (void *));
#define   event_subscribe(source, name, self, callback) _event_subscribe (__func__, source, name, self, callback)
void      _event_subscribe (const char *caller, void *source, char *name, void *self, event_callback_t callback);
#define   event_remove_source(source) _event_remove_source (__func__, source)
void      _event_remove_source (const char *caller, void *source);
void      event_enable_queue (void *self);
void      event_disable_queue (void *self);
void      event_process_queue (void *self);
void      event_unsubscribe_all (void *self);
void      event_scope_add (void *self, void *source);
int       event_has_subscribers (void *source, char *name);



#endif
