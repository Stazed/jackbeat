SUBDIRS = core stream

bin_PROGRAMS = jackbeat 

jackbeat_SOURCES = \
    sequence.h sequence.c \
    osc.h osc.c \
    song.h song.c \
    rc.h rc.c \
    arg.h arg.c \
    sample.h sample.c \
    gui.h gui.c \
    gui/common.h \
    gui/sequenceeditor.h gui/sequenceeditor.c \
    gui/file.h gui/file.c \
    gui/dk.h gui/dk.c \
    gui/slider.h gui/slider.c \
    gui/builder.h gui/builder.c \
    gui/prefs.h gui/prefs.c \
    gui/toggle.h gui/toggle.c \
    gui/misc.h gui/misc.c \
    jab.h jab.c \
    util.h util.c \
    error.h error.c \
    grid.h grid.c \
    types.h main.c 

if USE_PHAT
jackbeat_SOURCES += \
    gui/phat/phatfanslider.c \
    gui/phat/phatfanslider.h \
    gui/phat/phat.h \
    gui/phat/phathfanslider.c \
    gui/phat/phathfanslider.h \
    gui/phat/phatknob.c \
    gui/phat/phatknob.h \
    gui/phat/phatrange.c \
    gui/phat/phatrange.h \
    gui/phat/phatprivate.c \
    gui/phat/phatprivate.h \
    gui/phat/phatvfanslider.c \
    gui/phat/phatvfanslider.h \
    gui/phat/sample-display.c \
    gui/phat/sample-display.h 
endif

#if MEMDEBUG
#  jackbeat_SOURCES += memdebug.h memdebug.c
#endif


jackbeat_CFLAGS = \
    $(GLOBAL_CFLAGS) \
    $(GTK_CFLAGS) \
    $(SNDFILE_CFLAGS) \
    $(XML_CFLAGS) \
    $(SRC_CFLAGS) \
    $(LIBLO_CFLAGS) \
    $(PHAT_CFLAGS) \
    $(GLADE_CFLAGS) \
    $(MACINTEGRATION_CFLAGS) 

if MINGW32
jackbeat_CFLAGS += -DPKGDATADIR=\"share/jackbeat\" 
else
jackbeat_CFLAGS += -DPKGDATADIR=\"$(pkgdatadir)\" 
endif

jackbeat_LDFLAGS = $(GLOBAL_LDFLAGS)

if MINGW32
jackbeat_LDFLAGS += -mwindows 
endif

jackbeat_LDADD = \
    core/libcore.a stream/libstream.a \
    $(SNDFILE_LIBS) \
    $(GTK_LIBS) \
    $(XML_LIBS) \
    $(SRC_LIBS) \
    $(LIBLO_LIBS) \
    $(PHAT_LIBS) \
    $(GLADE_LIBS) \
    $(MACINTEGRATION_LIBS) \
    $(JACK_LIBS) \
    $(ALSA_LIBS) \
    $(PULSE_LIBS) \
    $(PORTAUDIO_LIBS)

if MINGW32
jackbeat_LDADD += -lwsock32 -lws2_32 -lcomdlg32 jackbeat-winres.$(OBJEXT)
jackbeat-winres.$(OBJEXT): ../mingw/jackbeat.rc ../mingw/jackbeat.ico
		i586-mingw32msvc-windres -i ../mingw/jackbeat.rc -o $@
endif

		
