/* Adapted from the PS4 thread-local destructor implementation in
 * https://github.com/orbis-ports/orbis-compat/src/orbis_thread_atexit.c.
 * SPDX-License-Identifier: MIT
 *
 * OpenOrbis libc++ forwards __cxa_thread_atexit to this libc hook, which is
 * absent from the SDK's musl archive. Register destructors with pthread TLS
 * so they run on the owning thread when it exits.
 */
#include <pthread.h>
#include <stdlib.h>

struct ps4_thread_dtor
{
  void (*function)(void*);
  void* object;
  struct ps4_thread_dtor* next;
};

static pthread_key_t dtor_key;
static pthread_once_t dtor_key_once = PTHREAD_ONCE_INIT;
static int dtor_key_created;

int __cxa_thread_atexit_impl(void (*function)(void*), void* object, void* dso_handle);

static void run_thread_dtors(void* head)
{
  struct ps4_thread_dtor* dtor = (struct ps4_thread_dtor*)head;
  while (dtor)
  {
    struct ps4_thread_dtor* next = dtor->next;
    dtor->function(dtor->object);
    free(dtor);
    dtor = next;
  }
}

static void create_dtor_key(void)
{
  dtor_key_created = pthread_key_create(&dtor_key, run_thread_dtors) == 0;
}

int __cxa_thread_atexit_impl(void (*function)(void*), void* object, void* dso_handle)
{
  (void)dso_handle;
  if (pthread_once(&dtor_key_once, create_dtor_key) != 0 || !dtor_key_created)
    return -1;

  struct ps4_thread_dtor* dtor = (struct ps4_thread_dtor*)malloc(sizeof(*dtor));
  if (!dtor)
    return -1;

  dtor->function = function;
  dtor->object = object;
  dtor->next = (struct ps4_thread_dtor*)pthread_getspecific(dtor_key);
  if (pthread_setspecific(dtor_key, dtor) != 0)
  {
    free(dtor);
    return -1;
  }

  return 0;
}
