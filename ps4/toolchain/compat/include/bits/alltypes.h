/*
 * Sony's pthread implementation stores these opaque attributes as pointers,
 * while OpenOrbis' musl headers declare four-byte objects. libkernel writes
 * eight bytes during their *_attr_init calls, corrupting adjacent memory.
 * Override only the four undersized types measured on PS4; keep mutex/cond/
 * rwlock and pthread_once layouts unchanged for compatibility with libc++.
 */
#if defined(__NEED_pthread_mutexattr_t) && !defined(__DEFINED_pthread_mutexattr_t)
typedef struct { void *__opaque; } pthread_mutexattr_t;
#define __DEFINED_pthread_mutexattr_t
#endif

#if defined(__NEED_pthread_condattr_t) && !defined(__DEFINED_pthread_condattr_t)
typedef struct { void *__opaque; } pthread_condattr_t;
#define __DEFINED_pthread_condattr_t
#endif

#if defined(__NEED_pthread_barrierattr_t) && !defined(__DEFINED_pthread_barrierattr_t)
typedef struct { void *__opaque; } pthread_barrierattr_t;
#define __DEFINED_pthread_barrierattr_t
#endif

#if defined(__NEED_pthread_spinlock_t) && !defined(__DEFINED_pthread_spinlock_t)
typedef void *pthread_spinlock_t;
#define __DEFINED_pthread_spinlock_t
#endif

#include_next <bits/alltypes.h>
