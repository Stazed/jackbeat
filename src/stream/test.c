#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "stream.h"
#include "jack.h"
#include "null.h"
#include "paudio.h"
#include "pulse.h"

#define PI 3.1416

typedef struct test_object_t
{
    stream_t *stream;
    stream_port_t * port1;
    stream_port_t * port2;
    stream_port_t * port3;
    stream_port_t * port4;
    int valid;
    unsigned long print_tick_at;
} test_object_t;

static void
silence (float *buffer, int nframes)
{
    int i;
    for (i = 0; i < nframes; i++)
        buffer[i] = 0;
}

static void
synth (int framerate, float *buffer,
       unsigned long pos, int nframes, float freq, float amp)
{
    int i;
    float period_seconds = 1 / freq;
    float period_nframes = period_seconds * (float) framerate;

    for (i = 0; i < nframes; i++)
        buffer[i] = cos ((float) pos++ * 2 * PI / period_nframes) * amp;
}

static int
process (unsigned long nframes, void *arg)
{
    test_object_t *obj = (test_object_t *) arg;
    if (obj->valid)
    {
        float *buffer1 = stream_port_get_buffer (obj->stream, obj->port1, nframes);
        float *buffer3 = stream_port_get_buffer (obj->stream, obj->port3, nframes);
        float *buffer4 = stream_port_get_buffer (obj->stream, obj->port4, nframes);
        if (stream_is_started (obj->stream))
        {
            int framerate = stream_get_sample_rate (obj->stream);
            unsigned long pos = stream_get_position (obj->stream);

            synth (framerate, buffer1, pos, nframes, 440, 0.3);
            synth (framerate, buffer3, pos, nframes, 110, 0.3);

            int tick_period = framerate * 2;

            int i;
            float period_seconds = 1 / 220.0f;
            float period_nframes = period_seconds * (float) framerate;

            int tick_duration = 50 * period_nframes;

            int latency = stream_port_get_latency (obj->stream, obj->port4);
            for (i = 0; i < nframes; i++)
            {
                int tick_pos = (pos + i) % tick_period;
                if ((tick_pos >= 0) && (tick_pos < tick_duration))
                {
                    if (tick_pos == 0)
                    {
                        printf ("tick in - latency: %d\n", latency);
                        fflush (stdout);
                        obj->print_tick_at = pos + i + latency;
                    }

                    buffer4[i] = sin ((float) tick_pos * 2 * PI / period_nframes) * 0.3;
                }
                else
                    buffer4[i] = 0;

                if (pos + i == obj->print_tick_at)
                {
                    printf ("tick out\n");
                    fflush (stdout);
                }
            }
        }
        else
        {
            silence (buffer1, nframes);
            silence (buffer3, nframes);
            silence (buffer4, nframes);
        }
    }

    return 0;
}

int
main (int argc, char *argv[])
{
    test_object_t *obj = malloc (sizeof (test_object_t));
    obj->valid = 1;
    obj->print_tick_at = -1;
    obj->stream = stream_new ();
    obj->port1 = stream_port_add (obj->stream, "port1", STREAM_OUTPUT | STREAM_LEFT);
    obj->port2 = stream_port_add (obj->stream, "port2", STREAM_OUTPUT | STREAM_RIGHT);
    obj->port3 = stream_port_add (obj->stream, "port3", STREAM_OUTPUT | STREAM_RIGHT | STREAM_LEFT);
    stream_port_remove (obj->stream, obj->port2);
    obj->port4 = stream_port_add (obj->stream, "port4", STREAM_OUTPUT | STREAM_RIGHT);

    test_object_t *junk = malloc (sizeof (test_object_t));
    junk->valid = 0;

    stream_add_process (obj->stream, "junk", process, (void *) junk);
    stream_add_process (obj->stream, "junk", process, (void *) junk); // fails, already exists
    stream_add_process (obj->stream, "synth", process, (void *) obj);
    stream_remove_process (obj->stream, "junk");

    printf ("Commands: connect, start, stop, rewind, quit\n");
    char s[64] = "";
    while (strcmp (s, "quit\n"))
    {
        fgets (s, 64, stdin);
        if (!strcmp (s, "start\n"))
            stream_start (obj->stream);
        else if (!strcmp (s, "stop\n"))
            stream_stop (obj->stream);
        else if (!strcmp (s, "rewind\n"))
            stream_seek (obj->stream, 0);
        else if (!strcmp (s, "connect\n"))
            stream_connect_physical (obj->stream);
#ifdef HAVE_JACK      
        else if (!strcmp (s, "loadjack\n"))
        {
            if (stream_set_driver (obj->stream, stream_driver_jack_new ("streamtest")))
                printf ("success\n");
            else
                printf ("failure\n");
        }
#endif    
        else if (!strcmp (s, "loadnull\n"))
        {
            if (stream_set_driver (obj->stream, stream_driver_null_new ()))
                printf ("success\n");
            else
                printf ("failure\n");
        }
        else if (!strcmp (s, "loadpaudio\n"))
        {
            if (stream_set_driver (obj->stream, stream_driver_portaudio_new ()))
                printf ("success\n");
            else
                printf ("failure\n");
        }
#ifdef HAVE_PULSE
        else if (!strcmp (s, "loadpulse\n"))
        {
            if (stream_set_driver (obj->stream, stream_driver_pulse_new ()))
                printf ("success\n");
            else
                printf ("failure\n");
        }
#endif    

        stream_add_process (obj->stream, "junk", process, junk);
        stream_remove_process (obj->stream, "junk");

        printf ("Commands: connect, start, stop, rewind, quit\n");
    }

    stream_destroy (obj->stream);
    return 0;
}
