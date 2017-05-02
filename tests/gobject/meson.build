gobject_tests = [
  ['gvalue-test'],
  ['paramspec-test'],
  ['deftype'],
  ['defaultiface', ['defaultiface.c', 'testmodule.c']],
  ['dynamictype', ['dynamictype.c', 'testmodule.c']],
  ['override'],
  ['signals'],
  ['singleton'],
  ['references'],
]

# The marshal test requires running a binary, which means we cannot
# build it when cross-compiling
if not meson.is_cross_build() or meson.has_exe_wrapper()
  gnome = import('gnome')

  testmarshal_srcs = gnome.genmarshal('testmarshal',
    sources : 'testmarshal.list',
    prefix : 'test_marshal')

  gobject_tests += [
    ['accumulator', ['accumulator.c', testmarshal_srcs]],
  ]
endif

foreach t : gobject_tests
  test_name = t.get(0)
  test_src = t.get(1, test_name + '.c')
  test_extra_cargs = t.get(2, [])
  test_timeout = t.get(3, 30)

  # FIXME? $(GLIB_DEBUG_FLAGS)
  exe = executable(test_name, test_src,
    c_args : test_cargs + test_extra_cargs + ['-DGLIB_DISABLE_DEPRECATION_WARNINGS'],
    dependencies : [libm, thread_dep, libglib_dep, libgobject_dep],
    install : false,
  )
  # FIXME? TESTS_ENVIRONMENT = LIBCHARSET_ALIAS_DIR=$(top_builddir)/glib/libcharset
  test(test_name, exe, env : test_env, timeout : test_timeout)
endforeach

# Don't install these ones, and keep them out of 'make check' because they take too long...
executable('performance', 'performance.c',
    c_args : test_cargs + test_extra_cargs + ['-DGLIB_DISABLE_DEPRECATION_WARNINGS'],
    dependencies : [libm, thread_dep, libglib_dep, libgobject_dep],
    install : false)

executable('performance-threaded', 'performance-threaded.c',
    c_args : test_cargs + test_extra_cargs + ['-DGLIB_DISABLE_DEPRECATION_WARNINGS'],
    dependencies : [libm, thread_dep, libglib_dep, libgobject_dep],
    install : false)

if host_system != 'windows' and host_system != 'minix'
  executable('timeloop-closure', 'timeloop-closure.c',
      c_args : test_cargs + test_extra_cargs + ['-DGLIB_DISABLE_DEPRECATION_WARNINGS'],
      dependencies : [libm, thread_dep, libglib_dep, libgobject_dep],
      install : false)
endif
