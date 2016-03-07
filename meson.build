project('glib', 'c', 'cpp',
  version : '2.47.6',
  meson_version : '>= 0.30.0',
  default_options : [ 'warning_level=1',
                      'c_std=gnu99',
                      'buildtype=debugoptimized' ])

cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

add_global_arguments('-Werror', language : 'c') # FIXME

major_version = 2
minor_version = 47
micro_version = 6
interface_version = 2 # FIXME

glib_version = meson.project_version()
inc_dirs = include_directories('.', 'glib', 'gmodule', 'gio')

glib_conf = configuration_data()
glibconfig_conf = configuration_data()

# accumulated list of defines as we check for them, so we can easily
# use them later in test programs (autoconf does this automatically)
glib_conf_prefix = ''

glib_conf.set('GLIB_MAJOR_VERSION', major_version)
glib_conf.set('GLIB_MINOR_VERSION', minor_version)
glib_conf.set('GLIB_MICRO_VERSION', micro_version)
glib_conf.set('GLIB_INTERFACE_AGE', micro_version)
glib_conf.set('GLIB_BINARY_AGE', major_version * 100 + minor_version * 10 + micro_version)
glib_conf.set('GETTEXT_PACKAGE', '"glib20"')
glib_conf.set('PACKAGE_BUGREPORT', '"http://bugzilla.gnome.org/enter_bug.cgi?product=glib"')
glib_conf.set('PACKAGE_NAME', '"glib"')
glib_conf.set('PACKAGE_STRING', '"glib @0@"'.format(meson.project_version()))
glib_conf.set('PACKAGE_TARNAME', '"glib"')
glib_conf.set('PACKAGE_URL', '""')
glib_conf.set('PACKAGE_VERSION', '"@0@"'.format(meson.project_version()))
glib_conf.set('ENABLE_NLS', 1)
glib_conf.set('HAVE_GOOD_PRINTF', 1) # FIXME

glib_conf.set('_GNU_SOURCE', 1)

if run_command('xgettext', '--help').returncode() != 0
  error('GNU Gettext required currently.')
endif
glib_conf.set('HAVE_GETTEXT', 1)
glib_conf.set('GLIB_LOCALE_DIR', '"@0@/share/locale"'.format(get_option('prefix')))

# FIXME: what about Cygwin (G_WITH_CYGWIN)
if host_machine.system() == 'windows'
  glib_os = '''#define G_OS_WIN32
               #define G_PLATFORM_WIN32'''
else
  glib_os = '#define G_OS_UNIX'
endif
glibconfig_conf.set('glib_os', glib_os)

# check for header files

include_sys_param_h_if_available = '''
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif'''

headers = [
  [ 'stdlib.h', 'HAVE_STDLIB_H' ],
  [ 'string.h', 'HAVE_STRING_H' ],
  [ 'strings.h', 'HAVE_STRINGS_H' ],
  [ 'memory.h', 'HAVE_MEMORY_H' ],
  [ 'alloca.h', 'HAVE_ALLOCA_H' ],
  [ 'locale.h', 'HAVE_LOCALE_H' ],
  [ 'xlocale.h', 'HAVE_XLOCALE_H' ],
  [ 'float.h', 'HAVE_FLOAT_H' ],
  [ 'limits.h', 'HAVE_LIMITS_H' ],
  [ 'pwd.h', 'HAVE_PWD_H' ],
  [ 'grp.h', 'HAVE_GRP_H' ],
  [ 'poll.h', 'HAVE_POLL_H' ],
  [ 'sys/param.h', 'HAVE_SYS_PARAM_H' ],
  [ 'sys/resource.h', 'HAVE_SYS_RESOURCE_H' ],
  [ 'mach/mach_time.h', 'HAVE_MACH_MACH_TIME_H' ],
  [ 'sys/select.h', 'HAVE_SYS_SELECT_H' ],
  [ 'stdint.h', 'HAVE_STDINT_H' ],
  [ 'inttypes.h', 'HAVE_INTTYPES_H' ],
  [ 'sched.h', 'HAVE_SCHED_H' ],
  [ 'malloc.h', 'HAVE_MALLOC_H' ],
  [ 'sys/vfs.h', 'HAVE_SYS_VFS_H' ],
  [ 'sys/vmount.h', 'HAVE_SYS_VMOUNT_H' ],
  [ 'sys/statfs.h', 'HAVE_SYS_STATFS_H' ],
  [ 'sys/statvfs.h', 'HAVE_SYS_STATVFS_H' ],
  [ 'sys/filio.h', 'HAVE_SYS_FILIO_H' ],
  [ 'mntent.h', 'HAVE_MNTENT_H' ],
  [ 'sys/mnttab.h', 'HAVE_SYS_MNTTAB_H' ],
  [ 'sys/vfstab.h', 'HAVE_SYS_VFSTAB_H' ],
  [ 'sys/mntctl.h', 'HAVE_SYS_mntctl_H' ],
  [ 'fstab.h', 'HAVE_FSTAB_H' ],
  [ 'linux/magic.h', 'HAVE_LINUX_MAGIC_H' ],
  [ 'sys/prctl.h', 'HAVE_SYS_PRCTL_H' ],
  [ 'dirent.h', 'HAVE_DIRENT_H' ], # Some versions of MSC lack these
  [ 'sys/time.h', 'HAVE_SYS_TIME_H' ], # Some versions of MSC lack these
  [ 'sys/times.h', 'HAVE_SYS_TIMES_H' ],
  [ 'sys/wait.h', 'HAVE_SYS_WAIT_H' ],
  [ 'unistd.h', 'HAVE_UNISTD_H' ],
  [ 'values.h', 'HAVE_VALUES_H' ],
  [ 'sys/types.h', 'HAVE_SYS_TYPES_H' ],
  [ 'sys/uio.h', 'HAVE_SYS_UIO_H' ],
  [ 'sys/mkdev.h', 'HAVE_SYS_MKDEV_H' ],
  [ 'sys/mount.h', 'HAVE_SYS_MOUNT_H', include_sys_param_h_if_available ],
  [ 'sys/sysctl.h', 'HAVE_SYS_SYSCTL_H', include_sys_param_h_if_available ],
  [ 'crt_externs.h', 'HAVE_CRT_EXTERNS_H' ],
  [ 'linux/netlink.h', 'HAVE_NETLINK', '#include <sys/socket.h>' ],
  [ 'sys/inotify.h', 'HAVE_SYS_INOTIFY_H' ],
  [ 'sys/event.h', 'HAVE_SYS_EVENT_H' ],
]

foreach h : headers
  header_check_prefix = glib_conf_prefix
  if h.length() == 3
    header_check_prefix = header_check_prefix + h[2]
  endif
  if cc.has_header(h[0], prefix: header_check_prefix)
    glib_conf.set(h[1], 1)
    glib_conf_prefix = glib_conf_prefix + '#define @0@ 1\n'.format(h[1])
  endif
endforeach

if glib_conf.has('HAVE_LOCALE_H')
  if cc.compiles('''#include <locale.h>
                    void foo() { LC_MESSAGES; }''')
    glib_conf.set('HAVE_LC_MESSAGES', 1)
  endif
endif

struct_stat_blkprefix = '''
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
'''

struct_members = [
  [ 'stat', 'st_mtimensec', 'HAVE_STRUCT_STAT_ST_MTIMENSEC' ],
  [ 'stat', 'st_mtim.tv_nsec', 'HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC' ],
  [ 'stat', 'st_atimensec', 'HAVE_STRUCT_STAT_ST_ATIMENSEC' ],
  [ 'stat', 'st_atim.tv_nsec', 'HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC' ],
  [ 'stat', 'st_ctimensec', 'HAVE_STRUCT_STAT_ST_CTIMENSEC' ],
  [ 'stat', 'st_ctim.tv_nsec', 'HAVE_STRUCT_STAT_ST_CTIM_TV_NSEC' ],
  [ 'stat', 'st_birthtime', 'HAVE_STRUCT_STAT_ST_BIRTHTIME' ],
  [ 'stat', 'st_birthtimensec', 'HAVE_STRUCT_STAT_ST_BIRTHTIMENSEC' ],
  [ 'stat', 'st_birthtim', 'HAVE_STRUCT_STAT_ST_BIRTHTIM' ],
  [ 'stat', 'st_birthtim.tv_nsec', 'HAVE_STRUCT_STAT_ST_BIRTHTIME_TV_NSEC' ],
  [ 'stat', 'st_blksize', 'HAVE_STRUCT_STAT_ST_BLKSIZE', struct_stat_blkprefix ],
  [ 'stat', 'st_blocks', 'HAVE_STRUCT_STAT_ST_BLOCKS', struct_stat_blkprefix ],
  [ 'statfs', 'f_fstypename', 'HAVE_STRUCT_STATFS_F_FSTYPENAME', struct_stat_blkprefix ],
  [ 'statfs', 'f_bavail', 'HAVE_STRUCT_STATFS_F_BAVAIL', struct_stat_blkprefix ],
  [ 'dirent', 'd_type', 'HAVE_STRUCT_DIRENT_D_TYPE', '''#include <sys/types.h>
                                                        #include <dirent.h>''' ],
]

foreach m : struct_members
  header_check_prefix = glib_conf_prefix
  if m.length() == 4
    header_check_prefix = header_check_prefix + m[3]
  else
    header_check_prefix = header_check_prefix + '#include <sys/stat.h>'
  endif
  if cc.has_member('struct ' + m[0], m[1], prefix : header_check_prefix)
    glib_conf.set(m[2], 1)
    glib_conf_prefix = glib_conf_prefix + '#define @0@ 1\n'.format(m[2])
  else
  endif
endforeach

#FIXME: maybe we should add available includes all to the glib_conf_prefix list
# functions
functions = [
  [ 'alloca', 'HAVE_ALLOCA', '#include <alloca.h>' ],
  [ 'mmap', 'HAVE_MMAP', '#include <sys/mman.h>' ],
  [ 'posix_memalign', 'HAVE_POSIX_MEMALIGN', '#include <stdlib.h>' ],
  [ 'memalign', 'HAVE_MEMALIGN', '#include <malloc.h>' ],
  [ 'valloc', 'HAVE_VALLOC', '#include <stdlib.h>' ],
  [ 'fsync', 'HAVE_FSYNC', '''#ifdef HAVE_UNISTD_H
                              #include <unistd.h>
                              #endif''' ],
  [ 'pipe2', 'HAVE_PIPE2', '''#define _GNU_SOURCE
                              #include <fcntl.h>
                              #include <unistd.h>''' ],
  [ 'issetugid', 'HAVE_ISSETUGID', '#include <unistd.h>' ],
  [ 'timegm', 'HAVE_TIMEGM', '#include <time.h>' ],
  [ 'gmtime_r', 'HAVE_GMTIME_R', '#include <time.h>' ],
  [ 'strerror_r', 'HAVE_STRERROR_R', '#include <string.h>' ],
  [ 'lstat', 'HAVE_LSTAT', '''#include <sys/types.h>
                              #include <sys/stat.h>
                              #include <unistd.h>'''],
  [ 'strsignal', 'HAVE_STRSIGNAL', '#include <string.h>' ],
  [ 'vsnprintf', 'HAVE_VSNPRINTF', '#include <stdio.h>' ],
  [ 'stpcpy', 'HAVE_STPCPY', '#include <string.h>' ],
  [ 'strcasecmp', 'HAVE_STRCASECMP', '#include <strings.h>' ],
  [ 'strncasecmp', 'HAVE_STRNCASECMP', '#include <strings.h>' ],
  [ 'poll', 'HAVE_POLL', '#include <poll.h>' ],
  [ 'vasprintf', 'HAVE_VASPRINTF', '''#define _GNU_SOURCE
                                      #include <stdio.h>''' ],
  [ 'setenv', 'HAVE_SETENV', '#include <stdlib.h>' ],
  [ 'unsetenv', 'HAVE_UNSETENV', '#include <stdlib.h>' ],
  [ 'getc_unlocked', 'HAVE_GETC_UNLOCKED', '#include <stdio.h>' ],
  [ 'readlink', 'HAVE_READLINK', '#include <unistd.h>' ],
  [ 'symlink', 'HAVE_SYMLINK', '#include <unistd.h>' ],
  [ 'fdwalk', 'HAVE_FDWALK', '#include <stdlib.h>' ],
  [ 'memmem', 'HAVE_MEMMEM', '''#define _GNU_SOURCE
                                #include <string.h>''' ],
# FIXME: this check doesn't work right, meson detects that it exists even if it's unusable
#  [ 'lchmod', 'HAVE_LCHMOD', '''#include <sys/stat.h>
#                                #include <unistd.h>''' ],
  [ 'lchown', 'HAVE_LCHOWN', '#include <unistd.h>' ],
  [ 'fchmod', 'HAVE_FCHMOD', '#include <sys/stat.h>' ],
  [ 'fchown', 'HAVE_FCHOWN', '#include <unistd.h>' ],
  [ 'utimes', 'HAVE_UTIMES', '#include <sys/time.h>' ],
  [ 'getresuid', 'HAVE_GETRESUID', '''#define _GNU_SOURCE
                                      #include <unistd.h>''' ],
  [ 'getmntent_r', 'HAVE_GETMNTENT_R', '''#include <stdio.h>
                                          #include <mntent.h>'''],
  [ 'setmntent', 'HAVE_SETMNTENT', '''#include <stdio.h>
                                      #include <mntent.h>'''],
  [ 'endmntent', 'HAVE_ENDMNTENT', '''#include <stdio.h>
                                      #include <mntent.h>'''],
  [ 'hasmntopt', 'HAVE_HASMNTOPT', '''#include <stdio.h>
                                      #include <mntent.h>'''],
  [ 'getfsstat', 'HAVE_GETFSSTAT', '''#include <sys/param.h>
                                      #include <sys/ucred.h>
                                      #include <sys/mount.h''' ],
  [ 'getvfsstat', 'HAVE_GETVFSSTAT', '''#include <sys/types.h>
                                        #include <sys/statvfs.h>''' ],
  [ 'fallocate', 'HAVE_FALLOCATE', '''#define _GNU_SOURCE
                                      #include <fcntl.h>''' ],
  [ 'localtime_r', 'HAVE_LOCALTIME_R', '#include <time.h>' ],
  [ 'gmtime_r', 'HAVE_GMTIME_R', '#include <time.h>' ],
  [ 'getpwuid_r', 'HAVE_GETPWUID_R', '''#include <sys/types.h>
                                        #include <pwd.h>''' ],
  [ 'getgrgid_r', 'HAVE_GETGRGID_R', '''#include <sys/types.h>
                                        #include <grp.h>''' ],
  [ 'prlimit', 'HAVE_PRLIMIT', '''#define _GNU_SOURCE
                                  #include <sys/time.h>
                                  #include <sys/resource.h>''' ],
  [ 'snprintf', 'HAVE_SNPRINTF', '#include <stdio.h>' ],
  [ 'strnlen', 'HAVE_STRNLEN', '#include <string.h>' ],
  [ 'wcslen', 'HAVE_WCSLEN', '#include <wchar.h>' ],
  [ 'wcsnlen', 'HAVE_WCSNLEN', '#include <wchar.h>' ],
  [ 'mbrtowc', 'HAVE_MBRTOWC', '#include <wchar.h>' ],
  [ 'wcrtomb', 'HAVE_WCRTOMB', '#include <wchar.h>' ],
  [ 'newlocale', 'HAVE_NEWLOCALE', '#include <locale.h>' ],
  [ 'uselocale', 'HAVE_USELOCALE', '#include <locale.h>' ],
  [ 'strtod_l', 'HAVE_STRTOD_L', '''#define _GNU_SOURCE
                                    #include <stdlib.h>
                                    #ifdef HAVE_XLOCALE_H
                                    #include <xlocale.h>
                                    #endif''' ],
  [ 'strtoll_l', 'HAVE_STRTOLL_L', '''#define _GNU_SOURCE
                                      #include <stdlib.h>
                                      #include <limits.h>
                                      #ifdef HAVE_XLOCALE_H
                                      #include <xlocale.h>
                                      #endif''' ],
  [ 'strtoull_l', 'HAVE_STRTOULL_L', '''#define _GNU_SOURCE
                                        #include <stdlib.h>
                                        #include <limits.h>
                                        #ifdef HAVE_XLOCALE_H
                                        #include <xlocale.h>
                                        #endif''' ],
  [ 'inotify_init1', 'HAVE_INOTIFY_INIT1', '#include <sys/inotify.h>' ],
  [ 'kqueue', 'HAVE_KQUEUE', '#include <sys/event.h>' ],
  [ 'kevent', 'HAVE_KEVENT', '#include <sys/event.h>' ],
  [ 'getservent', 'HAVE_GETSERVENT', '#include <netdb.h>' ],
  [ 'getprotobyname_r', 'HAVE_GETPROTOBYNAME_R', '#include <netdb.h>' ],
  [ 'if_indextoname', 'HAVE_IF_INDEXTONAME', '#include <net/if.h>' ],
  [ 'if_nametoindex', 'HAVE_IF_NAMETOINDEX', '#include <net/if.h>' ],
  [ 'sendmmsg', 'HAVE_SENDMMSG', '''#define _GNU_SOURCE
                                    #include <sys/socket.h>''' ],
  [ 'recvmmsg', 'HAVE_RECVMMSG', '''#define _GNU_SOURCE
                                    #include <sys/socket.h>''' ],
]

if glib_conf.has('HAVE_SYS_STATVFS_H')
  functions += [ [ 'statvfs', 'HAVE_STATVFS', '#include <sys/statvfs.h>'] ]
endif
if glib_conf.has('HAVE_SYS_STATFS_H') or glib_conf.has('HAVE_SYS_MOUNT_H')
  functions += [ [ 'statfs', 'HAVE_STATFS', '''#ifdef HAVE_SYS_STATFS_H
                                               #include <sys/statfs.h>
                                               #endif
                                               #ifdef HAVE_SYS_MOUNT_H
                                               #include <sys/mount.h>
                                               #endif'''] ]
endif

# AIX splice is something else
if host_machine.system() != 'aix'
  functions += [
    [ 'splice', 'HAVE_SPLICE', '''#define _GNU_SOURCE
                                  #include <fcntl.h>''' ]
  ]
endif

foreach f : functions
  header_check_prefix = glib_conf_prefix
  if f.length() == 3
    header_check_prefix = header_check_prefix + f[2]
  endif
  if cc.has_function (f[0], prefix : header_check_prefix)
    glib_conf.set(f[1], 1)
    glib_conf_prefix = glib_conf_prefix + '#define @0@ 1\n'.format(f[1])
    set_variable('have_func_' + f[0], true)
  else
    set_variable('have_func_' + f[0], false)
  endif
endforeach

# Check whether to use statfs or statvfs
# Some systems have both statfs and statvfs, pick the most "native" for these
if have_func_statfs and have_func_statvfs
  # on solaris and irix, statfs doesn't even have the f_bavail field
  if not glib_conf.has('HAVE_STRUCT_STATFS_F_BAVAIL')
    have_func_statfs = false
  else
    # at least on linux, statfs is the actual syscall
    have_func_statvfs = false
  endif
endif
if have_func_statfs
  glib_conf.set('USE_STATFS', 1)
  stat_func_to_use = 'statfs'
elif have_func_statvfs
  glib_conf.set('USE_STATVFS', 1)
  stat_func_to_use = 'statvfs'
else
  stat_func_to_use = 'neither'
endif
message('Checking whether to use statfs or statvfs .. ' + stat_func_to_use)

# Mac OS X Carbon support
glib_have_carbon = cc.compiles('''#include <Carbon/Carbon.h>
                                  #include <CoreServices/CoreServices.h>''',
                               name : 'Mac OS X Carbon support')

if glib_have_carbon
  glib_conf.set('HAVE_CARBON', true)
  CARBON_LIBS='-Wl,-framework,Carbon' # FIXME: propagate to .pc files as well
else
  CARBON_LIBS=''
endif

# Check for nl_langinfo and CODESET
if cc.links('''#include <langinfo.h>
               int main (int argc, char ** argv) {
                 char *codeset = nl_langinfo (CODESET);
                 return 0;
               }''', name : 'nl_langinfo and CODESET')
  glib_conf.set('HAVE_LANGINFO_CODESET', 1)
endif

# Check for futex(2)
if cc.links('''#include <linux/futex.h>
               #include <sys/syscall.h>
               #include <unistd.h>
               int main (int argc, char ** argv) {
                 syscall (__NR_futex, NULL, FUTEX_WAKE, FUTEX_WAIT);
                 return 0;
               }''', name : 'futex(2) system call')
  glib_conf.set('HAVE_FUTEX', 1)
endif

# Check for eventfd(2)
if cc.links('''#include <sys/eventfd.h>
               #include <unistd.h>
               int main (int argc, char ** argv) {
                 eventfd (0, EFD_CLOEXEC);
                 return 0;
               }''', name : 'eventfd(2) system call')
  glib_conf.set('HAVE_EVENTFD', 1)
endif

clock_gettime_test_code = '''
  #include <time.h>
  struct timespec t;
  int main (int argc, char ** argv) {
    return clock_gettime(CLOCK_REALTIME, &t);
  }'''
# FIXME: or will librt be in the meson threads dep already?
librt = [ ]
if cc.links(clock_gettime_test_code, name : 'clock_gettime')
  glib_conf.set('HAVE_CLOCK_GETTIME', 1)
elif cc.links(clock_gettime_test_code, args : '-lrt', name : 'clock_gettime in librt')
  glib_conf.set('HAVE_CLOCK_GETTIME', 1)
  librt = find_library('rt')
endif

# if statfs() takes 2 arguments (Posix) or 4 (Solaris)
if have_func_statfs
  if cc.compiles(glib_conf_prefix + '''
                 #include <unistd.h>
                        #ifdef HAVE_SYS_PARAM_H
                        #include <sys/param.h>
                        #endif
                        #ifdef HAVE_SYS_VFS_H
                        #include <sys/vfs.h>
                        #endif
                        #ifdef HAVE_SYS_MOUNT_H
                        #include <sys/mount.h>
                        #endif
                        #ifdef HAVE_SYS_STATFS_H
                        #include <sys/statfs.h>
                        #endif
                        void some_func (void) {
                          struct statfs st;
                          statfs(NULL, &st);
                        }''', name : 'number of arguments to statfs() (n=2)')
    glib_conf.set('STATFS_ARGS', 2)
  elif cc.compiles(glib_conf_prefix + '''
                   #include <unistd.h>
                          #ifdef HAVE_SYS_PARAM_H
                          #include <sys/param.h>
                          #endif
                          #ifdef HAVE_SYS_VFS_H
                          #include <sys/vfs.h>
                          #endif
                          #ifdef HAVE_SYS_MOUNT_H
                          #include <sys/mount.h>
                          #endif
                          #ifdef HAVE_SYS_STATFS_H
                          #include <sys/statfs.h>
                          #endif
                          void some_func (void) {
                            struct statfs st;
                            statfs(NULL, &st, sizeof (st), 0);
                          }''', name : 'number of arguments to statfs() (n=4)')
    glib_conf.set('STATFS_ARGS', 4)
  else
    error('Unable to determine number of arguments to statfs()')
  endif
endif

# open takes O_DIRECTORY as an option
#AC_MSG_CHECKING([])
if cc.compiles('''#include <fcntl.h>
                  #include <sys/types.h>
                  #include <sys/stat.h>],
                  void some_func (void) {
                    open(0, O_DIRECTORY, 0);
                  }''', name : 'open() option O_DIRECTORY')
  glib_conf.set('HAVE_OPEN_O_DIRECTORY', 1)
endif

# Check whether there is a vsnprintf() function with C99 semantics installed.
# AC_FUNC_VSNPRINTF_C99
# Check whether there is a snprintf() function with C99 semantics installed.
# AC_FUNC_SNPRINTF_C99
if host_machine.system() == 'windows' and cc.get_id() == 'gcc' # FIXME: is it mingw on windows?
  # Unfortunately the mingw implementations of C99-style snprintf and vsnprintf
  # don't seem to be quite good enough, at least not in mingw-runtime-3.14.
  # (Sorry, I don't know exactly what is the problem, but it is related to
  # floating point formatting and decimal point vs. comma.)
  # The simple tests in AC_FUNC_VSNPRINTF_C99 and AC_FUNC_SNPRINTF_C99 aren't
  # rigorous enough to notice, though.
  glib_conf.set('HAVE_C99_SNPRINTF', false)
  glib_conf.set('HAVE_C99_VSNPRINTF', false)
else
  vsnprintf_c99_test_code = '''
#include <stdio.h>
#include <stdarg.h>

int
doit(char * s, ...)
{
  char buffer[32];
  va_list args;
  int r;

  va_start(args, s);
  r = vsnprintf(buffer, 5, s, args);
  va_end(args);

  if (r != 7)
    exit(1);

  /* AIX 5.1 and Solaris seems to have a half-baked vsnprintf()
     implementation. The above will return 7 but if you replace
     the size of the buffer with 0, it borks! */
  va_start(args, s);
  r = vsnprintf(buffer, 0, s, args);
  va_end(args);

  if (r != 7)
    exit(1);

  exit(0);
}

int
main(void)
{
  doit("1234567");
  exit(1);
}'''

  rres = cc.run(vsnprintf_c99_test_code, name : 'C99 vsnprintf')
  if rres.compiled() and rres.returncode() == 0
    glib_conf.set('HAVE_C99_VSNPRINTF', 1)
  endif

  snprintf_c99_test_code = '''
#include <stdio.h>
#include <stdarg.h>

int
doit()
{
  char buffer[32];
  va_list args;
  int r;

  r = snprintf(buffer, 5, "1234567");

  if (r != 7)
    exit(1);

  r = snprintf(buffer, 0, "1234567");

  if (r != 7)
    exit(1);

  r = snprintf(NULL, 0, "1234567");

  if (r != 7)
    exit(1);

  exit(0);
}

int
main(void)
{
  doit();
  exit(1);
}'''

  rres = cc.run(snprintf_c99_test_code, name : 'C99 snprintf')
  if rres.compiled() and rres.returncode() == 0
    glib_conf.set('HAVE_C99_SNPRINTF', 1)
  endif
endif

if host_machine.system() == 'windows'
  glib_conf.set('EXEEXT', '".exe"')
else
  glib_conf.set('EXEEXT', '""')
endif

# Check whether the printf() family supports Unix98 %n$ positional parameters
# AC_FUNC_PRINTF_UNIX98
printf_unix98_test_code = '''
#include <stdio.h>

int
main (void)
{
  char buffer[128];

  sprintf (buffer, "%2\$d %3\$d %1\$d", 1, 2, 3);
  if (strcmp ("2 3 1", buffer) == 0)
    exit (0);
  exit (1);
}'''
rres = cc.run(printf_unix98_test_code, name : 'printf unix98 position parameters')
if rres.compiled() and rres.returncode() == 0
  glib_conf.set('HAVE_UNIX98_PRINTF', 1)
endif

# Check for nl_langinfo and CODESET
if cc.links('''#include <langinfo.h>
               int main (int argc, char ** argv) {
                 char *codeset = nl_langinfo (CODESET);
                 return 0;
               }''', name : 'nl_langinfo (CODESET)')
  glib_conf.set('HAVE_CODESET', 1)
endif

# Check for nl_langinfo and LC_TIME parts that are needed in gdatetime.c
if cc.links('''#include <langinfo.h>
               int main (int argc, char ** argv) {
                 char *str;
                 str = nl_langinfo (PM_STR);
                 str = nl_langinfo (D_T_FMT);
                 str = nl_langinfo (D_FMT);
                 str = nl_langinfo (T_FMT);
                 str = nl_langinfo (T_FMT_AMPM);
                 str = nl_langinfo (MON_1);
                 str = nl_langinfo (ABMON_12);
                 str = nl_langinfo (DAY_1);
                 str = nl_langinfo (ABDAY_7);
                 return 0;
               }''', name : 'nl_langinfo (PM_STR)')
  glib_conf.set('HAVE_LANGINFO_TIME', 1)
endif
if cc.links('''#include <langinfo.h>
               int main (int argc, char ** argv) {
                 char *str;
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT0_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT1_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT2_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT3_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT4_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT5_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT6_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT7_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT8_MB);
                 str = nl_langinfo (_NL_CTYPE_OUTDIGIT9_MB);
                 return 0;
               }''', name : 'nl_langinfo (_NL_CTYPE_OUTDIGITn_MB)')
  glib_conf.set('HAVE_LANGINFO_OUTDIGIT', 1)
endif

# Check if C compiler supports the 'signed' keyword
if not cc.compiles('''signed char x;''', name : 'signed')
  glib_conf.set('signed', '/* NOOP */')
endif

# Check if the ptrdiff_t type exists
if cc.compiles('''#include <stddef.h>
                  ptrdiff_t x;''', name : 'ptrdiff_t')
  glib_conf.set('HAVE_PTRDIFF_T', 1)
endif

# Check for sig_atomic_t type
if cc.links('''#include <signal.h>
               #include <sys/types.h>
               sig_atomic_t val = 42;
               int main (int argc, char ** argv) {
                 return val == 42 ? 0 : 1;
               }''', name : 'sig_atomic_t')
  glib_conf.set('HAVE_SIG_ATOMIC_T', 1)
endif

# Check if 'long long' works.
# jm_AC_TYPE_LONG_LONG
if cc.compiles('''long long ll = 1LL;
                  int i = 63;
                  int some_func (void) {
                    long long llmax = (long long) -1;
                    return ll << i | ll >> i | llmax / ll | llmax % ll;
                  }''', name : 'long long')
  glib_conf.set('HAVE_LONG_LONG', 1)
  have_long_long = true
else
  have_long_long = false
endif

# Test whether the compiler supports the 'long double' type.
if cc.compiles('''/* The Stardent Vistra knows sizeof(long double), but does not support it.  */
                  long double foo = 0.0;
                  /* On Ultrix 4.3 cc, long double is 4 and double is 8.  */
                  int array [2*(sizeof(long double) >= sizeof(double)) - 1];''',
               name : 'long double')
  glib_conf.set('HAVE_LONG_DOUBLE', 1)
endif

#dnl Test whether <stddef.h> has the 'wchar_t' type.
if cc.compiles('''#include <stddef.h>
                  wchar_t foo = (wchar_t)'\0';''',
               name : 'wchar_t')
  glib_conf.set('HAVE_WCHAR_T', 1)
endif

# Test whether <wchar.h> has the 'wint_t' type.
if cc.compiles('''#include <wchar.h>
                  wint_t foo = (wchar_t)'\0';''',
               name : 'wint_t')
  glib_conf.set('HAVE_WINT_T', 1)
endif

found_uintmax_t = false

# Define HAVE_INTTYPES_H_WITH_UINTMAX if <inttypes.h> exists,
# doesn't clash with <sys/types.h>, and declares uintmax_t.
# jm_AC_HEADER_INTTYPES_H
if cc.compiles('''#include <sys/types.h>
                  #include <inttypes.h>
                  void some_func (void) {
                    uintmax_t i = (uintmax_t) -1;
                  }''', name : 'uintmax_t in inttypes.h')
  glib_conf.set('HAVE_INTTYPES_H_WITH_UINTMAX', 1)
  found_uintmax_t = true
endif

# Define HAVE_STDINT_H_WITH_UINTMAX if <stdint.h> exists,
# doesn't clash with <sys/types.h>, and declares uintmax_t.
# jm_AC_HEADER_STDINT_H
if cc.compiles('''#include <sys/types.h>
                  #include <stdint.h>
                  void some_func (void) {
                    uintmax_t i = (uintmax_t) -1;
                  }''', name : 'uintmax_t in stdint.h')
  glib_conf.set('HAVE_STDINT_H_WITH_UINTMAX', 1)
  found_uintmax_t = true
endif

# Define intmax_t to 'long' or 'long long'
# if it is not already defined in <stdint.h> or <inttypes.h>.
# For simplicity, we assume that a header file defines 'intmax_t' if and
# only if it defines 'uintmax_t'.
if found_uintmax_t
  glib_conf.set('HAVE_INTMAX_T', 1)
elif have_long_long
  glib_conf.set('intmax_t', 'long long')
else
  glib_conf.set('intmax_t', 'long')
endif

char_size = cc.sizeof('char')
short_size = cc.sizeof('short')
int_size = cc.sizeof('int')
voidp_size = cc.sizeof('void*')
long_size = cc.sizeof('long')
if have_long_long
  long_long_size = cc.sizeof('long long')
else
  long_long_size = 0
endif
sizet_size = cc.sizeof('size_t')
ssizet_size = cc.sizeof('ssize_t')

char_align = cc.alignment('char')
short_align = cc.alignment('short')
int_align = cc.alignment('int')
voidp_align = cc.alignment('void*')
long_align = cc.alignment('long')
sizet_align = cc.alignment('size_t')

glib_conf.set('ALIGNOF_UNSIGNED_LONG', long_align)

glib_conf.set('SIZEOF_CHAR', char_size)
glib_conf.set('SIZEOF_INT', int_size)
glib_conf.set('SIZEOF_SHORT', short_size)
glib_conf.set('SIZEOF_LONG', long_size)
glib_conf.set('SIZEOF_LONG_LONG', long_long_size)
glib_conf.set('SIZEOF_SIZE_T', sizet_size)
glib_conf.set('SIZEOF_SSIZE_T', ssizet_size)
glib_conf.set('SIZEOF_VOID_P', voidp_size)

if long_size == 2
  # configure.ac does not work if this is the case. So we don't either.
  gint16 = 'long'
endif
if int_size == 2
  gint16 = 'int'
  gint16_modifier='""'
  gint16_format='"i"'
  guint16_format='"u"'
endif
if short_size == 2
  gint16 = 'short'
  gint16_modifier='"h"'
  gint16_format='"hi"'
  guint16_format='"hu"'
endif
glibconfig_conf.set('gint16', gint16)
glibconfig_conf.set('gint16_modifier', gint16_modifier)
glibconfig_conf.set('gint16_format', gint16_format)
glibconfig_conf.set('guint16_format', guint16_format)

if long_size == 4
  gint32 = 'long'
  gint32_modifier='"l"'
  gint32_format='"li"'
  guint32_format='"lu"'
  guint32_align = long_align
endif
if int_size == 4
  gint32 = 'int'
  gint32_modifier='""'
  gint32_format='"i"'
  guint32_format='"u"'
  guint32_align = int_align
endif
if short_size == 4
  gint32 = 'short'
  gint32_modifier='"h"'
  gint32_format='"hi"'
  guint32_format='"hu"'
  guint32_align = short_align
endif
glibconfig_conf.set('gint32', gint32)
glibconfig_conf.set('gint32_modifier', gint32_modifier)
glibconfig_conf.set('gint32_format', gint32_format)
glibconfig_conf.set('guint32_format', guint32_format)
glib_conf.set('ALIGNOF_GUINT32', guint32_align)

if long_long_size == 8
  # Currently broken.
  gint64 = 'long long'
endif
if long_size == 8
  gint64 = 'long'
  gint64_modifier='"l"'
  gint64_format='"li"'
  guint64_format='"lu"'
  glib_extension=''
  gint64_constant='(val##L)'
  guint64_constant='(val##UL)'
  guint64_align = long_align
endif
if int_size == 8
  gint64 = 'int'
  gint64_modifier='""'
  gint64_format='"i"'
  guint64_format='"u"'
  glib_extension=''
  gint64_constant='(val)'
  guint64_constant='(val)'
  guint64_align = int_align
endif
if short_size == 8
  # Another one broken in configure.ac
  gint64 = 'short'
endif
glibconfig_conf.set('gint64', gint64)
glibconfig_conf.set('gint64_modifier', gint64_modifier)
glibconfig_conf.set('gint64_format', gint64_format)
glibconfig_conf.set('guint64_format', guint64_format)
glibconfig_conf.set('gint64_constant', gint64_constant)
glibconfig_conf.set('guint64_constant', guint64_constant)
glib_conf.set('ALIGNOF_GUINT64', guint64_align)

if host_machine.system() == 'windows'
  glibconfig_conf.set('g_pid_type', 'void*')
  if host_machine.cpu_family() == 'x86_64'
    glibconfig_conf.set('g_pollfd_format', '"%#I64x"')
  else
    glibconfig_conf.set('g_pollfd_format', '"%#x"')
  endif
else
  glibconfig_conf.set('g_pid_type', 'int')
  glibconfig_conf.set('g_pollfd_format', '"%d"')
endif

if sizet_size == short_size
  glibconfig_conf.set('glib_size_type_define', 'short')
  glibconfig_conf.set('gsize_modifier', '"h"')
  glibconfig_conf.set('gssize_modifier', '"h"')
  glibconfig_conf.set('gsize_format', '"hu"')
  glibconfig_conf.set('gssize_format', '"hi"')
  glibconfig_conf.set('glib_msize_type', 'SHRT')
elif sizet_size == int_size
  glibconfig_conf.set('glib_size_type_define', 'int')
  glibconfig_conf.set('gsize_modifier', '""')
  glibconfig_conf.set('gssize_modifier', '""')
  glibconfig_conf.set('gsize_format', '"u"')
  glibconfig_conf.set('gssize_format', '"i"')
  glibconfig_conf.set('glib_msize_type', 'INT')
elif sizet_size == long_size
  glibconfig_conf.set('glib_size_type_define', 'long')
  glibconfig_conf.set('gsize_modifier', '"l"')
  glibconfig_conf.set('gssize_modifier', '"l"')
  glibconfig_conf.set('gsize_format', '"lu"')
  glibconfig_conf.set('gssize_format', '"li"')
  glibconfig_conf.set('glib_msize_type', 'LONG')
elif sizet_size == long_long_size
  glibconfig_conf.set('glib_size_type_define', 'long long')
  glibconfig_conf.set('gsize_modifier', '"I64"')
  glibconfig_conf.set('gssize_modifier', '"I64"')
  glibconfig_conf.set('gsize_format', '"I64u"')
  glibconfig_conf.set('gssize_format', '"I64i"')
  glibconfig_conf.set('glib_msize_type', 'INT64')
else
  error('Could not determine size of size_t.')
endif

if voidp_size == int_size
  glibconfig_conf.set('glib_intptr_type_define', 'int')
  glibconfig_conf.set('gintptr_modifier', '""')
  glibconfig_conf.set('gintptr_format', '"i"')
  glibconfig_conf.set('guintptr_format', '"u"')
  glibconfig_conf.set('glib_gpi_cast', '(gint)')
  glibconfig_conf.set('glib_gpui_cast', '(guint)')
elif voidp_size == long_size
  glibconfig_conf.set('glib_intptr_type_define', 'long')
  glibconfig_conf.set('gintptr_modifier', '"l"')
  glibconfig_conf.set('gintptr_format', '"li"')
  glibconfig_conf.set('guintptr_format', '"lu"')
  glibconfig_conf.set('glib_gpi_cast', '(glong)')
  glibconfig_conf.set('glib_gpui_cast', '(gulong)')
elif voidp_size == long_long_size
  glibconfig_conf.set('glib_intptr_type_define', 'long long')
  glibconfig_conf.set('gintptr_modifier', '"I64"')
  glibconfig_conf.set('gintptr_format', '"I64i"')
  glibconfig_conf.set('guintptr_format', '"I64u"')
  glibconfig_conf.set('glib_gpi_cast', '(gint64)')
  glibconfig_conf.set('glib_gpui_cast', '(guint64)')
# FIXME
#elif voidp_size == __int64_size
#  glib_intptr_type_define=__int64
#  gintptr_modifier='"I64"'
#  gintptr_format='"I64i"'
#  guintptr_format='"I64u"'
#  glib_gpi_cast='(gint64)'
#  glib_gpui_cast='(guint64)'
else
  error('Could not determine size of void *')
endif

glibconfig_conf.set('gintbits', int_size * 8)
glibconfig_conf.set('glongbits', long_size * 8)
glibconfig_conf.set('gsizebits', sizet_size * 8)
glibconfig_conf.set('gssizebits', ssizet_size * 8)

# FIXME: maybe meson should tell us the libsuffix?
if host_machine.system() == 'windows'
  g_module_suffix = 'dll'
elif host_machine.system() == 'darwin'
  g_module_suffix = 'dylib'
else
  g_module_suffix = 'so'
endif
glibconfig_conf.set('g_module_suffix', g_module_suffix)

glibconfig_conf.set('GLIB_MAJOR_VERSION', major_version)
glibconfig_conf.set('GLIB_MINOR_VERSION', minor_version)
glibconfig_conf.set('GLIB_MICRO_VERSION', micro_version)

glibconfig_conf.set('glib_void_p', voidp_size)
glibconfig_conf.set('glib_long', long_size)
glibconfig_conf.set('glib_size_t', sizet_size)
glibconfig_conf.set('glib_ssize_t', ssizet_size)
if host_machine.endian() == 'big'
  glibconfig_conf.set('g_byte_order', 'G_BIG_ENDIAN')
  glibconfig_conf.set('g_bs_native', 'BE')
  glibconfig_conf.set('g_bs_alien', 'LE')
else
  glibconfig_conf.set('g_byte_order', 'G_LITTLE_ENDIAN')
  glibconfig_conf.set('g_bs_native', 'LE')
  glibconfig_conf.set('g_bs_alien', 'BE')
endif

# === va_copy checks ===
# we currently check for all three va_copy possibilities, so we get
# all results in config.log for bug reports.

foreach try_func : [ '__va_copy', 'va_copy' ]
  if cc.compiles('''#include <stdarg.h>
                    #include <stdlib.h>
                    void f (int i, ...) {
                    va_list args1, args2;
                    va_start (args1, i);
                    @0@ (args2, args1);
                    if (va_arg (args2, int) != 42 || va_arg (args1, int) != 42)
                      exit (1);
                    va_end (args1); va_end (args2);
                    }
                    int main() {
                      f (0, 42);
                      return 0;
                    }'''.format(try_func),
                    name : try_func + ' check')
    va_copy_func = try_func
  endif
endforeach
glib_conf.set('G_VA_COPY', va_copy_func)
glib_vacopy = '#define G_VA_COPY ' + va_copy_func

va_list_val_copy_prog = '''
  #include <stdarg.h>
  #include <stdlib.h>
  void f (int i, ...) {
    va_list args1, args2;
    va_start (args1, i);
    args2 = args1;
    if (va_arg (args2, int) != 42 || va_arg (args1, int) != 42)
      exit (1);
    va_end (args1); va_end (args2);
  }
  int main() {
    f (0, 42);
    return 0;
  }'''

# We do this in two steps so if compilation fails already it looks less alarming
glib_va_val_copy = false
if cc.compiles(va_list_val_copy_prog, name : 'va_lists can be copied as values')
  # FIXME: what to do when cross-compiling?
  if cc.run(va_list_val_copy_prog, 'va_lists can be copied as values').returncode() == 0
    glib_va_val_copy = true
  endif
endif
if not glib_va_val_copy
  glib_va_val_copy = false
  glib_vacopy = glib_vacopy + '\n#define G_VA_COPY_AS_ARRAY 1'
  glib_conf.set('G_VA_COPY_AS_ARRAY', 1)
endif
glibconfig_conf.set('glib_vacopy', glib_vacopy)

# check for flavours of varargs macros
g_have_iso_c_varargs = cc.compiles('''
  void some_func (void) {
    int a(int p1, int p2, int p3);
    #define call_a(...) a(1,__VA_ARGS__)
    call_a(2,3);
  }''', name : 'ISO C99 varargs macros in C')

if g_have_iso_c_varargs
  glibconfig_conf.set('g_have_iso_c_varargs', '''
#ifndef __cplusplus
# define G_HAVE_ISO_VARARGS 1
#endif''')
endif

g_have_iso_cxx_varargs = cxx.compiles('''
  void some_func (void) {
    int a(int p1, int p2, int p3);
    #define call_a(...) a(1,__VA_ARGS__)
    call_a(2,3);
  }''', name : 'ISO C99 varargs macros in C++')

if g_have_iso_cxx_varargs
  glibconfig_conf.set('g_have_iso_cxx_varargs', '''
#ifdef __cplusplus
# define G_HAVE_ISO_VARARGS 1
#endif''')
endif

g_have_gnuc_varargs = cc.compiles('''
  void some_func (void) {
    int a(int p1, int p2, int p3);
    #define call_a(params...) a(1,params)
    call_a(2,3);
  }''', name : 'GNUC varargs macros')

if cc.has_header('alloca.h')
  glibconfig_conf.set('GLIB_HAVE_ALLOCA_H', true)
endif
has_syspoll = cc.has_header('sys/poll.h')
has_systypes = cc.has_header('sys/types.h')
if has_syspoll
  glibconfig_conf.set('GLIB_HAVE_SYS_POLL_H', true)
endif

if has_syspoll and has_systypes
  templ = '''#include<sys/poll.h>
#include<sys/types.h>
#include<stdio.h>
int main(int argc, char **argv) {
  printf("%d\n", (int)@0@);
  return 0;
}'''
  value_POLLIN = cc.run(templ.format('POLLIN')).stdout().strip()
  value_POLLOUT = cc.run(templ.format('POLLOUT')).stdout().strip()
  value_POLLPRI = cc.run(templ.format('POLLPRI')).stdout().strip()
  value_POLLERR = cc.run(templ.format('POLLERR')).stdout().strip()
  value_POLLHUP = cc.run(templ.format('POLLHUP')).stdout().strip()
  value_POLLNVAL = cc.run(templ.format('POLLNVAL')).stdout().strip()
else
  # FIXME?
  error('FIX POLL* defines')
  value_POLLIN = 1
  value_POLLOUT = 4
  value_POLLPRI = 2
  value_POLLERR = 8
  value_POLLHUP = 16
  value_POLLNVAL = 32
endif

glibconfig_conf.set('g_pollin', value_POLLIN)
glibconfig_conf.set('g_pollout', value_POLLOUT)
glibconfig_conf.set('g_pollpri', value_POLLPRI)
glibconfig_conf.set('g_pollerr', value_POLLERR)
glibconfig_conf.set('g_pollhup', value_POLLHUP)
glibconfig_conf.set('g_pollnval', value_POLLNVAL)

# Internet address families
# FIXME: what about Cygwin (G_WITH_CYGWIN)
if host_machine.system() == 'windows'
  glib_inet_includes= '''
#include <winsock2.h>
'''
else
  glib_inet_includes='''
#include <sys/types.h>
#include <sys/socket.h>
'''
endif

net_defines = [
  [ 'AF_UNIX', 'g_af_unix' ],
  [ 'AF_INET', 'g_af_inet' ],
  [ 'AF_INET6', 'g_af_inet6' ],
  [ 'MSG_OOB', 'g_msg_oob' ],
  [ 'MSG_PEEK', 'g_msg_peek' ],
  [ 'MSG_DONTROUTE', 'g_msg_dontroute' ],
]
foreach d : net_defines
  templ = '''@0@
#include <stdio.h>
int main(int argc, char **argv) {
  printf("%d\n", (int)@1@);
  return 0;
}'''
  # FIXME: fix for cross-compilation
  if meson.is_cross_build()
    error('Fix sys define detection for cross build')
  endif
  val = cc.run(templ.format(glib_inet_includes, d[0])).stdout().strip()
  glibconfig_conf.set(d[1], val)
endforeach

glibconfig_conf.set('GLIB_USING_SYSTEM_PRINTF', true) # FIXME!

# We need a more robust approach here...
host_cpu_family = host_machine.cpu_family()
if host_cpu_family == 'x86' or host_cpu_family == 'x86_64' or host_cpu_family == 's390' or host_cpu_family == 's390x' or host_cpu_family.startswith('arm') or host_cpu_family.startswith('crisv32') or host_cpu_family.startswith('etrax')
  glib_memory_barrier_needed = false
elif host_cpu_family.startswith('sparc') or host_cpu_family.startswith('alpha') or host_cpu_family.startswith('powerpc') or host_cpu_family == 'ia64'
  glib_memory_barrier_needed = true
else
  # FIXME: should warn perhaps
  glib_memory_barrier_needed = true
endif
glibconfig_conf.set('G_ATOMIC_OP_MEMORY_BARRIER_NEEDED', glib_memory_barrier_needed)

atomictest = '''void func() {
  volatile int atomic = 2;
  __sync_bool_compare_and_swap (&atomic, 2, 3);
}
'''
if cc.compiles(atomictest)
  glibconfig_conf.set('G_ATOMIC_LOCK_FREE', true)
else
  glibconfig_conf.set('G_ATOMIC_LOCK_FREE', false)
endif

# === Threads ===

# Let meson figure out all this business and whether -pthread or whatnot is needed
# FIXME: probably needs more tweaking in meson for things like -D_REENTRANT etc.
thread_dep = dependency('threads')

# Determination of thread implementation
if host_machine.system() == 'windows'
  glibconfig_conf.set('g_threads_impl_def', 'WIN32')
  glib_conf.set('THREADS_WIN32', 1)
else
  glibconfig_conf.set('g_threads_impl_def', 'POSIX')
  glib_conf.set('THREADS_POSIX', 1)
endif

# FIXME: how to do this when cross-compiling?
# FIXME: we should make it print the result and always return 0, so that
# the output in meson shows up as green
stack_grows_check_prog = '''
  volatile int *a = 0, *b = 0;
  void f (int i) {
    volatile int x = 5;
    if (i == 0)
      b = &x;
    else
      f (i - 1);
  }
  int main () {
    volatile int y = 7;
    a = &y;
    f (100);
    return b > a ? 0 : 1;
  }'''
stack_grows_run_result = cc.run(stack_grows_check_prog, name : 'stack grows check')
if stack_grows_run_result.compiled() and stack_grows_run_result.returncode() == 0
  glibconfig_conf.set('G_HAVE_GROWING_STACK', 1)
else
  glibconfig_conf.set('G_HAVE_GROWING_STACK', 0)
endif

# Tests for iconv
# USE_LIBICONV_GNU: Using GNU libiconv
# USE_LIBICONV_NATIVE: Using a native implementation of iconv in a separate library
# On Windows we use a native implementation
if host_machine.system() == 'windows'
  glib_conf.set('USE_LIBICONV_NATIVE', true)
  libiconv = find_library('iconv')
else
  # FIXME: add option as well
  if cc.has_function('iconv_open', prefix : '#include <iconv.h>', name : 'C library iconv')
    libiconv = [ ]
  # FIXME: this is all wrong, meson needs a more intuitive check for this, like cc.has_lib_function() or such
  elif cc.compiles('''#include <iconv.h>
                      void somefunc (void) {
                        libiconv_open("foo", "bar")
                      }''', args : '-liconv', name : 'GNU libiconv')
    glib_conf.set('USE_LIBICONV_GNU', true)
    libiconv = find_library('iconv')
  elif cc.compiles('''#include <iconv.h>
                      void somefunc (void) {
                        iconv_open("foo", "bar")
                      }''', args : '-liconv', name : 'other libiconv')
    glib_conf.set('USE_LIBICONV_NATIVE', true)
    libiconv = find_library('iconv')
  else
    error('No iconv() implementation found in C library or libiconv')
  endif
endif

pcre = dependency('libpcre') # Should check for Unicode support, too. FIXME
glib_conf.set('USE_SYSTEM_PCRE', '/**/')

libm = find_library('m')
libffi_dep = dependency('libffi', version : '>= 3.0.0')
libz_dep = dependency('zlib')

# FIXME: defines in config.h that are not actually used anywhere
# (we add them for now to minimise the diff)
glib_conf.set('HAVE_DLFCN_H', 1)
glib_conf.set('__EXTENSIONS__', 1)
glib_conf.set('STDC_HEADERS', 1)
# THREADS_NONE
glib_conf.set('SIZEOF___INT64', 0)

subdir('glib')
subdir('gobject')
subdir('gthread')
subdir('gmodule')
subdir('gio')

configure_file(input : 'config.h.meson', output : 'config.h', configuration : glib_conf)
