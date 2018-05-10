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
 *   SVN:$Id:$
 */

#ifndef JACKBEAT_POOL_H
#define JACKBEAT_POOL_H

typedef struct pool_t pool_t;
typedef int (* pool_process_callback_t) (void *data);

pool_t * pool_new(int nthreads);
void pool_destroy(pool_t *pool);
#define pool_add_process(pool, callback, data) _pool_add_process (pool, callback, data, __func__)
void _pool_add_process(pool_t *pool, pool_process_callback_t callback, void *data, const char *caller);
void pool_remove_process(pool_t *pool, pool_process_callback_t callback, void *data);

#endif
