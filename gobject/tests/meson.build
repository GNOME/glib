marshalers_h = custom_target('marshalers_h',
  output : 'marshalers.h',
  input : 'marshalers.list',
  command : [
    python, glib_genmarshal,
    '--prefix=test',
    '--valist-marshallers',
    '--output=@OUTPUT@',
    '--quiet',
    '--header',
    '@INPUT@',
  ],
)
marshalers_c = custom_target('marshalers_c',
  output : 'marshalers.c',
  input : 'marshalers.list',
  command : [
    python, glib_genmarshal,
    '--prefix=test',
    '--valist-marshallers',
    '--include-header=marshalers.h',
    '--output=@OUTPUT@',
    '--quiet',
    '--body',
    '@INPUT@',
  ],
)

gobject_tests = {
  'qdata' : {},
  'boxed' : {},
  'enums' : {},
  'param' : {},
  'threadtests' : {},
  'dynamictests' : {},
  'binding' : {},
  'properties' : {},
  'reference' : {},
  'flags' : {},
  'value' : {},
  'type' : {},
  'gobject-private' : {
    'source' : 'private.c',
  },
  'closure' : {},
  'closure-refcount' : { 'suite': ['slow'] },
  'object' : {},
  'signal-handler' : {},
  'ifaceproperties' : {},
  'signals' : {
    'source' : ['signals.c', marshalers_h, marshalers_c],
  },
  'testing' : {},
}

if cc.get_id() != 'msvc'
  gobject_tests += {'autoptr' : {}}
endif

python_tests = [
  'genmarshal.py',
  'mkenums.py',
]

# FIXME: put common bits of test environment() in one location
# Not entirely random of course, but at least it changes over time
random_number = minor_version + meson.version().split('.').get(1).to_int()

test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('G_DEBUG', 'gc-friendly')
test_env.set('MALLOC_CHECK_', '2')
test_env.set('MALLOC_PERTURB_', '@0@'.format(random_number % 256))

test_deps = [libm, thread_dep, libglib_dep, libgobject_dep]
test_cargs = ['-DG_LOG_DOMAIN="GLib-GObject"', '-UG_DISABLE_ASSERT']

foreach test_name, extra_args : gobject_tests
  source = extra_args.get('source', test_name + '.c')
  install = installed_tests_enabled and extra_args.get('install', true)

  if install
    test_conf = configuration_data()
    test_conf.set('installed_tests_dir', installed_tests_execdir)
    test_conf.set('program', test_name)
    test_conf.set('env', '')
    configure_file(
      input: installed_tests_template_tap,
      output: test_name + '.test',
      install_dir: installed_tests_metadir,
      configuration: test_conf
    )
  endif

  exe = executable(test_name, source,
    c_args : test_cargs + extra_args.get('c_args', []),
    dependencies : test_deps + extra_args.get('dependencies', []),
    install_dir: installed_tests_execdir,
    install: install,
  )

  suite = ['gobject'] + extra_args.get('suite', [])
  timeout = suite.contains('slow') ? test_timeout_slow : test_timeout

  # FIXME: https://gitlab.gnome.org/GNOME/glib/issues/1316
  # aka https://bugs.debian.org/880883
  if test_name == 'closure-refcount' and ['arm', 'aarch64'].contains(host_cpu_family)
    timeout = timeout * 10
  endif

  test(test_name, exe, env : test_env, timeout : timeout, suite : suite)
endforeach

foreach test_name : python_tests
  test(
    test_name,
    python,
    args: ['-B', files(test_name)],
    env: test_env,
    suite: ['gobject', 'no-valgrind'],
  )

  if installed_tests_enabled
    install_data(
      files(test_name),
      install_dir: installed_tests_execdir,
      install_mode: 'rwxr-xr-x',
    )

    test_conf = configuration_data()
    test_conf.set('installed_tests_dir', installed_tests_execdir)
    test_conf.set('program', test_name)
    test_conf.set('env', '')
    configure_file(
      input: installed_tests_template_tap,
      output: test_name + '.test',
      install_dir: installed_tests_metadir,
      configuration: test_conf,
    )
  endif
endforeach

# TAP test runner for Python tests
if installed_tests_enabled
  install_data(
    files('taptestrunner.py'),
    install_dir: installed_tests_execdir,
  )
endif
