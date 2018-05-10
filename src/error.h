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
 *   SVN:$Id: error.h 528 2009-04-19 20:32:47Z olivier $
 */

#ifndef JACKBEAT_ERR_H
#define JACKBEAT_ERR_H

enum {
  ERR_JAB_XML = 1,
  ERR_JAB_VERSION,
  ERR_JAB_EXTRACT,
  ERR_SEQUENCE_INTERNAL,
  ERR_SEQUENCE_JACK_CONNECT,  
  ERR_SEQUENCE_JACK_ACTIVATE, 
  ERR_SEQUENCE_JACK_REGPORT,
  ERR_SEQUENCE_DUPLICATE_TRACK_NAME,
  ERR_SEQUENCE_INVALID_NAME,
  ERR_INTERNAL
};  

char * error_to_string (int code);

#endif
