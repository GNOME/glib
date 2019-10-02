configure_file(input : 'glibconfig.h.in', output : 'glibconfig.h',
  install_dir : join_paths(get_option('libdir'), 'glib-2.0/include'),
  configuration : glibconfig_conf)

subdir('libcharset')
if not use_system_pcre
  subdir('pcre')
endif

# TODO: gnulib_objects, pcre_objects and pcre_deps are a workaround for
# <https://github.com/mesonbuild/meson/issues/3934> and
# <https://github.com/mesonbuild/meson/issues/3937>. When we can depend
# on a meson version where those are fixed, revert the commit that
# introduced this workaround.
if use_system_printf
  gnulib_lib = []
  gnulib_objects = []
  gnulib_libm_dependency = []
  glib_conf.set ('gl_unused', '')
  glib_conf.set ('gl_extern_inline', '')
else
  subdir ('gnulib/gl_extern_inline')
  subdir ('gnulib/gl_cv_long_double_equals_double')
  subdir ('gnulib/gl_cv_cc_double_expbit0')
  subdir ('gnulib/gl_cv_func_printf_precision')
  subdir ('gnulib/gl_cv_func_printf_enomem')
  subdir ('gnulib/gl_cv_func_printf_flag_zero')
  subdir ('gnulib/gl_cv_func_printf_flag_leftadjust')
  subdir ('gnulib/gl_cv_func_printf_flag_grouping')
  subdir ('gnulib/gl_cv_func_printf_directive_a')
  subdir ('gnulib/gl_cv_func_printf_directive_f')
  subdir ('gnulib/gl_cv_func_printf_directive_ls')
  subdir ('gnulib/gl_cv_func_printf_long_double')
  subdir ('gnulib/gl_cv_func_printf_infinite')
  subdir ('gnulib/gl_cv_func_printf_infinite_long_double')

  gl_unused = '''
/* Define as a marker that can be attached to declarations that might not
    be used.  This helps to reduce warnings, such as from
    GCC -Wunused-parameter.  */
#if __GNUC__ >= 3 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)
# define _GL_UNUSED __attribute__ ((__unused__))
#else
# define _GL_UNUSED
#endif
'''
  glib_conf.set ('gl_unused', gl_unused)
  glib_conf.set ('gl_extern_inline', gl_extern_inline)

  if (gl_cv_long_double_equals_double)
    glib_conf.set ('HAVE_SAME_LONG_DOUBLE_AS_DOUBLE', 1)
  endif

  if (gl_cv_cc_double_expbit0_word >= 0 and
      gl_cv_cc_double_expbit0_bit >= 0)
    glib_conf.set('DBL_EXPBIT0_WORD', gl_cv_cc_double_expbit0_word)
    glib_conf.set('DBL_EXPBIT0_BIT', gl_cv_cc_double_expbit0_bit)
  endif

  if not gl_cv_func_printf_precision
    glib_conf.set('NEED_PRINTF_UNBOUNDED_PRECISION', 1)
  endif

  if not gl_cv_func_printf_enomem
    glib_conf.set('NEED_PRINTF_ENOMEM', 1)
  endif

  if not gl_cv_func_printf_flag_leftadjust
    glib_conf.set('NEED_PRINTF_FLAG_LEFTADJUST', 1)
  endif

  if not gl_cv_func_printf_flag_zero
    glib_conf.set('NEED_PRINTF_FLAG_ZERO', 1)
  endif

  if not gl_cv_func_printf_flag_grouping
    glib_conf.set('NEED_PRINTF_FLAG_GROUPING', 1)
  endif

  if not gl_cv_func_printf_directive_a
    glib_conf.set('NEED_PRINTF_DIRECTIVE_A', 1)
  endif

  if not gl_cv_func_printf_directive_f
    glib_conf.set('NEED_PRINTF_DIRECTIVE_F', 1)
  endif

  if not gl_cv_func_printf_directive_ls
    glib_conf.set('NEED_PRINTF_DIRECTIVE_LS', 1)
  endif

  if (not gl_cv_func_printf_precision or
      not gl_cv_func_printf_enomem)
    glib_conf.set('NEED_PRINTF_DOUBLE', 1)
    glib_conf.set('NEED_PRINTF_LONG_DOUBLE', 1)
  endif

  if not gl_cv_func_printf_infinite
    glib_conf.set('NEED_PRINTF_INFINITE_DOUBLE', 1)
  endif

  if gl_cv_func_printf_long_double and gl_cv_func_printf_infinite_long_double != 'false'
    glib_conf.set('NEED_PRINTF_INFINITE_LONG_DOUBLE', 1)
  endif

  subdir('gnulib')
  gnulib_objects = [gnulib_lib.extract_all_objects()]
endif

glib_headers = files(
  'glib.h',
  'glib-unix.h',
  'glib-object.h',
)
install_headers(glib_headers, subdir : 'glib-2.0')

glib_deprecated_headers = files(
  'deprecated/gallocator.h',
  'deprecated/gcache.h',
  'deprecated/gcompletion.h',
  'deprecated/gmain.h',
  'deprecated/grel.h',
  'deprecated/gthread.h',
)
install_headers(glib_deprecated_headers, subdir : 'glib-2.0/glib/deprecated')

glib_sub_headers = files(
  'glib-autocleanups.h',
  'galloca.h',
  'garray.h',
  'gasyncqueue.h',
  'gatomic.h',
  'gbacktrace.h',
  'gbase64.h',
  'gbitlock.h',
  'gbookmarkfile.h',
  'gbytes.h',
  'gcharset.h',
  'gchecksum.h',
  'gconvert.h',
  'gdataset.h',
  'gdate.h',
  'gdatetime.h',
  'gdir.h',
  'genviron.h',
  'gerror.h',
  'gfileutils.h',
  'ggettext.h',
  'ghash.h',
  'ghmac.h',
  'ghook.h',
  'ghostutils.h',
  'gi18n.h',
  'gi18n-lib.h',
  'giochannel.h',
  'gkeyfile.h',
  'glist.h',
  'gmacros.h',
  'gmain.h',
  'gmappedfile.h',
  'gmarkup.h',
  'gmem.h',
  'gmessages.h',
  'gnode.h',
  'goption.h',
  'gpattern.h',
  'gpoll.h',
  'gprimes.h',
  'gqsort.h',
  'gquark.h',
  'gqueue.h',
  'grand.h',
  'grcbox.h',
  'grefcount.h',
  'grefstring.h',
  'gregex.h',
  'gscanner.h',
  'gsequence.h',
  'gshell.h',
  'gslice.h',
  'gslist.h',
  'gspawn.h',
  'gstdio.h',
  'gstrfuncs.h',
  'gtestutils.h',
  'gstring.h',
  'gstringchunk.h',
  'gthread.h',
  'gthreadpool.h',
  'gtimer.h',
  'gtimezone.h',
  'gtrashstack.h',
  'gtree.h',
  'gtypes.h',
  'guuid.h',
  'gunicode.h',
  'gurifuncs.h',
  'gutils.h',
  'gvarianttype.h',
  'gvariant.h',
  'gversion.h',
  'gversionmacros.h',
  'gwin32.h',
  'gprintf.h',
)
install_headers(glib_sub_headers, subdir : 'glib-2.0/glib')

deprecated_sources = files(
  'deprecated/gallocator.c',
  'deprecated/gcache.c',
  'deprecated/gcompletion.c',
  'deprecated/grel.c',
  'deprecated/gthread-deprecated.c'
)

glib_sources = files(
  'garcbox.c',
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
  'grcbox.c',
  'grefcount.c',
  'grefstring.c',
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
  'gtranslit.c',
  'gtrashstack.c',
  'gtree.c',
  'guniprop.c',
  'gutf8.c',
  'gunibreak.c',
  'gunicollate.c',
  'gunidecomp.c',
  'gurifuncs.c',
  'gutils.c',
  'gutilsprivate.h',
  'guuid.c',
  'gvariant.c',
  'gvariant-core.c',
  'gvariant-parser.c',
  'gvariant-serialiser.c',
  'gvarianttypeinfo.c',
  'gvarianttype.c',
  'gversion.c',
  'gwakeup.c',
  'gprintf.c',
)

if host_system == 'windows'
  glib_win_rc = configure_file(
    input: 'glib.rc.in',
    output: 'glib.rc',
    configuration: glibconfig_conf,
  )
  glib_win_res = windows.compile_resources(glib_win_rc)
  glib_sources += [glib_win_res]
  glib_sources += files('gwin32.c', 'gspawn-win32.c', 'giowin32.c')
  platform_deps = [winsock2, cc.find_library('winmm')]
  if cc.get_id() == 'msvc' or cc.get_id() == 'clang-cl'
    glib_sources += files('dirent/wdirent.c')
  endif
else
  glib_sources += files('glib-unix.c', 'gspawn.c', 'giounix.c')
  platform_deps = []
endif

if host_system == 'darwin'
  glib_sources += files('gosxutils.m')
endif

glib_sources += files('gthread-@0@.c'.format(threads_implementation))

if enable_dtrace
  glib_dtrace_obj = dtrace_obj_gen.process('glib_probes.d')
  glib_dtrace_hdr = dtrace_hdr_gen.process('glib_probes.d')
else
  glib_dtrace_obj = []
  glib_dtrace_hdr = []
endif

pcre_static_args = []

if use_pcre_static_flag
  pcre_static_args = ['-DPCRE_STATIC']
endif

if use_system_pcre
  pcre_deps = [pcre]
  pcre_objects = []
else
  pcre_deps = []
  pcre_objects = [libpcre.extract_all_objects()]
endif

glib_c_args = ['-DG_LOG_DOMAIN="GLib"', '-DGLIB_COMPILATION'] + pcre_static_args + glib_hidden_visibility_args
libglib = library('glib-2.0',
  glib_dtrace_obj, glib_dtrace_hdr,
  sources : [deprecated_sources, glib_sources],
  objects : [charset_lib.extract_all_objects()] + gnulib_objects + pcre_objects,
  version : library_version,
  soversion : soversion,
  darwin_versions : darwin_versions,
  install : true,
  # intl.lib is not compatible with SAFESEH
  link_args : [noseh_link_args, glib_link_flags, win32_ldflags],
  include_directories : configinc,
  dependencies : pcre_deps + [thread_dep, libintl, librt] + libiconv + platform_deps + gnulib_libm_dependency,
  c_args : glib_c_args,
  objc_args : glib_c_args,
)

libglib_dep = declare_dependency(
  link_with : libglib,
  # thread_dep doesn't get pulled in from libglib atm,
  # see https://github.com/mesonbuild/meson/issues/1426
  dependencies : [thread_dep, libintl],
  # We sadly need to export configinc here because everyone includes <glib/*.h>
  include_directories : [configinc, glibinc])

pkg.generate(libglib,
  libraries : [libintl],
  libraries_private : [osx_ldflags, win32_ldflags],
  subdirs : ['glib-2.0'],
  extra_cflags : ['-I${libdir}/glib-2.0/include'] + win32_cflags,
  variables : ['bindir=' + join_paths('${prefix}', get_option('bindir')),
               'glib_genmarshal=' + join_paths('${bindir}', 'glib-genmarshal'),
               'gobject_query=' + join_paths('${bindir}', 'gobject-query'),
               'glib_mkenums=' + join_paths('${bindir}', 'glib-mkenums')],
  version : glib_version,
  install_dir : glib_pkgconfigreldir,
  filebase : 'glib-2.0',
  name : 'GLib',
  description : 'C Utility Library',
)

# On Windows, glib needs a spawn helper for g_spawn* API
if host_system == 'windows'
  if host_machine.cpu_family() == 'x86'
    executable('gspawn-win32-helper', 'gspawn-win32-helper.c',
      install : true,
      gui_app : true,
      include_directories : configinc,
      dependencies : [libglib_dep])
    executable('gspawn-win32-helper-console', 'gspawn-win32-helper.c',
      install : true,
      c_args : ['-DHELPER_CONSOLE'],
      include_directories : configinc,
      dependencies : [libglib_dep])
  else
    executable('gspawn-win64-helper', 'gspawn-win32-helper.c',
      install : true,
      gui_app : true,
      include_directories : configinc,
      dependencies : [libglib_dep])
    executable('gspawn-win64-helper-console', 'gspawn-win32-helper.c',
      install : true,
      c_args : ['-DHELPER_CONSOLE'],
      include_directories : configinc,
      dependencies : [libglib_dep])
  endif
else
  gtester = executable('gtester', 'gtester.c',
    install : true,
    c_args : ['-UG_DISABLE_ASSERT'],
    include_directories : configinc,
    dependencies : [libglib_dep])
endif

report_conf = configuration_data()
report_conf.set('GLIB_VERSION', glib_version)
report_conf.set('PYTHON', python_name)
configure_file(
  input: 'gtester-report.in',
  output: 'gtester-report',
  install_dir: get_option('bindir'),
  configuration: report_conf,
  install_mode: 'rwxr-xr-x'
)

install_data('glib_gdb.py', install_dir : join_paths(glib_pkgdatadir, 'gdb'))

gdb_conf = configuration_data()
gdb_conf.set('datadir', glib_datadir)
# This is also used in gobject/meson.build
if host_system != 'windows'
  # XXX: We add a leading './' because glib_libdir is an absolute path and we
  # need it to be a relative path so that join_paths appends it to the end.
  gdb_install_dir = join_paths(glib_datadir, 'gdb', 'auto-load', './' + glib_libdir)
else
  # FIXME: Cannot install on Windows because the path will contain a drive
  # letter and colons are not allowed in paths.
  gdb_install_dir = false
endif
configure_file(
  input: 'libglib-gdb.py.in',
  output: 'libglib-2.0.so.@0@-gdb.py'.format(library_version),
  configuration: gdb_conf,
  install_dir: gdb_install_dir,
)

if enable_systemtap
  glib_stp = configure_file(input : 'glib.stp.in',
    output : '@0@.stp'.format(libglib.full_path().split('/').get(-1)),
    configuration : stp_cdata,
    install_dir : tapset_install_dir,
  )
endif

if build_tests
  subdir('tests')
endif
