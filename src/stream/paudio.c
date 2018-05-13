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
 *   SVN:$Id: paudio.c 591 2009-06-24 16:49:03Z olivier $
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <portaudio.h>
//#include <dlfcn.h>
#include "paudio.h"
#include "driver.h"
#include "stereo.h"

#define CLASSNAME "PortAudioStreamDriver"
#define CAST(self) stream_driver_cast (self, CLASSNAME)
#define BIND(self, parent, data) \
  stream_driver_t *_self = CAST(self); \
  stream_driver_t *parent = _self->parent; \
  stream_driver_portaudio_data_t *data = (stream_driver_portaudio_data_t *) _self->data; 
#define BIND_DATA(self, data) stream_driver_portaudio_data_t *data = (stream_driver_portaudio_data_t *) (CAST(self)->data)
#define BIND_PARENT(self, parent) stream_driver_t *parent = CAST(self)->parent; 

#undef STAT_FORMATS

typedef struct stream_driver_portaudio_data_t
{
    PaStream  *pa_stream;
    int       sample_rate;
    int       output_latency;
    char      *device_name;
    int       current_device;
} stream_driver_portaudio_data_t;

static int
get_sample_rate (stream_driver_t *self)
{
    BIND_DATA (self, data);
    return data->sample_rate;
}

static void
destroy (stream_driver_t *self)
{
    BIND (self, parent, data);

    if (data->device_name)
        free (data->device_name);
    free (data);

    parent->interface->destroy (self);

    self = CAST (self);
    free (self->classname);
    free (self->interface);
    free (self);
}

static int
process (const void *_input, void *_output, unsigned long nframes,
         const PaStreamCallbackTimeInfo* timeinfo, PaStreamCallbackFlags statusFlags,
         void *_data)
{
    stream_driver_t *self = (stream_driver_t *) _data;
    //BIND_DATA (self, data);
    float *output = (float *) _output;
    int ofs = 0;

    //data->output_latency = (timeinfo->outputBufferDacTime - timeinfo->currentTime) * data->sample_rate;

    self->interface->sync (self);

    while (ofs < nframes)
        ofs += self->interface->iterate (self, nframes - ofs, (void *) (output + ofs));

    return 0;
}

static void
get_device_info (int index, const char **api_name, int *api_type,
                 const char **dev_name, int *max_out_channels)
{
    const PaDeviceInfo *dev_info = Pa_GetDeviceInfo (index);
    const PaHostApiInfo *api_info = Pa_GetHostApiInfo (dev_info->hostApi);
    if (api_name)
        *api_name = api_info->name;
    if (api_type)
        *api_type = api_info->type;
    if (dev_name)
        *dev_name = dev_info->name;
    if (max_out_channels)
        *max_out_channels = dev_info->maxOutputChannels;
}

static char *
get_device_generic_name (int index)
{
    static char generic_name[256];
    const char *api_name, *dev_name;
    get_device_info (index, &api_name, NULL, &dev_name, NULL);
    sprintf (generic_name, "%s - %s", api_name, dev_name);
    return generic_name;
}

static void
portaudio_stats (stream_driver_t *self)
{
    int i, ii = Pa_GetDeviceCount ();
    int max_out;
    DEBUG ("Available Portaudio devices:");
    for (i = 0; i < ii; i++)
    {
        get_device_info (i, NULL, NULL, NULL, &max_out);
        DEBUG ("  %s (%d channels)", get_device_generic_name (i), max_out);

#ifdef STAT_FORMATS
        if (max_out > 0)
        {
            // Testing all formats takes time...
            PaStreamParameters hostApiOutputParameters;
            hostApiOutputParameters.device = i;
            hostApiOutputParameters.channelCount = max_out;
            hostApiOutputParameters.suggestedLatency =
                    Pa_GetDeviceInfo ( hostApiOutputParameters.device )->defaultHighOutputLatency;
            hostApiOutputParameters.hostApiSpecificStreamInfo = NULL;
            const int formats[] = {paInt8, paInt16, paInt24, paInt32, paFloat32, 0};
            const char *formats_str[] = {"8bit", "16bit", "24bit", "32bit", "32bit (float)"};
            const int rates[] = {22050, 44100, 48000, 88200, 96000, 192000, 0};
            int j, k;
            for (j = 0; formats[j]; j++)
            {
                printf ("    %s: ", formats_str[j]);
                hostApiOutputParameters.sampleFormat = formats[j];
                for (k = 0; rates[k]; k++)
                    if (!Pa_IsFormatSupported (NULL, &hostApiOutputParameters, rates[k]))
                        printf ("%d ", rates[k]);
                printf ("\n");
            }
        }
#endif    
    }
    int default_dev = Pa_GetDefaultOutputDevice ();
    if (default_dev != paNoDevice)
        DEBUG ("Portaudio default output device: %s", get_device_generic_name (default_dev));
    else
        DEBUG ("Portaudio default output device: unknown");
}

static PaDeviceIndex
find_output_device (char *generic_name)
{
    int api_type;
    int max_out;
    int i, ii = Pa_GetDeviceCount ();
    if (generic_name)
    {
        for (i = 0; i < ii; i++)
            if (generic_name && !strcmp (get_device_generic_name (i), generic_name))
                return i;
    }
    else
    {
        for (i = 0; i < ii; i++)
        {
            get_device_info (i, NULL, &api_type, NULL, &max_out);
            if (max_out >= 2 && api_type == paALSA)
                return i;
        }
        return Pa_GetDefaultOutputDevice ();
    }
    return paNoDevice;
}

// Function copied and adapted from Portaudio's pa_front.c (Pa_OpenDefaultStream())

static
PaError
open_pa_stream ( PaStream** stream,
                PaDeviceIndex outputDevice,
                int inputChannelCount,
                int outputChannelCount,
                PaSampleFormat sampleFormat,
                double sampleRate,
                unsigned long framesPerBuffer,
                PaStreamCallback *streamCallback,
                void *userData )
{
    PaError result;
    PaStreamParameters hostApiInputParameters, hostApiOutputParameters;
    PaStreamParameters *hostApiInputParametersPtr, *hostApiOutputParametersPtr;

    if ( inputChannelCount > 0 )
    {
        hostApiInputParameters.device = Pa_GetDefaultInputDevice ();
        if ( hostApiInputParameters.device == paNoDevice )
            return paDeviceUnavailable;

        hostApiInputParameters.channelCount = inputChannelCount;
        hostApiInputParameters.sampleFormat = sampleFormat;
        hostApiInputParameters.suggestedLatency =
                Pa_GetDeviceInfo ( hostApiInputParameters.device )->defaultHighInputLatency;
        hostApiInputParameters.hostApiSpecificStreamInfo = NULL;
        hostApiInputParametersPtr = &hostApiInputParameters;
    }
    else
    {
        hostApiInputParametersPtr = NULL;
    }

    const PaHostApiInfo *api_info;
    if ( outputChannelCount > 0 )
    {
        hostApiOutputParameters.device = outputDevice;
        if ( hostApiOutputParameters.device == paNoDevice )
            return paDeviceUnavailable;

        hostApiOutputParameters.channelCount = outputChannelCount;
        hostApiOutputParameters.sampleFormat = sampleFormat;
        hostApiOutputParameters.suggestedLatency =
                Pa_GetDeviceInfo ( hostApiOutputParameters.device )->defaultHighOutputLatency;
#ifdef __WIN32__        
        if (hostApiOutputParameters.suggestedLatency > 0.05)
            hostApiOutputParameters.suggestedLatency = 0.05;
#endif  
        DEBUG ("Default latency (low/high): %f / %f",
               Pa_GetDeviceInfo ( hostApiOutputParameters.device )->defaultLowOutputLatency,
               Pa_GetDeviceInfo ( hostApiOutputParameters.device )->defaultHighOutputLatency);
        hostApiOutputParameters.hostApiSpecificStreamInfo = NULL;
        hostApiOutputParametersPtr = &hostApiOutputParameters;

        const PaDeviceInfo *dev_info = Pa_GetDeviceInfo (hostApiOutputParameters.device);
        api_info = Pa_GetHostApiInfo (dev_info->hostApi);
        DEBUG ("Using output device: %s - %s", api_info->name, dev_info->name);
    }
    else
    {
        hostApiOutputParametersPtr = NULL;
    }

    PaError supported = Pa_IsFormatSupported (hostApiInputParametersPtr, hostApiOutputParametersPtr, sampleRate);
    if (supported == paNoError)
        DEBUG ("Format seems supported");
    else
        DEBUG ("Warning: unsupported format: %s", Pa_GetErrorText (supported));

    result = Pa_OpenStream (
                            stream, hostApiInputParametersPtr, hostApiOutputParametersPtr,
                            sampleRate, framesPerBuffer, paNoFlag, streamCallback, userData );

    /*
    if ((result == paNoError) && !strcmp (api_info->name, "ALSA"))
    {
      DEBUG("Using ALSA, trying to enable realtime..");
      void *handle = dlopen (NULL, RTLD_LAZY);
      if (handle)
      {
        void (* enable_realtime) (PaStream *, int) = NULL;
        enable_realtime = dlsym (handle, "PaAlsa_EnableRealtimeScheduling");
        if (enable_realtime)
        {
          enable_realtime (*stream, 1);
          DEBUG("Done");
        }
        else
          DEBUG("dlsym() failed: %s", dlerror());

        dlclose (handle);          
      }
      else
        DEBUG("dlopen() failed: %s", dlerror());
    }
     */

    return result;
}

static int
activate (stream_driver_t *self)
{
    BIND (self, parent, data);

    if (!parent->interface->activate (self))
        return 0;

    DEBUG ("Connecting to Portaudio");

    if (Pa_Initialize () == paNoError)
    {
        portaudio_stats (self);

        if (Pa_GetSampleSize (paFloat32) == sizeof (float))
        {
            PaDeviceIndex output = find_output_device (data->device_name);
            if (output != paNoDevice)
            {
                PaError err = open_pa_stream (&data->pa_stream, output,
                                              0, 2, paFloat32, data->sample_rate,
                                              paFramesPerBufferUnspecified, process, (void *) self);
                if (!data->device_name)
                {
                    if ((err != paNoError) && (output != Pa_GetDefaultOutputDevice ()))
                    {
                        output = Pa_GetDefaultOutputDevice ();
                        err = open_pa_stream (&data->pa_stream, output,
                                              0, 2, paFloat32, data->sample_rate,
                                              paFramesPerBufferUnspecified, process, (void *) self);
                    }
                }

                if (err == paNoError)
                {
                    const PaStreamInfo *info = Pa_GetStreamInfo (data->pa_stream);
                    data->current_device = output;
                    data->sample_rate = info->sampleRate;
                    data->output_latency = info->outputLatency * data->sample_rate;
                    DEBUG ("Portaudio sample rate: %d (latency: %dms)", data->sample_rate, (int) (info->outputLatency * 1000.0));
                    err = Pa_StartStream (data->pa_stream);
                    if (err == paNoError)
                    {
                        DEBUG ("Success");
                    }
                    else
                    {
                        DEBUG ("Can't start stream");
                        Pa_CloseStream (data->pa_stream);
                        Pa_Terminate ();
                        data->pa_stream = NULL;
                        parent->interface->deactivate (self);
                    }
                }
                else
                {
                    DEBUG ("Can't open stream, error: %s", Pa_GetErrorText (err));
                    Pa_Terminate ();
                    data->pa_stream = NULL;
                    parent->interface->deactivate (self);
                }
            }
            else
            {
                if (data->device_name)
                    DEBUG ("Can't find device: %s", data->device_name);
                else
                    DEBUG ("Can't find default device");

                Pa_Terminate ();
            }
        }
        else
        {
            DEBUG ("Portaudio and internal sample size do not match");
            Pa_Terminate ();
        }
    }
    else
    {
        DEBUG ("Failed to initialize Portaudio");
    }

    return (data->pa_stream != NULL);
}

static int
deactivate (stream_driver_t *self)
{
    BIND (self, parent, data);
    if (data->pa_stream != NULL)
    {
        if (Pa_StopStream (data->pa_stream) == paNoError
            && Pa_CloseStream (data->pa_stream) == paNoError
            && Pa_Terminate () == paNoError)
        {
            DEBUG ("Portaudio brought down");
        }
        else
        {
            DEBUG ("ERROR: failed to shut down Portaudio");
        }
        data->current_device = paNoDevice;
        data->pa_stream = NULL;
        parent->interface->deactivate (self);
    }
    return 1;
}

#ifdef USE_DEPRECIATED
static int
port_get_latency (stream_driver_t *self, stream_driver_port_t *port)
{
    BIND_DATA (self, data);
    if (port->flags & STREAM_OUTPUT)
        return data->output_latency;
    else
        return 0;

}
#endif // USE_DEPRECIATED

stream_driver_t *
stream_driver_portaudio_new (char *device_name, int sample_rate)
{
    stream_driver_t *self = stream_driver_subclass (CLASSNAME, stream_driver_stereo_new ());
    self->data = malloc (sizeof (stream_driver_portaudio_data_t));
    BIND_DATA (self, data);
    data->pa_stream = NULL;
    data->sample_rate = sample_rate > 0 ? sample_rate : 44100;
    data->output_latency = 0;
    data->current_device = paNoDevice;
    if (device_name)
        data->device_name = strdup (device_name);
    else
        data->device_name = NULL;

    self->interface->get_sample_rate   = get_sample_rate;
    self->interface->destroy           = destroy;
    self->interface->activate          = activate;
    self->interface->deactivate        = deactivate;
#ifdef USE_DEPRECIATED
    self->interface->port_get_latency  = port_get_latency;
#endif

    return self;
}

char **
stream_driver_portaudio_list_devices ()
{
    char ** devices = NULL;
    int devcount = 0;
    int api_type;
    int max_out;
    if (Pa_Initialize () == paNoError)
    {
        int i, ii = Pa_GetDeviceCount ();
        for (i = 0; i < ii; i++)
        {
            get_device_info (i, NULL, &api_type, NULL, &max_out);
            // No JACK support by Portaudio, we have our own JACK driver
            if (api_type != paJACK && max_out >= 2)
            {
                devices = realloc (devices, sizeof (char *) * (devcount + 2));
                devices[devcount] = strdup (get_device_generic_name (i));
                devices[devcount + 1] = NULL;
                devcount++;
            }
        }
        Pa_Terminate ();
    }
    return devices;
}

char *
stream_driver_portaudio_get_current_device (stream_driver_t *self)
{
    BIND_DATA (self, data);
    if (data->current_device != paNoDevice)
        return strdup (get_device_generic_name (data->current_device));
    else
        return NULL;
}
