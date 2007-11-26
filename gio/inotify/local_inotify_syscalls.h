#ifndef _LINUX_INOTIFY_SYSCALLS_H
#define _LINUX_INOTIFY_SYSCALLS_H

#include <asm/types.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(__i386__)
# define __NR_inotify_init	291
# define __NR_inotify_add_watch	292
# define __NR_inotify_rm_watch	293
#elif defined(__x86_64__)
# define __NR_inotify_init	253
# define __NR_inotify_add_watch	254
# define __NR_inotify_rm_watch	255
#elif defined(__alpha__)
# define __NR_inotify_init      444
# define __NR_inotify_add_watch 445
# define __NR_inotify_rm_watch  446
#elif defined(__ppc__) || defined(__powerpc__) || defined(__powerpc64__)
# define __NR_inotify_init      275
# define __NR_inotify_add_watch 276
# define __NR_inotify_rm_watch  277
#elif defined(__sparc__) || defined (__sparc64__)
# define __NR_inotify_init      151
# define __NR_inotify_add_watch 152
# define __NR_inotify_rm_watch  156
#elif defined (__ia64__)
# define __NR_inotify_init  1277
# define __NR_inotify_add_watch 1278
# define __NR_inotify_rm_watch  1279
#elif defined (__s390__) || defined (__s390x__)
# define __NR_inotify_init  284
# define __NR_inotify_add_watch 285
# define __NR_inotify_rm_watch  286
#elif defined (__arm__)
# define __NR_inotify_init  316
# define __NR_inotify_add_watch 317
# define __NR_inotify_rm_watch  318
#elif defined (__SH4__)
# define __NR_inotify_init  290
# define __NR_inotify_add_watch 291
# define __NR_inotify_rm_watch  292
#elif defined (__SH5__)
# define __NR_inotify_init  318
# define __NR_inotify_add_watch 319
# define __NR_inotify_rm_watch  320
#else
# warning "Unsupported architecture"
#endif

#if defined(__i386__) || defined(__x86_64) || defined(__alpha__) || defined(__ppc__) || defined(__sparc__) || defined(__powerpc__) || defined(__powerpc64__) || defined(__ia64__) || defined(__s390__)
static inline int inotify_init (void)
{
	return syscall (__NR_inotify_init);
}

static inline int inotify_add_watch (int fd, const char *name, __u32 mask)
{
	return syscall (__NR_inotify_add_watch, fd, name, mask);
}

static inline int inotify_rm_watch (int fd, __u32 wd)
{
	return syscall (__NR_inotify_rm_watch, fd, wd);
}
#else
static inline int inotify_init (void)
{
	return -1;
}

static inline int inotify_add_watch (int fd, const char *name, __u32 mask)
{
	return -1;
}

static inline int inotify_rm_watch (int fd, __u32 wd)
{
	return -1;
}

#endif

#endif /* _LINUX_INOTIFY_SYSCALLS_H */
