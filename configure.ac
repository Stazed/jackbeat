AC_INIT([jackbeat],[0.7.6])
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([subdir-objects])
AM_CONFIG_HEADER(src/config.h)
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB

GLOBAL_CFLAGS="-O3"
GLOBAL_LDFLAGS=""

#GLOBAL_CFLAGS+=" -DGTK_DISABLE_SINGLE_INCLUDES"
#GLOBAL_CFLAGS+=" -DGTK_DISABLE_DEPRECATED"

case "$host_os" in
    darwin*) 
        GLOBAL_LDFLAGS="$GLOBAL_LDFLAGS -bind_at_load"
        # Adding the path where JackOSX puts jack.pc into 
        export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig
    	;;
    mingw32*)
    	GLOBAL_CFLAGS="$GLOBAL_CFLAGS -Wall" 
        is_mingw32=1
        ;;
    *) 
    	GLOBAL_CFLAGS="$GLOBAL_CFLAGS -Wall -rdynamic" 
     	GLOBAL_LDFLAGS="$GLOBAL_LDFLAGS -rdynamic" 
	;;
esac    

if echo "$host_os" | grep -i linux > /dev/null
then
  is_linux=1
fi

AC_CHECK_HEADERS([execinfo.h])
AC_CHECK_HEADERS([ucontext.h])

AM_CONDITIONAL(MINGW32, [test "$is_mingw32" = "1"])

PKG_CHECK_MODULES(PORTAUDIO, portaudio-2.0 >= 19, true,
                  AC_MSG_ERROR([you need portaudio-2.0 >= 19 - Please see http://www.portaudio.com ]))
AC_SUBST(PORTAUDIO_CFLAGS)
AC_SUBST(PORTAUDIO_LIBS)

if test "$is_linux" == "1"
then
  PKG_CHECK_MODULES(ALSA, alsa >= 1.0, true,
                    AC_MSG_ERROR([libasound >= 1.0 is required on Linux - http://www.alsa-project.org ]))
  AC_SUBST(ALSA_CFLAGS)
  AC_SUBST(ALSA_LIBS)
fi  

AC_ARG_WITH([jack], [AS_HELP_STRING([--without-jack], [disable JACK support])], [], [with_jack=yes])
          
if test "x$with_jack" != xno
then
  PKG_CHECK_MODULES(JACK, jack >= 0.101, [have_jack=1], true)
else
  AC_MSG_CHECKING([for JACK])
  AC_MSG_RESULT([disabled])
fi

if test "$have_jack" != ""
then
  AC_DEFINE(HAVE_JACK, [1], [Jack support])
fi

AC_SUBST(JACK_CFLAGS)
AC_SUBST(JACK_LIBS)
AM_CONDITIONAL(HAVE_JACK, [test "$have_jack" = "1"])

PKG_CHECK_MODULES(PULSE, libpulse-simple >= 0.9.10, [have_pulse=1], true)
AC_SUBST(PULSE_CFLAGS)
AC_SUBST(PULSE_LIBS)
if test "$have_pulse" = "1"
then
  AC_DEFINE(HAVE_PULSE, [1], [PulseAudio support])
fi
AM_CONDITIONAL(HAVE_PULSE, [test "$have_pulse" = "1"])

if test "$have_pulse" = "" && test "$is_linux" = "1"
then
  AC_MSG_WARN([Can't find libpulse, PulseAudio will not be supported])
fi

PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.12, [have_pkg_gtk=true], true)

if test x$have_pkg_gtk = xtrue
then
  AC_MSG_CHECKING([whether this is gtk-quartz])
  if test "`pkg-config --variable=target gtk+-2.0`" == "quartz"
  then
      AC_MSG_RESULT([yes])
      quartz=true
      AC_DEFINE(HAVE_GTK_QUARTZ, 1, [Whether GTK is running on Mac OS X Quartz])
      PKG_CHECK_MODULES(MACINTEGRATION, ige-mac-integration >= 0.8.2, true,
                        AC_MSG_ERROR([you need ige-mac-integration >= 0.8.2 - Please see http://developer.imendio.com/projects/gtk-macosx]))
      MACINTEGRATION_CFLAGS="$MACINTEGRATION_CFLAGS -I/System/Library/Frameworks/Carbon.framework/Headers"
      MACINTEGRATION_LIBS="$MACINTEGRATION_LIBS -framework Carbon"
      AC_SUBST(MACINTEGRATION_CFLAGS)
      AC_SUBST(MACINTEGRATION_LIBS)
  else    
      AC_MSG_RESULT([no])
  fi
else
  AC_MSG_CHECKING([for GTK-OSX framework])
  if test -f /Library/Frameworks/Gtk.framework/Headers/gtk/gtk.h
  then
      AC_MSG_RESULT([yes])
      quartz=true
      AC_DEFINE(HAVE_GTK_QUARTZ, 1, [Whether GTK is running on Mac OS X Quartz])
      GTK_CFLAGS="-I/Library/Frameworks/Gtk.framework/Headers -I/Library/Frameworks/GLib.framework/Headers -I/Library/Frameworks/Cairo.framework/Headers -I/System/Library/Frameworks/Carbon.framework/Headers"
      GTK_LIBS="-framework Gtk -framework GLib -framework Cairo -framework Carbon"
  else    
      AC_MSG_RESULT([no])
      AC_MSG_ERROR([you need gtk+ >= 2.12 - Please see http://www.gtk.org ])
  fi  
fi

AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

AC_DEFINE(USE_PHAT, 1, [Whether to use the (modified) Phat Audio Toolkit])
use_phat=true

AM_CONDITIONAL(USE_PHAT, [test x$use_phat = xtrue])
AM_CONDITIONAL(GTK_QUARTZ, [test x$quartz = xtrue])

PKG_CHECK_MODULES(SNDFILE, sndfile >= 1.0.15, true,
                  AC_MSG_ERROR([you need libsndfile >= 1.0.15 - Please see http://www.mega-nerd.com/libsndfile ]))
AC_SUBST(SNDFILE_CFLAGS)
AC_SUBST(SNDFILE_LIBS)

PKG_CHECK_MODULES(XML, libxml-2.0 >= 2.6, true,
                  AC_MSG_ERROR([you need libxml >= 2.6 - Please see http://xmlsoft.org ]))
AC_SUBST(XML_CFLAGS)
AC_SUBST(XML_LIBS)

PKG_CHECK_MODULES(SRC, samplerate >= 0.1.2, true,
                  AC_MSG_ERROR([you need libsamplerate >= 0.1.2 - Please see http://www.mega-nerd.com/SRC ]))
AC_SUBST(SRC_CFLAGS)
AC_SUBST(SRC_LIBS)

PKG_CHECK_MODULES(LIBLO, liblo >= 0.22, true,
                  AC_MSG_ERROR([you need liblo >= 0.22 - http://liblo.sourceforge.net ]))
AC_SUBST(LIBLO_CFLAGS)
AC_SUBST(LIBLO_LIBS)

PKG_CHECK_MODULES(GMODULE, gmodule-2.0 >= 2.0, true,
                  AC_MSG_ERROR([you need gmodule >= 2.0 ]))
AC_SUBST(GMODULE_CFLAGS)
AC_SUBST(GMODULE_LIBS)

AC_ARG_ENABLE([dmalloc], AS_HELP_STRING([--enable-dmalloc],
            [link against dmalloc, for memory debugging (default: no)]),
            [AC_DEFINE(DMALLOC,1,[Whether to link against dmalloc]) GLOBAL_LDFLAGS="$GLOBAL_LDFLAGS -ldmalloc"])

AC_ARG_ENABLE([memdebug], AS_HELP_STRING([--enable-memdebug],
            [include built-in memory debugging routines (default: no)]),
            [AC_DEFINE(MEMDEBUG,1,[Whether to include built-in memory debugging routines]) memdebug=true])

AM_CONDITIONAL(MEMDEBUG, test x$memdebug = xtrue)

AC_ARG_ENABLE([devmode], AS_HELP_STRING([--enable-devmode],
            [enable developer mode]),
            [AC_DEFINE(DEVMODE,1,[Developer mode])])

AC_ARG_ENABLE([macbundle], AS_HELP_STRING([--enable-macbundle],
            [enable mac bundle packaging]),
            [AC_DEFINE(MACBUNDLE,1,[Mac bundle mode])])

AC_SUBST(MEMDEBUG_SOURCES)
AC_SUBST(GLOBAL_CFLAGS)
AC_SUBST(GLOBAL_LDFLAGS)

AC_CONFIG_FILES([ \
    Makefile \
    src/Makefile \
    src/core/Makefile \
    src/stream/Makefile \
    pkgdata/Makefile \
    pkgdata/glade/Makefile \
    pkgdata/help/Makefile \
    pkgdata/pixmaps/Makefile \
])
AC_OUTPUT

