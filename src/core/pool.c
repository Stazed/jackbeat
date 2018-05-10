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
 *   SVN:$Id$
 */

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "pool.h"
#include "msg.h"
#include "compat.h"

#define DEBUG(M, ...) { printf("POO  %s(): ", __func__); printf(M, ## __VA_ARGS__); printf("\n"); fflush(stdout); }

typedef struct pool_process_t 
{
  pool_process_callback_t callback;
  void *                  data;
} pool_process_t;

typedef struct pool_thread_t {
  pthread_t         id;
  pool_t *          pool;
  msg_t *           msg;
  pool_process_t ** processes;
} pool_thread_t;

struct pool_t 
{
  pool_thread_t ** threads;
  int              nthreads;
  int volatile     terminate;
  int              running;
  int              current_thread;
  pthread_mutex_t  mutex;
};

static void *
pool_thread_start (void * arg)
{
  pool_thread_t *thread = (pool_thread_t *) arg;
  int i = 0;
  while (!thread->pool->terminate)
  {
    pool_process_t **new_processes;
    char str[160];
    while (msg_receive (thread->msg, (void *) &new_processes))
      thread->processes = new_processes;

    msg_sync (thread->msg);

    int i, more_work = 1;
    while (more_work && !thread->pool->terminate)
    {
      more_work = 0;
      for (i = 0; thread->processes && thread->processes[i]; i++)
      {
        pool_process_t *process = thread->processes[i];
        more_work |= process->callback (process->data);
      }
    }      
    if (!thread->pool->terminate) {
      compat_sleep(1); // 1ms is quite a lot...
    }     
  }
  return NULL;
}

pool_t *
pool_new (int nthreads)
{
  pool_t *pool = malloc (sizeof (pool_t));
  pool->threads = malloc (nthreads * sizeof (pool_thread_t *));
  pool->nthreads = nthreads;
  pool->terminate = 0;
  pool->running = 0;
  pool->current_thread = 0;
  pthread_mutex_init (&pool->mutex, NULL);
  int i, success = 0;

  for (i = 0; i < nthreads; i++)
  {
    pool_thread_t *thread = malloc (sizeof (pool_thread_t));
    thread->pool = pool;
    thread->processes = NULL;
    thread->msg = msg_new (4096, sizeof (pool_process_t **));
    pool->threads[i] = thread;
    success = !pthread_create (&thread->id, NULL, pool_thread_start, (void *) thread);
    if (success)
    {
      pool->running = 1;
    }
    else
    {
      msg_destroy (thread->msg);
      free (thread);
      pool->threads[i] = NULL;
      break;
    }
  }    

  if (!success)
  {
    pool_destroy (pool);
    pool = NULL;
  }

  return pool;
}

void
pool_destroy (pool_t *pool)
{
  pthread_mutex_lock (&pool->mutex);
  int i, j;
  if (pool->running)
  {
    pool->terminate = 1;
    for (i = 0; i < pool->nthreads && pool->threads[i]; i++)
    {
      pool_thread_t *thread = pool->threads[i];
      pthread_join (thread->id, NULL); // test this
      for (j = 0; thread->processes && thread->processes[j]; j++)
        free (thread->processes[j]);
      free (thread->processes);
      msg_destroy (thread->msg);
      free (thread);
    }
  }

  pthread_mutex_unlock (&pool->mutex);
  pthread_mutex_destroy (&pool->mutex);
  free (pool->threads);
  free (pool);
}

void
_pool_add_process (pool_t *pool, pool_process_callback_t callback, void *data, const char *caller)
{
  pthread_mutex_lock (&pool->mutex);
  if (pool->running)
  {
    pool_thread_t *thread = pool->threads[pool->current_thread];

    int n;
    for (n = 0; thread->processes && thread->processes[n]; n++)
      ;

    pool_process_t **old_processes = thread->processes;
    pool_process_t **new_processes = malloc ((n + 2) * sizeof (pool_process_t *));
    if (n)
      memcpy (new_processes, old_processes, n * sizeof (pool_process_t *));

    pool_process_t *p = malloc (sizeof (pool_process_t));
    p->callback = callback;
    p->data = data;

    new_processes[n] = p;
    new_processes[n + 1] = NULL;
    
    msg_send (thread->msg, &new_processes, MSG_ACK);

    free (old_processes);

    DEBUG("process %d of thread %d added by %s", n, pool->current_thread, caller);
    if (++pool->current_thread >= pool->nthreads)
      pool->current_thread = 0;
  }    
  pthread_mutex_unlock (&pool->mutex);
}

void
pool_remove_process (pool_t *pool, pool_process_callback_t callback, void *data)
{
  pthread_mutex_lock (&pool->mutex);
  int i, j, k, l;
  if (pool->running)
  {
    for (k = 0; k < pool->nthreads; k++)
    {
      pool_thread_t *thread = pool->threads[k];

      pool_process_t **old_processes = thread->processes;
      pool_process_t **new_processes = NULL;
      pool_process_t **gc = NULL;
      int removed = 0;
      for (i = 0, j = 0, l = 0; old_processes && old_processes[i]; i++)
      {
        if (old_processes[i]->callback != callback || old_processes[i]->data != data)
        {
          new_processes = realloc (new_processes, (j + 2) * sizeof (pool_process_t *));
          new_processes[j] = old_processes[i];
          new_processes[j + 1] = NULL;
          j++;
        }
        else
        {
          gc = realloc (gc, (l + 2) * sizeof (pool_process_t *));
          gc[l] = old_processes[i];
          gc[l + 1] = NULL;
          l++;
          removed = 1;
        }
      }      
     
      if (removed)
      {
        msg_send (thread->msg, &new_processes, MSG_ACK);
        for (i = 0; gc && gc[i]; i++)
          free (gc[i]);
        free (old_processes);
      }
      else
      {
        free (new_processes);
      }
    }        
  }    
  pthread_mutex_unlock (&pool->mutex);
}

