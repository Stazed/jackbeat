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
 *   SVN:$Id: main.c 682 2010-09-08 14:28:19Z olivier $
 */

#define _GNU_SOURCE // For REG_RIP from ucontext.h

#include <string.h>
#include <config.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_GTK_QUARTZ
#include <Carbon.h>
#endif
#ifdef __WIN32__
#define _WIN32_WINNT 0x0501
#include <windows.h>
#endif

#include "song.h"
#include "gui.h"
#include "arg.h"
#include "osc.h"
#include "core/event.h"
#include "core/pool.h"
#include "stream/stream.h"
#include "stream/device.h"
#include "util.h"

#ifdef MEMDEBUG
#include "memdebug.h"
#endif

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#include <signal.h>

#define DEBUG(M, ...) { printf("MAI  %s(): ", __func__); printf(M, ## __VA_ARGS__); printf("\n"); }

#ifdef HAVE_EXECINFO_H
#ifdef HAVE_UCONTEXT_H

#include <execinfo.h>

/* get REG_EIP from ucontext.h */
#include <ucontext.h>

void print_backtrace(void **trace, int size) {
    printf("CAUGHT SIGSEGV >> DUMPING BACKTRACE\n");

    backtrace_symbols_fd(trace + 1, size - 1, STDOUT_FILENO);

    printf("#####################################################################\n");
    printf("#                             KRASH !!!                             #\n");
    printf("#                                                                   #\n");
    printf("#  Please help fixing this issue by describing what you were doing  #\n");
    printf("#    and attaching the above console output in a new ticket at:     #\n");
    printf("#              http://jackbeat.samalyse.org/newticket               #\n");
    printf("#                                                                   #\n");
    printf("#####################################################################\n\n");

    exit(0);
}

void handle_sigsegv_siginfo(int sig, siginfo_t *info, void *secret) {
    void *trace[200];
    int trace_size = 0;
    ucontext_t *uc = (ucontext_t *)secret;

    trace_size = backtrace(trace, 200);

#ifndef __APPLE__

/* overwrite sigaction with caller's address */
#ifdef __FreeBSD__

#if defined(__i386__)
    trace[1] = (void *) uc->uc_mcontext.mc_eip;
#elif defined(__x86_64__)
    trace[1] = (void *) uc->uc_mcontext.mc_rip;
#endif

#else /* __FreeBSD__ */

#if defined(__i386__)
    trace[1] = (void *) uc->uc_mcontext.gregs[REG_EIP];
#elif defined(__x86_64__)
    trace[1] = (void *) uc->uc_mcontext.gregs[REG_RIP];
#endif

#endif /* __FreeBSD__ */
#endif /* ! __APPLE__ */

    print_backtrace(trace, trace_size);
}

void handle_sigsegv(int sig) {
    void *trace[200];
    int trace_size = 0;

    trace_size = backtrace(trace, 200);
    print_backtrace(trace, trace_size);
}

#endif /* HAVE_UCONTEXT_H */
#endif /* HAVE_EXECINFO_H */

#ifdef HAVE_GTK_QUARTZ
static pascal OSErr
osx_desktop_open_handler (const AppleEvent *event, AppleEvent *reply, long refCon)
{
    AEDescList docs;
    DEBUG("Received Mac OS Open event");
    if (AEGetParamDesc(event, keyDirectObject, typeAEList,
                       &docs) == noErr) {
        long n = 0;
        int i;
        AECountItems(&docs, &n);
        DEBUG("About to open %ld file(s)", n);
        UInt8 strBuffer[256];
        for (i = 0; i < n; i++) {
            FSRef ref;
            if (AEGetNthPtr(&docs, i + 1, typeFSRef, 0, 0,
                            &ref, sizeof(ref), 0)
                    != noErr)
                continue;
            if (FSRefMakePath(&ref, strBuffer, 256)
                    == noErr) {
                DEBUG("Sending desktop open event for: %s\n", (char *) strBuffer);
                event_fire(NULL, "desktop-open-action", strdup((char *) strBuffer),
                           free);
            }
        }
     }
     return noErr;
}
#endif

int
main (int argc, char *argv[])
{
#ifdef HAVE_EXECINFO_H
#ifdef HAVE_UCONTEXT_H
    struct sigaction sa;

#ifdef SA_SIGINFO
    sa.sa_sigaction = (void *) handle_sigsegv_siginfo;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
#else
    sa.sa_sigaction = (void *) handle_sigsegv;
    sa.sa_flags = SA_RESTART;
#endif /* SA_SIGINFO  */

    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
#endif /* HAVE_UCONTEXT_H */
#endif /* HAVE_EXECINFO_H */

#ifdef __WIN32__
  if (AttachConsole(ATTACH_PARENT_PROCESS)) {
      freopen("CONOUT$", "w", stdout);
  }
#endif

  util_init_paths();

  DEBUG("Initializing event manager");
  event_init ();
  event_enable_queue(NULL);
  event_register (NULL, "desktop-open-action");

#ifdef HAVE_GTK_QUARTZ
  OSStatus err = AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments,
                                       NewAEEventHandlerUPP(osx_desktop_open_handler), 
                                       0, false);
  if (err) {
      printf("Can't install Mac OS Open event handler\n");
  }
#endif

  DEBUG ("Loading RC settings");
  rc_t rc;  
  rc_read (&rc);

  DEBUG ("Parsing arguments");
  arg_t *arg = arg_parse (argc, argv);
 
  DEBUG ("Creating threads pool");
  pool_t *pool = pool_new (4);

  DEBUG ("Creating song");
  song_t *song = song_new (pool);

  DEBUG ("Bringing OSC up");
  osc_t *osc = osc_new (song);

  DEBUG ("Creating audio stream");
  char *client_name = arg->client_name ? arg->client_name : rc.client_name;
  stream_t *stream = stream_new ();
  stream_auto_connect (stream, rc.auto_connect);
  if (!arg->null_stream)
    stream_device_open (stream, rc.audio_output, rc.audio_sample_rate, client_name, rc.jack_auto_start);

  DEBUG ("Creating GUI");
  gui_new (&rc, arg, song, osc, stream);

  stream_destroy (stream);

  osc_destroy (osc);

  song_destroy (song);

  pool_destroy (pool);

  event_cleanup ();

  arg_cleanup (arg);
  
  return 0;
}
