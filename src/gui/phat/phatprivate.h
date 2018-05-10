#ifndef __PHAT_PRIVATE_H__
#define __PHAT_PRIVATE_H__

#include <stdio.h>
#include "config.h"

#define PHATDEBUG 0
#define debug(...) if (PHATDEBUG) fprintf (stderr, __VA_ARGS__)

void phat_warp_pointer (int xsrc, int ysrc,
                        int xdest, int ydest);

#endif /* __PHAT_PRIVATE_H__ */
