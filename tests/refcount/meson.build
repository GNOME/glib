refcount_tests = {
  'objects' : {},
  'objects2' : {'suite' : ['slow']},
  'properties' : {},
  'properties2' : {'suite' : ['slow']},
  'properties3' : {'suite' : ['slow']},
  'properties4' : {},
  'signal1' : {
    'source' : 'signals.c',
    'c_args' : ['-DTESTNUM=1'],
  },
  'signal2' : {
    'source' : 'signals.c',
    'c_args' : ['-DTESTNUM=2'],
  },
  'signal3' : {
    'source' : 'signals.c',
    'c_args' : ['-DTESTNUM=3'],
  },
  'signal4' : {
    'source' : 'signals.c',
    'c_args' : ['-DTESTNUM=4'],
  },
}

common_c_args = test_cargs + ['-DGLIB_DISABLE_DEPRECATION_WARNINGS']
common_deps = [libm, thread_dep, libglib_dep, libgobject_dep]

foreach test_name, extra_args : refcount_tests
  source = extra_args.get('source', test_name + '.c')
  extra_sources = extra_args.get('extra_sources', [])
  install = installed_tests_enabled and extra_args.get('install', true)

  if install
    test_conf = configuration_data()
    test_conf.set('installed_tests_dir', installed_tests_execdir)
    test_conf.set('program', test_name)
    test_conf.set('env', '')
    configure_file(
      input: installed_tests_template,
      output: test_name + '.test',
      install_dir: installed_tests_metadir,
      configuration: test_conf
    )
  endif

  # FIXME? $(GLIB_DEBUG_FLAGS)
  exe = executable(test_name, [source, extra_sources],
    c_args : common_c_args + extra_args.get('c_args', []),
    dependencies : common_deps + extra_args.get('dependencies', []),
    install_dir: installed_tests_execdir,
    install: install,
  )

  suite = ['refcount'] + extra_args.get('suite', [])
  timeout = suite.contains('slow') ? test_timeout_slow : test_timeout

  # FIXME? TESTS_ENVIRONMENT = LIBCHARSET_ALIAS_DIR=$(top_builddir)/glib/libcharset
  test(test_name, exe, env : test_env, timeout : timeout, suite : suite)
endforeach
