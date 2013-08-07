configure_file(input : 'glibconfig.h.in', output : 'glibconfig.h', configuration : glibconfig_conf)

subdir('libcharset')

if host.name() == 'windows'
  thread_src = ['gthread-win32.c']
  plat_src = []
else
  thread_src = ['gthread-posix.c']
  plat_src = ['glib-unix.c']
endif

deprecated_sources = [
'deprecated/gallocator.c',
'deprecated/gcache.c',
'deprecated/gcompletion.c',
'deprecated/grel.c',
'deprecated/gthread-deprecated.c']

glib_sources = [
'garray.c',
'gasyncqueue.c',
'gatomic.c',
'gbacktrace.c',
'gbase64.c',
'gbitlock.c',
'gbookmarkfile.c',
'gbytes.c',
'gcharset.c',
'gchecksum.c',
'gconvert.c',
'gdataset.c',
'gdate.c',
'gdatetime.c',
'gdir.c',
'genviron.c',
'gerror.c',
'gfileutils.c',
'ggettext.c',
'ghash.c',
'ghmac.c',
'ghook.c',
'ghostutils.c',
'giochannel.c',
'gkeyfile.c',
'glib-init.c',
'glib-private.c',
'glist.c',
'gmain.c',
'gmappedfile.c',
'gmarkup.c',
'gmem.c',
'gmessages.c',
'gnode.c',
'goption.c',
'gpattern.c',
'gpoll.c',
'gprimes.c',
'gqsort.c',
'gquark.c',
'gqueue.c',
'grand.c',
'gregex.c',
'gscanner.c',
'gsequence.c',
'gshell.c',
'gslice.c',
'gslist.c',
'gstdio.c',
'gstrfuncs.c',
'gstring.c',
'gstringchunk.c',
'gtestutils.c',
'gthread.c',
'gthreadpool.c',
'gtimer.c',
'gtimezone.c',
'gtrashstack.c',
'gtree.c',
'guniprop.c',
'gutf8.c',
'gunibreak.c',
'gunicollate.c',
'gunidecomp.c',
'gurifuncs.c',
'gutils.c',
'gvariant.c',
'gvariant-core.c',
'gvariant-parser.c',
'gvariant-serialiser.c',
'gvarianttypeinfo.c',
'gvarianttype.c',
'gversion.c',
'gwakeup.c',
'gprintf.c',]

extra_src = ['gspawn.c', 'giounix.c']

libglib = shared_library('glib',
sources : [deprecated_sources, glib_sources, thread_src, plat_src, extra_src],
version : glib_version,
soversion : interface_version,
include_dirs : inc_dirs,
link_with : charset_lib,
deps : [pcre, thread_lib],
c_args : ['-DG_LOG_DOMAIN="GLib"', '-DGLIB_COMPILATION', '-DPCRE_STATIC'])
