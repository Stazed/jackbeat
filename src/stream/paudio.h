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
 *   SVN:$Id: paudio.h 291 2008-09-08 17:25:49Z olivier $
 */

#ifndef JACKBEAT_STREAM_PORTAUDIO_H
#define JACKBEAT_STREAM_PORTAUDIO_H

#include "stream.h"

stream_driver_t * stream_driver_portaudio_new (char *device_name, int sample_rate);
char ** stream_driver_portaudio_list_devices ();
char * stream_driver_portaudio_get_current_device (stream_driver_t *self);

#endif
