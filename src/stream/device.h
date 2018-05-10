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
 *   SVN:$Id: paudio.h 230 2008-08-02 13:46:28Z olivier $
 */

#ifndef JACKBEAT_STREAM_DEVICE_H
#define JACKBEAT_STREAM_DEVICE_H

#include "stream.h"

#define STREAM_DEVICE_NONE "None"
#define STREAM_DEVICE_AUTO "Automatic"
#define STREAM_DEVICE_JACK "JACK"
#define STREAM_DEVICE_PULSE "PulseAudio"

int stream_device_open (stream_t *stream, char *device_name, int sample_rate, 
                        const char *client_name, int auto_start_server);
char ** stream_device_list ();
char * stream_device_get_default ();
char * stream_device_get_name (stream_t *stream);

#endif
