noinst_LIBRARIES = libstream.a
libstream_a_SOURCES = stream.h stream.c driver.h driver.c null.h null.c \
											paudio.h paudio.c stereo.h stereo.c device.h device.c
if HAVE_JACK
libstream_a_SOURCES += jack.c jack.h 
endif

if HAVE_PULSE
libstream_a_SOURCES += pulse.c pulse.h 
endif

libstream_a_CFLAGS = -I$(srcdir)/.. $(GLOBAL_CFLAGS) $(JACK_CFLAGS) $(PULSE_CFLAGS) $(PORTAUDIO_CFLAGS)

EXTRA_PROGRAMS = streamtest
streamtest_SOURCES = test.c
streamtest_CFLAGS = $(GLOBAL_CFLAGS)
streamtest_LDFLAGS = $(GLOBAL_LDFLAGS)
streamtest_LDADD = libstream.a ../core/libcore.a

stream-test: streamtest$(EXEEXT)
.PHONY: streamtest
