#include <gdk/gdk.h>
//#include <gdk/gdkx.h> // Fails on OSX/Gtk-Quartz
#include "gui/phat/phatprivate.h"

void
phat_warp_pointer (int xsrc, int ysrc,
                   int xdest, int ydest) {
    /*
    int x = xdest - xsrc;
    int y = ydest - ysrc;

    XWarpPointer (GDK_DISPLAY ( ), None, None, 0, 0, 0, 0, x, y); // Fails on OSX/Gtk-Quartz
     */ }
