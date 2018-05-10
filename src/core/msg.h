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
 *   SVN:$Id: msg.h 279 2008-09-02 11:27:40Z olivier $
 */

#ifndef JACKBEAT_MSG_H
#define JACKBEAT_MSG_H

#define MSG_ACK 1

#include <stdio.h>

typedef struct msg_t msg_t;

typedef struct msg_call_t {
  int feature;
  char params[255];
} msg_call_t;

msg_t *   msg_new (int buffer_size, int item_size);
void      msg_destroy (msg_t *msg);
void      msg_send (msg_t *msg, void *data, int flags);
void      msg_sync(msg_t *msg);
int       msg_receive (msg_t *msg, void *data);
#define   msg_event_fire(M,N,D,S,F) _msg_event_fire (M,N,D,S,F,__LINE__, __func__)
void      _msg_event_fire (msg_t *msg, char *name, void *data, int data_size,
                           void (* free_data) (void *), int line, const char *func);
void      msg_process_events (msg_t * msg, void *source);

#define   msg_call(MSG, FEATURE, FLAGS, FORMAT, ...) \
  { msg_call_t _call; _call.feature = FEATURE; sprintf(_call.params, FORMAT, ## __VA_ARGS__); \
    msg_send (MSG, &_call, FLAGS); }

#endif
