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
 *   SVN:$Id: paudio.c 246 2008-08-15 12:41:45Z olivier $
 */

#include <string.h>
#include <stdlib.h>
#include "device.h"
#include "config.h"
#include "jack.h"
#include "paudio.h"
#include "pulse.h"


int 
stream_device_open (stream_t *stream, char *device_name, int sample_rate, 
                    const char *client_name, int auto_start_server)
{
  int success = 0;
  if (!strcmp (device_name, STREAM_DEVICE_AUTO))
  {
    success =
#ifdef HAVE_JACK
        stream_set_driver (stream, stream_driver_jack_new (client_name, 0)) ||
#endif    
        stream_set_driver (stream, stream_driver_portaudio_new (NULL, -1));
  }      
#ifdef HAVE_JACK
  else if (!strcmp (device_name, STREAM_DEVICE_JACK))
    success = stream_set_driver (stream, stream_driver_jack_new (client_name, auto_start_server));
#endif    
#ifdef HAVE_PULSE
  else if (!strcmp (device_name, STREAM_DEVICE_PULSE))
    success = stream_set_driver (stream, stream_driver_pulse_new (client_name));
#endif    
  else
    success = stream_set_driver (stream, stream_driver_portaudio_new (device_name, sample_rate));

  return success;    
}    

char **
stream_device_list ()
{
  char ** devices = malloc (sizeof (char *) * 2);
  devices[0] = strdup (STREAM_DEVICE_AUTO);
  devices[1] = NULL;
  int devcount = 1;

#ifdef HAVE_JACK
  devices = realloc (devices, sizeof (char *) * (devcount + 2));
  devices[devcount] = strdup (STREAM_DEVICE_JACK);
  devices[++devcount] = NULL;
#endif    
#ifdef HAVE_PULSE
  devices = realloc (devices, sizeof (char *) * (devcount + 2));
  devices[devcount] = strdup (STREAM_DEVICE_PULSE);
  devices[++devcount] = NULL;
#endif    

  char ** pa_devices = stream_driver_portaudio_list_devices ();
  if (pa_devices)
  {
    int i;
    for (i = 0; pa_devices[i] != NULL; i++)
    {
      devices = realloc (devices, sizeof (char *) * (devcount + 2));
      devices[devcount] = pa_devices[i];
      devices[++devcount] = NULL;
    }
    free (pa_devices);
  }    
   
  return devices;
}

char * 
stream_device_get_default ()
{
  static char device[32] = STREAM_DEVICE_AUTO;
  return device;
}

char *
stream_device_get_name (stream_t *stream)
{
  char *name = NULL;
  const char *classname = stream_get_driver_classname (stream);  
  if (classname)
  {
    if (!strcmp (classname, "JackStreamDriver"))
      name = strdup (STREAM_DEVICE_JACK);
    else if (!strcmp (classname, "PulseAudioStreamDriver"))
      name = strdup (STREAM_DEVICE_PULSE);
    else if (!strcmp (classname, "PortAudioStreamDriver"))
      name = stream_driver_portaudio_get_current_device (stream_get_driver (stream));
  }      
    
  return name ? name : strdup (STREAM_DEVICE_NONE);  
}
