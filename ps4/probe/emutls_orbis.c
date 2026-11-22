/* Minimal emulated TLS runtime backed directly by Orbis libkernel keys.
 * Keep this file compiled without -femulated-tls to avoid recursion. */
#define _POSIX_C_SOURCE 200112L
#include <orbis/libkernel.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
  size_t size;
  size_t align;
  union
  {
    uintptr_t index;
    void* address;
  } object;
  void* value;
} Ps4EmutlsControl;

static volatile int g_keyCreationLock;

static void lock_key_creation(void)
{
  while (__atomic_exchange_n(&g_keyCreationLock, 1, __ATOMIC_ACQUIRE))
  {
  }
}

static void unlock_key_creation(void)
{
  __atomic_store_n(&g_keyCreationLock, 0, __ATOMIC_RELEASE);
}

void* __emutls_get_address(Ps4EmutlsControl* control)
{
  if (!control)
    return NULL;

  uintptr_t index = __atomic_load_n(&control->object.index, __ATOMIC_ACQUIRE);
  OrbisPthreadKey key;
  if (!index)
  {
    lock_key_creation();
    index = control->object.index;
    if (!index)
    {
      if (scePthreadKeyCreate(&key, free) != 0)
      {
        unlock_key_creation();
        return NULL;
      }
      index = (uintptr_t)key + 1;
      __atomic_store_n(&control->object.index, index, __ATOMIC_RELEASE);
    }
    unlock_key_creation();
  }
  key = (OrbisPthreadKey)(index - 1);

  void* object = scePthreadGetspecific(key);
  if (object)
    return object;

  size_t align = control->align < sizeof(void*) ? sizeof(void*) : control->align;
  if (!align || (align & (align - 1)) != 0)
    return NULL;
  size_t size = control->size ? control->size : 1;
  if (posix_memalign(&object, align, size) != 0 || !object)
    return NULL;

  if (control->value)
    memcpy(object, control->value, control->size);
  else
    memset(object, 0, size);

  if (scePthreadSetspecific(key, object) != 0)
  {
    free(object);
    return NULL;
  }
  return object;
}
