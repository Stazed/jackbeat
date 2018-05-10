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
 *   SVN:$Id: gui.h 232 2008-08-02 17:39:26Z olivier $
 */

#ifndef JACKBEAT_GUI_H
#define JACKBEAT_GUI_H

#include "rc.h"
#include "song.h"
#include "arg.h"
#include "osc.h"
#include "stream/stream.h"

typedef struct gui_t gui_t;

void gui_new(rc_t *rc, arg_t *arg, song_t *song, osc_t *osc, stream_t *stream);

#endif
