gmodule_tests = {
  'max-version' : {'install': false},
}

if have_cxx
  gmodule_tests += {
    'cxx' : {
      'protocol': 'exitcode',
      'source' : ['cxx.cpp'],
      'suite' : ['cpp'],
    }
  }

  foreach std, arg: cxx_standards
    gmodule_tests += {
      'cxx-@0@'.format(std) : {
        'protocol': 'exitcode',
        'source' : ['cxx.cpp'],
        'suite' : ['cpp'],
        'cpp_args' : [arg],
      },
    }
  endforeach
endif

module_suffix = []
# Keep the autotools convention for shared module suffix because GModule
# depends on it: https://gitlab.gnome.org/GNOME/glib/issues/520
if ['darwin', 'ios'].contains(host_machine.system())
  module_suffix = 'so'
endif

gmodule_test_modules = []
gmodule_test_libraries = []
if glib_build_shared
  foreach module : ['moduletestplugin_a', 'moduletestplugin_b']
    gmodule_test_modules += shared_module(module + '_plugin',
      'lib@0@.c'.format(module),
      dependencies : [libglib_dep, libgmodule_dep],
      install_dir : installed_tests_execdir,
      install_tag : 'tests',
      install : installed_tests_enabled,
      name_suffix : module_suffix
    )
    gmodule_test_libraries += shared_library(module + '_library',
      'lib@0@.c'.format(module),
      dependencies : [libglib_dep, libgmodule_dep],
      install_dir : installed_tests_execdir,
      install_tag : 'tests',
      install : installed_tests_enabled,
      name_suffix : module_suffix
    )
  endforeach
endif

gmodule_tests += {
  'module-test-library' : {
    'export_dynamic' : true,
    'source': 'module-test.c',
    'depends': gmodule_test_libraries,
    'c_args': ['-DMODULE_TYPE="library"'],
    'can_fail': glib_build_static_only,
  },
  'module-test-plugin' : {
    'export_dynamic' : true,
    'source': 'module-test.c',
    'depends': gmodule_test_modules,
    'c_args': ['-DMODULE_TYPE="plugin"'],
    'can_fail': glib_build_static_only,
  },
}

test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())

test_deps = [libm, thread_dep, libglib_dep, libgmodule_dep]
test_cargs = ['-DG_LOG_DOMAIN="GModule"', '-UG_DISABLE_ASSERT']
test_cpp_args = test_cargs

foreach test_name, extra_args : gmodule_tests
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
      install_tag: 'tests',
      configuration: test_conf
    )
  endif

  exe = executable(test_name, source,
    c_args : test_cargs + extra_args.get('c_args', []),
    cpp_args : test_cpp_args + extra_args.get('cpp_args', []),
    link_args : extra_args.get('link_args', []),
    dependencies : test_deps + extra_args.get('dependencies', []),
    export_dynamic : extra_args.get('export_dynamic', false),
    install_dir: installed_tests_execdir,
    install_tag: 'tests',
    install: install,
  )

  suite = ['gmodule'] + extra_args.get('suite', [])
  timeout = suite.contains('slow') ? test_timeout_slow : test_timeout
  depends = [extra_args.get('depends', [])]

  if extra_args.get('can_fail', false)
    suite += 'failing'
  endif

  test(test_name,
    exe,
    protocol : extra_args.get('protocol', test_protocol),
    depends : depends,
    env : test_env,
    timeout : timeout,
    suite : suite,
  )
endforeach
