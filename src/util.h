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
 *   SVN:$Id: util.h 607 2009-06-25 15:49:51Z olivier $
 */

#ifndef JACKBEAT_UTIL_H
#define JACKBEAT_UTIL_H

#include "config.h"

char *  util_resampler_type_to_str (int type);
int     util_str_to_resampler_type (char *str);
char *  util_get_wd ();
double  util_round (double value);
int     util_version_cmp (const char *v1, const char *v2);
void    util_init_paths();
char *  util_pkgdata_path (char *filename);
char *  util_home_dir();
char *  util_settings_dir();
void    util_strcanon(char *str, char *valid, char subst);
int     util_exec(char *command, ...);
char *  util_path(char *path);
int     util_mkdir(const char *path, int mode);
char *  util_mktmpdir();
int     util_wipe_tmpdir(char *path);
char *  util_basename(const char *path);
char *  util_dirname(const char *path);
char *  util_str_creplace(char *subject, char from, char to);


#endif /* JACKBEAT_UTIL_H */
