#ifndef LIBJBC_DEFS_H
#define LIBJBC_DEFS_H

/* Minimal FreeBSD uid/gid typedefs for standalone use with the OpenOrbis
   PS4 toolchain (which does not ship <sys/types.h> uid_t definitions). */
#include <stdint.h>
typedef unsigned int uid_t;
typedef unsigned int gid_t;

#endif /* LIBJBC_DEFS_H */
