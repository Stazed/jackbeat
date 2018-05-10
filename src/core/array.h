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
 *   SVN:$Id: util.h 492 2009-02-01 13:30:04Z olivier $
 */

#ifndef CORE_ARRAY_H
#define CORE_ARRAY_H

#define ARRAY_ADD(TYPE,VAR,NUM,ITEM)           \
  TYPE **_buf = calloc (NUM + 1, sizeof (TYPE *));  \
  if (VAR)                                          \
   {                                                \
    memcpy (_buf, VAR, NUM * sizeof (TYPE *));      \
    free (VAR);                                     \
   }                                                \
  VAR = _buf;                                       \
  VAR[NUM++] = ITEM; 
  
#define ARRAY_REMOVE(TYPE,VAR,NUM,ITEM)        \
  if (NUM == 1)                                     \
   {                                                \
    NUM = 0;                                        \
    free (VAR);                                     \
    VAR = NULL;                                     \
   }                                                \
  else                                              \
   {                                                \
    int _i;                                         \
    TYPE **_buf = calloc (NUM-1, sizeof (TYPE *));  \
    TYPE **_ptr = _buf;                             \
    for (_i=0; _i < NUM; _i++)                      \
     if (VAR[_i] != ITEM) *(_ptr++) = VAR[_i];      \
    NUM--;                                          \
    free (VAR);                                     \
    VAR = _buf;                                     \
   }                                         

#define ARRAY_DESTROY(VAR, NUM) \
 { \
  int _i; \
  for (_i = 0; _i < NUM; _i++) \
    free (VAR[_i]); \
  free (VAR); \
  VAR = NULL; \
 }

#endif /* CORE_ARRAY_H */
