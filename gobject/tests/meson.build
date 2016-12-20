gobject_tests = [
  'qdata',
  'boxed',
  'enums',
  'param',
  'threadtests',
  'dynamictests',
  'binding',
  'properties',
  'reference',
  'value',
  'type',
  'private',
  'closure',
  'object',
  'signal-handler',
  'ifaceproperties',
]

test_env = [
  'G_TEST_SRCDIR=' + meson.current_source_dir(),
  'G_TEST_BUILDDIR=' + meson.current_build_dir(),
]

foreach test_name : gobject_tests
  deps = [libm, thread_dep, libglib_dep, libgobject_dep]
  test_src = '@0@.c'.format(test_name)
  # private is an existing or reserved target it seems
  if test_name == 'private'
    test_name = 'gobject-private'
  endif
  exe = executable(test_name, test_src,
      c_args : ['-DHAVE_CONFIG_H=1', '-DG_LOG_DOMAIN="GLib-GObject"'],
      dependencies : deps,
  )
  test(test_name, exe, env : test_env)
endforeach

# The marshalers test requires running a binary, so we cannot build it when
# cross-compiling
if not meson.has_exe_wrapper()
  genmarshal = find_program('gobject_test_marshal.py')

  marshalers_h = custom_target('marshalers_h',
    output : 'marshalers.h',
    input : 'marshalers.list',
    command : [genmarshal, glib_genmarshal, '@INPUT@', '@OUTPUT@'],
  )
  marshalers_c = custom_target('marshalers_c',
    output : 'marshalers.c',
    input : 'marshalers.list',
    command : [genmarshal, glib_genmarshal, '@INPUT@', '@OUTPUT@'],
  )

  exe = executable('signals',
      'signals.c', marshalers_h, marshalers_c,
      c_args : ['-DHAVE_CONFIG_H=1', '-DG_LOG_DOMAIN="GLib-GObject"'],
      dependencies : deps,
  )
  test('signals', exe, env : test_env)
endif
