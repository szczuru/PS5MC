#ifndef LIBJBC_H
#define LIBJBC_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>
#include <stdint.h>
#include "defs.h"

	typedef struct jbc_cred {
		uid_t uid;
		uid_t ruid;
		uid_t svuid;
		gid_t rgid;
		gid_t svgid;
		uintptr_t prison;
		uintptr_t cdir;
		uintptr_t rdir;
		uintptr_t jdir;
		uint64_t sceProcType;
		uint64_t sonyCred;
		uint64_t sceProcCap;
	} jbc_cred;

	typedef struct jbc_jail_state {
		struct jbc_cred original;
		struct jbc_cred root;
	} jbc_jail_state;


	uintptr_t jbc_get_prison0(void);
	uintptr_t jbc_get_rootvnode(void);
	int jbc_get_cred(struct jbc_cred* cred);
	int jbc_jailbreak_cred(struct jbc_cred* cred);
	int jbc_set_cred(const struct jbc_cred* cred);
	int jbc_jailbreak(struct jbc_jail_state* state);
	int jbc_unjailbreak(struct jbc_jail_state* state);


	uint64_t __attribute__((sysv_abi)) jbc_krw_kcall(uint64_t fn, ...);
	uintptr_t jbc_krw_get_td(void);

	typedef enum KmemKind { USERSPACE, KERNEL_HEAP, KERNEL_TEXT } KmemKind;

	int jbc_krw_memcpy(uintptr_t dst, uintptr_t src, size_t sz, KmemKind kind);
	uint64_t jbc_krw_read64(uintptr_t p, KmemKind kind);
	int jbc_krw_write64(uintptr_t p, KmemKind kind, uintptr_t val);

	
	enum {
		CWD_KEEP,
		CWD_ROOT,
		CWD_RESET,
	};

	void jbc_run_as_root(void (*fn)(void* arg), void* arg, int cwd_mode);
	int jbc_mount_in_sandbox(const char* system_path, const char* mnt_name);
	int jbc_unmount_in_sandbox(const char* mnt_name);

#ifdef __cplusplus
}
#endif

#endif // LIBJBC_H
