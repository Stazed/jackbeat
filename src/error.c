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
 *   SVN:$Id: error.c 528 2009-04-19 20:32:47Z olivier $
 */
#include <string.h>

#include "error.h"

char *
error_to_string (int code)
{
    char *error_string;
    switch (code)
    {
        case ERR_SEQUENCE_JACK_CONNECT:
            error_string = strdup ("The JACK server is not running, and could not be "
                                   "automatically started. Please check that the audio "
                                   "device isn't already used by another application, "
                                   "and verify your ALSA/JACK settings.");
            break;
        case ERR_SEQUENCE_JACK_ACTIVATE:
            error_string = strdup ("There was an error activating the JACK client. "
                                   "The JACK server may be in an inconsistent state, "
                                   "please check its configuration or try to upgrade "
                                   "to a newer version. You should also check your "
                                   "ALSA settings.");
            break;
        case ERR_SEQUENCE_JACK_REGPORT:
            error_string = strdup ("Unable to register a new JACK Port. You might need "
                                   "to increase the JACK server maximum number of ports.");
            break;
        case ERR_JAB_XML:
            error_string = strdup ("The XML definition of your .jab file seems "
                                   "corrupted. The operation can't complete.");
            break;
        case ERR_JAB_EXTRACT:
            error_string = strdup ("Extraction failed");
            break;
        case ERR_JAB_VERSION:
            error_string = strdup ("This .jab file was created with a newer version of Jackbeat. "
                                   "Your version is too old. Please upgrade.");
            break;
        case ERR_SEQUENCE_DUPLICATE_TRACK_NAME:
            error_string = strdup ("A track with this name already exists. Please provide another name.");
            break;
        case ERR_SEQUENCE_INVALID_NAME:
            error_string = strdup ("Malformed name. Please only use letters, digits, underscores, dashes, "
                                   "plus signs and points.");
            break;
        case ERR_SEQUENCE_INTERNAL:
        case ERR_INTERNAL:
        default:
            error_string = strdup ("Sorry, an internal error occured. The operation can't "
                                   "complete.");
            break;
    }
    return error_string;
}
