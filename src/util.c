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
 *   SVN:$Id: util.c 607 2009-06-25 15:49:51Z olivier $
 */


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>
#ifdef __WIN32__
#include <windows.h>
#else
#include <sys/wait.h>
#endif

#include "sequence.h"
#include "util.h"

#ifdef __WIN32__
#define DIRSEP "\\"
#else
#define DIRSEP "/"
#endif

#define DEBUG(M, ...) { printf("UTL  %s(): ", __func__); printf(M, ## __VA_ARGS__); printf("\n"); fflush(stdout); }

static char *root_path = NULL;

char *
util_resampler_type_to_str (int type)
{
  static char str[64];
  switch (type) {
    case SEQUENCE_SINC   : strcpy (str, "sinc"); break;
    case SEQUENCE_LINEAR : strcpy (str, "linear"); break;
    default: sprintf (str, "UNKNOWN:%d", type);
  }
  return str;
}

int
util_str_to_resampler_type (char *str)
{
  if (strcmp (str, "sinc") == 0)    return SEQUENCE_SINC;
  if (strcmp (str, "linear") == 0)  return SEQUENCE_LINEAR;
  return -1;
}

char *
util_get_wd()
{
  char s = 16;
  char *wd = malloc (s);
  while (getcwd (wd, s) == NULL)
   {
    s += 16;
    wd = realloc (wd, s);
   }
  return wd;
}

double  
util_round (double value)
{
  double bottom = floor (value);
  return ((value - bottom) < 0.5) ? bottom : ceil (value);
}

int 
util_version_cmp (const char *v1, const char *v2)
{
  int x1, y1, z1, x2, y2, z2;
  y1 = y2 = z1 = z2 = 0;
  int res = -2;
  if (sscanf (v1, "%d.%d.%d", &x1, &y1, &z1) && sscanf (v2, "%d.%d.%d", &x2, &y2, &z2))
  {
    printf("%d.%d.%d - %d.%d.%d\n", x1, y1, z1, x2, y2, z2);

    if (x2 > x1)
      res = 1;
    else if (x2 < x1)
      res = -1;
    else if (y2 > y1)
      res = 1;
    else if (y2 < y1)
      res = -1;
    else if (z2 > z1)
      res = 1;
    else if (z2 < z1)
      res = -1;
    else
      res = 0;
  }
  return res;
}

char *  
util_pkgdata_path (char *filename)
{
    static char *datadir = NULL;
    char *path = NULL;

    if (!datadir) {
#ifdef MACBUNDLE
        datadir = strdup(getenv("XDG_DATA_DIRS"));
        datadir = realloc(datadir, strlen(datadir) + strlen("/jackbeat") + 1);
        strcat(datadir, "/jackbeat");
#elif __WIN32__
        datadir = malloc(strlen(root_path) + strlen(PKGDATADIR) + 2);
        sprintf(datadir, "%s/%s", root_path, PKGDATADIR);
#else
#ifdef DEVMODE        
        datadir = strdup("pkgdata");
#else
        datadir = strdup(PKGDATADIR);
#endif
#endif
    }

    if (path) 
        free(path);

    path = malloc(strlen(datadir) + 1 + strlen(filename) + 1);
    sprintf(path, "%s/%s", datadir, filename);
    util_path(path);

    return path;
}

char *util_home_dir()
{
#ifdef __WIN32__
    return getenv("HOMEPATH");
#else
    return getenv("HOME");    
#endif    
}

char *util_settings_dir()
{
    static char *path = NULL;
    if (!path) { 
#ifdef __WIN32__
        char *root = getenv("APPDATA");
        char dot[] = "";
#else
        char *root = util_home_dir();
        char dot[] = ".";
#endif  
        path = malloc(strlen(root) + strlen(PACKAGE) + 3); // small leak
        sprintf(path, "%s%s%s%s", root, DIRSEP, dot, PACKAGE);
    }
    return path;
}

void
util_strcanon(char *str, char *valid, char subst)
{
    int i, j;
    for (i = 0; str[i]; i++) {
        for (j = 0; valid[j]; j++) {
            if (str[i] == valid[j])
                break;
        }
        if (!valid[j])
            str[i] = subst;
    }
}

/*
struct archive_entry *
util_archive_find (struct archive *archive, char *path)
{
  struct archive_entry *entry, *result = NULL;
  while (archive_read_next_header (archive, &entry) == ARCHIVE_OK)
  {
    if (!strcmp (archive_entry_pathname (entry), path))
    {
      result = entry;
      break;
    }
    if (archive_entry_filetype (entry) == S_IFREG)
      archive_read_data_skip(archive);
  }
  return result;
}

char *
util_archive_load_textfile (struct archive *archive, struct archive_entry *entry)
{
  
}
*/

#ifdef __WIN32__
int util_execvp(char *command, char *argv[])
{
    STARTUPINFO siStartupInfo;
    PROCESS_INFORMATION piProcessInfo;

    memset(&siStartupInfo, 0, sizeof(siStartupInfo));
    memset(&piProcessInfo, 0, sizeof(piProcessInfo));

    siStartupInfo.cb = sizeof(siStartupInfo);

    char *cmdline = strdup("");
    while (*argv) {
        cmdline = realloc(cmdline, strlen(cmdline) + strlen(*argv) + 4);
        strcat(cmdline, " \"");
        strcat(cmdline, *argv++);
        strcat(cmdline, "\"");
    }
    char *cmd = malloc(strlen(root_path) + strlen(command) + 10);
    sprintf(cmd, "%s\\bin\\%s.exe", root_path, command);

    DEBUG("Creating win32 process: %s [%s]", command, cmdline);
    int success = 0;
    if(CreateProcess(cmd, cmdline, NULL, NULL, FALSE, CREATE_DEFAULT_ERROR_MODE, 
                     NULL, NULL, &siStartupInfo, &piProcessInfo) == TRUE) {
        WaitForSingleObject(piProcessInfo.hProcess, INFINITE);
        CloseHandle(piProcessInfo.hThread);
        CloseHandle(piProcessInfo.hProcess);
        success = 1;
    } else {
        unsigned long errcode = GetLastError();
        DEBUG("Failed to create win32 process (errcode: %lu)", errcode);
    }
    free(cmdline);
    free(cmd);
    return success;
}

#else
static int util_execvp(char *command, char *argv[])
{
    int status  = 0;

    // Assign command to argv[0]
    char **fullargv = malloc(sizeof(command));
    fullargv[0]     = command;
    int i           = 0;
    while(1) {
        DEBUG("argv[%d]: %s", i, fullargv[i]);
        i++;
        fullargv    = realloc(fullargv, sizeof(char *) * (i + 1));
        fullargv[i] = argv[i - 1];
        if (!fullargv[i])
          break;
    }

    pid_t pid = fork();
    if (!pid) {
        execvp(command, fullargv);
        exit(127);
    } else if (pid < 0) {
        status = -1;
    } else {
        if (waitpid(pid, &status, 0) != pid)
            status = -1;
    }

    free(fullargv);
    return (status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
#endif    

int util_exec(char *command, ...)
{
    va_list ap;
    char *  arg;
    char ** argv    = NULL;
    int     nargs   = 0;

    DEBUG("Running command: %s", command);

    va_start(ap, command);
    while (1) {
        arg           = va_arg(ap, char *);
        argv          = realloc(argv, (nargs + 1) * sizeof(arg));
        argv[nargs++] = arg;
        if (!arg)
          break;
        DEBUG("Argument %d: %s", nargs - 1, arg);  
    }
    va_end(ap);

    int success = util_execvp(command, argv);
    free(argv);
    return success;
}

char *util_str_creplace(char *subject, char from, char to)
{
    int i, ii = strlen(subject);
    for (i = 0; i < ii; i++) {
        if (subject[i] == from)
            subject[i] = to;
    }
    return subject;
}

char *util_path(char *path)
{
#ifdef __WIN32__
    if (path) {
        util_str_creplace(path, '/', '\\');
    }
#endif
    return path;
}

int util_mkdir(const char *path, int mode)
{
#ifdef __WIN32__
    return (mkdir (path) == 0);
#else
    return (mkdir (path, (mode_t) mode) == 0);
#endif
}

char * util_mktmpdir()
{
#ifdef __WIN32__
    char *tmp = strdup(getenv("TEMP"));
#else
    char *tmp = strdup("/tmp");    
#endif
    char basename[] = "/" PACKAGE ".tmp.%%%%%%%%";
    int i;
    srand(time(NULL));
    for (i = 0; i < strlen(basename); i++) {
        if (basename[i] == '%') {
            basename[i] = rand() % ('Z' - 'A') + 'A';
        }
    }
    tmp = realloc(tmp, strlen(tmp) + strlen(basename) + 1);
    strcat(tmp, basename);
    if (util_mkdir(util_path(tmp), 0700)) {
        DEBUG("Created tmp dir: %s", tmp);
        return tmp;
    } 
    DEBUG("ERROR: Failed to create tmp dir: %s", tmp);
    return NULL;
}

static int util_wipe_content(char *path, char *security_pattern) 
{
    int success = 0;
    DIR *dir = opendir(path);
    if (dir) {
        success = 1;
        struct dirent *dirent;
        while (success && (dirent = readdir(dir))) {
            if (strcmp(dirent->d_name, ".") && strcmp(dirent->d_name, "..")) {
                char *fullpath = malloc(strlen(path) + strlen(dirent->d_name) + 2);
                strcpy(fullpath, path);
                strcat(fullpath, DIRSEP);
                strcat(fullpath, dirent->d_name);
                struct stat info;
                if (!stat(fullpath, &info)) {
                    if (S_IFDIR & info.st_mode) {
                        success = util_wipe_content(fullpath, security_pattern);
                        if (success) {
                            DEBUG("rmdir %s", fullpath);
                            success = (rmdir(fullpath) == 0);
                        }
                    } else {
                        DEBUG("unlink %s", fullpath);
                        success = (unlink(fullpath) == 0);
                    }
                } else {
                    success = 0;
                }
            }
        }
        closedir(dir);
    }
    return success;
}

int util_wipe_tmpdir(char *path)
{
    int success = 0;
    char security_pattern[] = PACKAGE ".tmp";
    if (strstr(path, security_pattern)) {
        success = util_wipe_content(path, security_pattern);
        if (success) {
            DEBUG("rmdir %s", path);
            success = (rmdir(path) == 0);
        }
    } else {
        DEBUG("ERROR: path '%s' doesn't contain security pattern '%s' ; wiping canceled", 
              path, security_pattern);
    }
    if (success) {
        DEBUG("Successfully removed tmp dir: %s", path);
    } else {
        DEBUG("Failed to remove tmp dir: %s", path);
    }
    return success;
}

char *util_basename(const char *path)
{
    char *s = strdup(path);
    util_str_creplace(s, '\\', '/');
    char *b = strdup(basename(s));
    free(s);
    return b;
}

char *util_dirname(const char *path)
{
    char *s = strdup(path);
    util_str_creplace(s, '\\', '/');
    char *d = strdup(dirname(s));
    free(s);
    return util_path(d);
}

void util_init_paths()
{
#ifdef __WIN32__
    char fullpath[MAX_PATH];
    GetModuleFileName(NULL, fullpath, MAX_PATH);
    char *d = util_dirname(fullpath);
    root_path = util_dirname(d); // small leak
    free(d);
    DEBUG("Root directory: %s", root_path);
#endif
}
