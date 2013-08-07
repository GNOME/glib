project('glib', 'c')

cc = meson.get_compiler('c')

major_version = 3
minor_version = 36
micro_version = 3
interface_version = 3

glib_version = '3.36.3'
inc_dirs = include_directories('.', 'glib', 'gmodule')

glib_conf = configuration_data()
glibconfig_conf = configuration_data()

glib_conf.set('GLIB_MAJOR_VERSION', major_version)
glib_conf.set('GLIB_MINOR_VERSION', minor_version)
glib_conf.set('GLIB_MICRO_VERSION', micro_version)
glib_conf.set('GLIB_INTERFACE_AGE', micro_version)
glib_conf.set('GLIB_BINARY_AGE', 363)
glib_conf.set('GETTEXT_PACKAGE', '"glib20"')
glib_conf.set('ENABLE_NLS', 1)
glib_conf.set('HAVE_VASPRINTF', 1)
glib_conf.set('HAVE_VPRINTF', 1)
glib_conf.set('HAVE_GOOD_PRINTF', 1)

if run_command('xgettext', '--help').returncode() != 0
  error('GNU Gettext required currently.')
endif
glib_conf.set('HAVE_GETTEXT', 1)
glib_conf.set('GLIB_LOCALE_DIR', '"share/locale"')

if cc.has_header('locale.h')
  glib_conf.set('HAVE_LOCALE_H', 1)
  lc_msg_test = '''#include<locale.h>
void foo() { LC_MESSAGES; }
'''
  if cc.compiles(lc_msg_test)
    glib_conf.set('HAVE_LC_MESSAGES', 1)
  endif
endif
if cc.has_header('xlocale.h')
  glib_conf.set('HAVE_XLOCALE_H', 1)
endif

if host.name() == 'windows'
  glib_os = '''#define G_OS_WIN32
#define G_PLATFORM_WIN32'''
else
  glib_os = '#define G_OS_UNIX'
endif
glibconfig_conf.set('glib_os', glib_os)

glib_conf.set('HAVE_STDIO_H', 1)
glib_conf.set('HAVE_STDDEF_H', 1)
if cc.has_header('dirent.h')
  glib_conf.set('HAVE_DIRENT_H', 1)
endif
if cc.has_header('float.h')
  glib_conf.set('HAVE_FLOAT_H', 1)
endif
if cc.has_header('limits.h')
  glib_conf.set('HAVE_LIMITS_H', 1)
endif
if cc.has_header('pwd.h')
  glib_conf.set('HAVE_PWD_H', 1)
endif
if cc.has_header('grp.h')
  glib_conf.set('HAVE_GRP_H', 1)
endif
if cc.has_header('sys/param.h')
  glib_conf.set('HAVE_SYS_PARAM_H', 1)
  sysparam_include = '#include<sys/param.h>'
else
  sysparam_include = ''
endif
if cc.has_header('poll.h')
  glib_conf.set('HAVE_POLL_H', 1)
endif
if cc.has_header('sys/resource.h')
  glib_conf.set('HAVE_SYS_RESOURCE_H', 1)
endif

if cc.has_header('sys/time.h')
  glib_conf.set('HAVE_SYS_TIME_H', 1)
endif
if cc.has_header('sys/times.h')
  glib_conf.set('HAVE_SYS_TIMES_H', 1)
endif
if cc.has_header('sys/wait.h')
  glib_conf.set('HAVE_SYS_WAIT_H', 1)
endif
if cc.has_header('unistd.h')
  glib_conf.set('HAVE_UNISTD_H', 1)
endif
if cc.has_header('values.h')
  glib_conf.set('HAVE_VALUES_H', 1)
endif


if cc.has_header('sys/select.h')
  glib_conf.set('HAVE_SYS_SELECT_H', 1)
endif
if cc.has_header('sys/types.h')
  glib_conf.set('HAVE_SYS_TYPES_H', 1)
endif
if cc.has_header('stdint.h')
  glib_conf.set('HAVE_STDINT_H', 1)
endif
if cc.has_header('inttypes.h')
  glib_conf.set('HAVE_INTTYPES_H', 1)
endif
if cc.has_header('sched.h')
  glib_conf.set('HAVE_SCHED_H', 1)
endif
if cc.has_header('malloc.h')
  glib_conf.set('HAVE_MALLOC_H', 1)
endif

if cc.has_header('sys/vfs.h')
  glib_conf.set('HAVE_SYS_VFS_H', 1)
endif
if cc.has_header('sys/vmount.h')
  glib_conf.set('HAVE_SYS_VMOUNT_H', 1)
endif
if cc.has_header('sys/statvfs.h')
  glib_conf.set('HAVE_SYS_STATVFS_H', 1)
endif
if cc.has_header('sys/filio.h')
  glib_conf.set('HAVE_SYS_FILIO_H', 1)
endif

if cc.has_header('mntent.h')
  glib_conf.set('HAVE_MNTENT_H', 1)
endif
if cc.has_header('sys/mnttab.h')
  glib_conf.set('HAVE_SYS_MNTTAB_H', 1)
endif
if cc.has_header('sys/vfstab.h')
  glib_conf.set('HAVE_SYS_VFSTAB_H', 1)
endif
if cc.has_header('sys/mntctl.h')
  glib_conf.set('HAVE_SYS_mntctl_H', 1)
endif
if cc.has_header('fstab.h')
  glib_conf.set('HAVE_FSTAB_H', 1)
endif

if cc.has_header('sys/uio.h')
  glib_conf.set('HAVE_SYS_UIO_H', 1)
endif
if cc.has_header('sys/mkdev.h')
  glib_conf.set('HAVE_SYS_MKDEV_H', 1)
endif

if cc.has_header('linux/magic.h')
  glib_conf.set('HAVE_LINUX_MAGIC_H', 1)
endif

if cc.has_header('sys/prctl.h')
  glib_conf.set('HAVE_SYS_PRCTL_H', 1)
endif

if cc.has_header('sys/mount.h', prefix : sysparam_include)
  glib_conf.set('HAVE_SYS_MOUNT_H', 1)
endif
if cc.has_header('sys/sysctl.h', prefix : sysparam_include)
  glib_conf.set('HAVE_SYS_SYSCTL_H', 1)
endif

if cc.has_header('xlocale.h')
  glib_conf.set('HAVE_XLOCALE_H', 1)
endif

if cc.has_member('struct stat', 'st_mtimensec', prefix : '#include<sys/stat.h>')
  glib_conf.set('HAVE_STRUCT_STAT_ST_MTIMENSEC', 1)
endif

if cc.has_member('struct stat', 'st_mtim.tv_nsec', prefix : '#include<sys/stat.h>')
  glib_conf.set('HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC', 1)
endif

if cc.has_member('struct stat', 'st_atimensec', prefix : '#include<sys/stat.h>')
  glib_conf.set('HAVE_STRUCT_STAT_ST_ATIMENSEC', 1)
endif

if cc.has_member('struct stat', 'st_atim.tv_nsec', prefix : '#include<sys/stat.h>')
  glib_conf.set('HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC', 1)
endif

if cc.has_member('struct stat', 'st_ctimensec', prefix : '#include<sys/stat.h>')
  glib_conf.set('HAVE_STRUCT_STAT_ST_CTIMENSEC', 1)
endif

if cc.has_member('struct stat', 'st_ctim.tv_nsec', prefix : '#include<sys/stat.h>')
  glib_conf.set('HAVE_STRUCT_STAT_ST_CTIM_TV_NSEC', 1)
endif

if cc.has_member('struct stat', 'st_ctim.tv_nsec', prefix : '#include<sys/stat.h>')
  glib_conf.set('HAVE_STRUCT_STAT_ST_CTIM_TV_NSEC', 1)
endif

# Fixme, build properly.
blkprefix = '''#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/statfs.h>
#include <sys/param.h>
#include <sys/mount.h>
'''

if cc.has_member('struct stat', 'blksize', prefix : blkprefix)
  glib_conf.set('HAVE_STRUCT_STAT_BLKSIZE', 1)
endif
if cc.has_member('struct stat', 'st_blocks', prefix : blkprefix)
  glib_conf.set('HAVE_STRUCT_STAT_BLOCKS', 1)
endif
if cc.has_member('struct stat', 'f_fstypename', prefix : blkprefix)
  glib_conf.set('HAVE_STRUCT_STAT_F_FSTYPENAME', 1)
endif
if cc.has_member('struct stat', 'f_bavail', prefix : blkprefix)
  glib_conf.set('HAVE_STRUCT_STAT_F_BAVAIL', 1)
endif

if cc.has_header('string.h')
  glib_conf.set('HAVE_STRING_H', 1)
endif

# Let's configuring love
char_size = cc.sizeof('char')
short_size = cc.sizeof('short')
int_size = cc.sizeof('int')
voidp_size = cc.sizeof('void*')
long_size = cc.sizeof('long')
long_long_size = cc.sizeof('long long')
sizet_size = cc.sizeof('size_t')

char_align = cc.alignment('char')
short_align = cc.alignment('short')
int_align = cc.alignment('int')
voidp_align = cc.alignment('void*')
long_align = cc.alignment('long')
long_long_align = cc.alignment('long long')
sizet_align = cc.alignment('size_t')

glib_conf.set('ALIGNOF_UNSIGNED_LONG', long_align)

glib_conf.set('SIZEOF_CHAR', char_size)
glib_conf.set('SIZEOF_INT', int_size)
glib_conf.set('SIZEOF_SHORT', short_size)
glib_conf.set('SIZEOF_LONG', long_size)
glib_conf.set('SIZEOF_LONG_LONG', long_long_size)
glib_conf.set('SIZEOF_SIZE_T', sizet_size)
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

# Horrible hack.
glibconfig_conf.set('gintbits', 32)

if host.name() == 'windows'
  glibconfig_conf.set('g_pid_type', 'void*')
else
  glibconfig_conf.set('g_pid_type', 'int')
endif

if sizet_size == short_size
  glibconfig_conf.set('glib_size_type_define', 'short')
  glibconfig_conf.set('gsize_modifier', '"h"')
  glibconfig_conf.set('gsize_format', '"hu"')
  glibconfig_conf.set('gssize_format', '"hi"')
  glibconfig_conf.set('glib_msize_type', 'SHRT')
elif sizet_size == int_size
  glibconfig_conf.set('glib_size_type_define', 'int')
  glibconfig_conf.set('gsize_modifier', '""')
  glibconfig_conf.set('gsize_format', '"u"')
  glibconfig_conf.set('gssize_format', '"i"')
  glibconfig_conf.set('glib_msize_type', 'INT')
elif sizet_size == long_size
  glibconfig_conf.set('glib_size_type_define', 'long')
  glibconfig_conf.set('gsize_modifier', '"l"')
  glibconfig_conf.set('gsize_format', '"lu"')
  glibconfig_conf.set('gssize_format', '"li"')
  glibconfig_conf.set('glib_msize_type', 'LONG')
elif sizet_size == long_long_size
  glibconfig_conf.set('glib_size_type_define', 'long long')
  glibconfig_conf.set('gsize_modifier', '"I64"')
  glibconfig_conf.set('gsize_format', '"I64u"')
  glibconfig_conf.set('gssize_format', '"I64i"')
  glibconfig_conf.set('glib_msize_type', 'INT64')
else
  error('Could not determine size of size_t.')
endif

glibconfig_conf.set('glib_void_p', voidp_size)
glibconfig_conf.set('glib_long', long_size)
glibconfig_conf.set('glib_size_t', sizet_size)
if host.is_big_endian()
  glibconfig_conf.set('g_byte_order', 'G_BIG_ENDIAN')
  glibconfig_conf.set('g_bs_native', 'BE')
  glibconfig_conf.set('g_bs_alien', 'LE')
else
  glibconfig_conf.set('g_byte_order', 'G_LITTLE_ENDIAN')
  glibconfig_conf.set('g_bs_native', 'LE')
  glibconfig_conf.set('g_bs_alien', 'BE')
endif

if cc.has_header('limits.h')
  glibconfig_conf.set('INC_LIMITS_H', '#include<limits.h>')
  glibconfig_conf.set('glib_ms', 'SHRT_MIN')
  glibconfig_conf.set('glib_Ms', 'SHRT_MAX')
  glibconfig_conf.set('glib_Mus', 'USHRT_MAX')
  glibconfig_conf.set('glib_mi', 'INT_MIN')
  glibconfig_conf.set('glib_Mi', 'INT_MAX')
  glibconfig_conf.set('glib_Mui', 'UINT_MAX')
  glibconfig_conf.set('glib_ml', 'LONG_MIN')
  glibconfig_conf.set('glib_Ml', 'LONG_MAX')
  glibconfig_conf.set('glib_Mul', 'ULONG_MAX')
endif
if cc.has_header('float.h')
  glibconfig_conf.set('INC_FLOAT_H', '#include<float.h>')
  glibconfig_conf.set('glib_mf', 'FLT_MIN')
  glibconfig_conf.set('glib_Mf', 'FLT_MAX')
  glibconfig_conf.set('glib_md', 'DBL_MIN')
  glibconfig_conf.set('glib_Md', 'DBL_MAX')
endif
if cc.has_header('values.h')
  glibconfig_conf.set('INC_VALUES_H', '#include<values.h>')
  glibconfig_conf.set('glib_mf', 'MINFLOAT')
  glibconfig_conf.set('glib_Mf', 'MAXFLOAT')
  glibconfig_conf.set('glib_md', 'MINDOUBLE')
  glibconfig_conf.set('glib_Md', 'MAXDOUBLE')
  glibconfig_conf.set('glib_ms', 'MINSHORT')
  glibconfig_conf.set('glib_Ms', 'MAXSHORT')
  glibconfig_conf.set('glib_Mus', '(((gushort)G_MAXSHORT)*2+1)')
  glibconfig_conf.set('glib_mi', 'MININT')
  glibconfig_conf.set('glib_Mi', 'MAXINT')
  glibconfig_conf.set('glib_Mui', '(((guint)G_MAXINT)*2+1)')
  glibconfig_conf.set('glib_ml', 'MINLONG')
  glibconfig_conf.set('glib_Ml', 'MAXLONG')
  glibconfig_conf.set('glib_Mul', '(((gulong)G_MAXLONG)*2+1)')
endif

if cc.compiles('''#undef inline
	inline int foo () { return 0; }
	int main () { return foo (); }''')
  glibconfig_conf.set('G_HAS_INLINE', 1)
else
  glibconfig_conf.set('G_HAS_INLINE', 0)
endif

# I can't quite decipher what the multiple consecutive checks
# are trying to do. Use this, fix if it breaks.
glibconfig_conf.set('G_CAN_INLINE', '1')

vacopy_src = '''#include <stdarg.h>
#include <stdlib.h>
void f (int i, ...) {
va_list args1, args2;
va_start (args1, i);
va_copy (args2, args1);
if (va_arg (args2, int) != 42 || va_arg (args1, int) != 42)
  exit (1);
va_end (args1); va_end (args2);
}
int main() {
  f (0, 42);
  return 0;
}
'''
have_vacopy = cc.compiles(vacopy_src)

__vaacopy_src = '''#include <stdarg.h>
#include <stdlib.h>
void f (int i, ...) {
va_list args1, args2;
va_start (args1, i);
__va_copy (args2, args1);
if (va_arg (args2, int) != 42 || va_arg (args1, int) != 42)
  exit (1);
va_end (args1); va_end (args2);
}
int main() {
  f (0, 42);
  return 0;
}
'''
have___vaacopy = cc.compiles(__vaacopy_src)

if have_vacopy
  glib_conf.set('G_VA_COPY', 'va_copy')
elif have___vacopy
  glib_conf.set('G_VA_COPY', '__va_copy')
endif

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

glibconfig_conf.set('GLIB_USING_SYSTEM_PRINTF', true)

# Need to check $host for this one.
glibconfig_conf.set('glib_memory_barrier_needed', true)
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

pcre = dependency('libpcre') # Should check for Unicode support, too.
thread_lib = find_library('pthread')

configure_file(input : 'config.h.meson', output : 'config.h', configuration : glib_conf)

subdir('glib')
#subdir('tests')
