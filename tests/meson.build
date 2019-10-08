# tests

# Not entirely random of course, but at least it changes over time
random_number = minor_version + meson.version().split('.').get(1).to_int()

test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('G_DEBUG', 'gc-friendly')
test_env.set('MALLOC_CHECK_', '2')
test_env.set('MALLOC_PERTURB_', '@0@'.format(random_number % 256))

test_cargs = ['-DG_LOG_DOMAIN="GLib"', '-UG_DISABLE_ASSERT']

subdir('gobject')
subdir('refcount')

tests = {
  'testglib' : {'tap' : true},
  'testgdate' : {},
  'datetime' : {},
  'bit-test' : {},
  'child-test' : {},
  'completion-test' : {},
  'dirname-test' : {},
  'file-test' : {},
  'env-test' : {},
  'gio-test' : {},
  'mainloop-test' : {},
  'mapping-test' : {},
  'onceinit' : {},
  'asyncqueue-test' : {},
  'qsort-test' : {},
  'relation-test' : {},
  'slice-concurrent' : {},
  'slice-threadinit' : {
    'dependencies' : [libgthread_dep],
  },
  'sources' : {},
  'spawn-test' : {},
  'thread-test' : {},
  'threadpool-test' : {'suite' : ['slow']},
  'type-test' : {},
  'unicode-caseconv' : {},
  'unicode-encoding' : {},
  'module-test-library' : {
    'dependencies' : [libgmodule_dep],
    'export_dynamic' : true,
    'source': 'module-test.c',
    'c_args': ['-DMODULE_TYPE="library"'],
  },
  'module-test-plugin' : {
    'dependencies' : [libgmodule_dep],
    'export_dynamic' : true,
    'source': 'module-test.c',
    'c_args': ['-DMODULE_TYPE="plugin"'],
  },
  'cxx-test' : {
    'source' : 'cxx-test.cpp',
    'include_directories' : gmoduleinc,
    'dependencies' : [libgio_dep],
  },
}

test_extra_programs = {
  'slice-test' : {
    'extra_sources' : ['memchunks.c'],
  },
  'slice-color' : {
    'extra_sources' : ['memchunks.c'],
  },
  'assert-msg-test' : {},
  'unicode-collate' : {},
}

if host_machine.system() != 'windows'
  tests += {
    'timeloop' : {},
    'iochannel-test' : {},
  }
else
  test_extra_programs += {
    'spawn-test-win32-gui' : {'gui_app' : true}
  }
endif

if installed_tests_enabled
  install_data(
    'iochannel-test-infile',
    'casemap.txt',
    'casefold.txt',
    'utf8.txt',
    install_dir : installed_tests_execdir,
  )
endif

module_suffix = []
# Keep the autotools convention for shared module suffix because GModule
# depends on it: https://gitlab.gnome.org/GNOME/glib/issues/520
if ['darwin', 'ios'].contains(host_machine.system())
  module_suffix = 'so'
endif

foreach module : ['moduletestplugin_a', 'moduletestplugin_b']
  shared_module(module + '_plugin', 'lib@0@.c'.format(module),
    dependencies : [libglib_dep, libgmodule_dep],
    install_dir : installed_tests_execdir,
    install : installed_tests_enabled,
    name_suffix : module_suffix
  )
  shared_library(module + '_library', 'lib@0@.c'.format(module),
    dependencies : [libglib_dep, libgmodule_dep],
    install_dir : installed_tests_execdir,
    install : installed_tests_enabled,
    name_suffix : module_suffix
  )
endforeach

common_c_args = test_cargs + ['-DGLIB_DISABLE_DEPRECATION_WARNINGS']
common_deps = [libm, thread_dep, libglib_dep]

foreach test_name, extra_args : tests
  source = extra_args.get('source', test_name + '.c')
  extra_sources = extra_args.get('extra_sources', [])
  install = installed_tests_enabled and extra_args.get('install', true)
  template = extra_args.get('tap', false) ? installed_tests_template_tap : installed_tests_template

  if install
    test_conf = configuration_data()
    test_conf.set('installed_tests_dir', installed_tests_execdir)
    test_conf.set('program', test_name)
    test_conf.set('env', '')
    configure_file(
      input: template,
      output: test_name + '.test',
      install_dir: installed_tests_metadir,
      configuration: test_conf
    )
  endif

  # FIXME? $(GLIB_DEBUG_FLAGS)
  exe = executable(test_name, [source, extra_sources],
    c_args : common_c_args + extra_args.get('c_args', []),
    dependencies : common_deps + extra_args.get('dependencies', []),
    export_dynamic : extra_args.get('export_dynamic', false),
    include_directories : extra_args.get('include_directories', []),
    install_dir: installed_tests_execdir,
    install: install,
  )

  suite = ['glib'] + extra_args.get('suite', [])
  timeout = suite.contains('slow') ? test_timeout_slow : test_timeout
  # FIXME? TESTS_ENVIRONMENT = LIBCHARSET_ALIAS_DIR=$(top_builddir)/glib/libcharset
  test(test_name, exe, env : test_env, timeout : timeout, suite : suite)
endforeach

foreach program_name, extra_args : test_extra_programs
  source = extra_args.get('source', program_name + '.c')
  extra_sources = extra_args.get('extra_sources', [])
  install = installed_tests_enabled and extra_args.get('install', true)
  executable(program_name, [source, extra_sources],
      c_args : common_c_args,
      dependencies : common_deps + extra_args.get('dependencies', []),
      install_dir : installed_tests_execdir,
      install : install,
      gui_app : extra_args.get('gui_app', false),
  )
endforeach
