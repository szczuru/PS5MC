/* OpenOrbis does not provide FreeBSD's pthread_getthreadid_np extension. */
#pragma once

#include <pthread.h>
#include <stdint.h>

static inline int pthread_getthreadid_np(void)
{
  return (int)(uintptr_t)pthread_self();
}
