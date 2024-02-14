# SPDX-License-Identifier: LGPL-2.1-or-later
# SPDX-FileCopyrightText: 2024 GNOME Foundation

girepository_tests = {
  'cmph-bdz': {
    'dependencies': [cmph_dep],
  },
  'gthash' : {
    'dependencies': [girepo_gthash_dep],
  },
}

# Some tests require GIR files to have been generated
if enable_gir
  girepository_tests += {
    'function-info' : {
      'dependencies': [libffi_dep],
      'depends': [glib_gir],
    },
    'object-info' : {
      'depends': [gio_gir],
    },
    'registered-type-info' : {
      'depends': [gobject_gir],
    },
    'repository' : {
      'depends': [glib_gir, gio_gir, gobject_gir],
    },
    'repository-search-paths' : {
      'c_args': '-DGOBJECT_INTROSPECTION_LIBDIR="@0@"'.format(glib_libdir),
      'depends': [glib_gir],
    },
    'struct-info' : {
      'depends': [gobject_gir],
    },
    'throws' : {
      'depends': [glib_gir, gio_gir],
    },
    'union-info' : {
      'depends': [glib_gir],
    },
  }

  if cc.get_id() != 'msvc'
    girepository_tests += {
      'autoptr-girepository' : {
        'source' : 'autoptr.c',
        'depends': [glib_gir, gio_gir],
      },
    }
  endif
endif

test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())

test_deps = [libm, thread_dep, libgirepository_dep]
test_cargs = ['-DG_LOG_DOMAIN="GIRepository"', '-UG_DISABLE_ASSERT']
test_cpp_args = test_cargs

foreach test_name, extra_args : girepository_tests
  source = extra_args.get('source', test_name + '.c')
  install = installed_tests_enabled and extra_args.get('install', true)
  installed_tests_env = extra_args.get('installed_tests_env', {})

  if install
    test_conf = configuration_data()
    test_conf.set('installed_tests_dir', installed_tests_execdir)
    test_conf.set('program', test_name)
    test_env_override = ''
    if installed_tests_env != {}
      envs = []
      foreach var, value : installed_tests_env
         envs += '@0@=@1@'.format(var, value)
      endforeach
      test_env_override = '@0@ @1@ '.format(env_program.full_path(), ' '.join(envs))
    endif
    test_conf.set('env', test_env_override)
    configure_file(
      input: installed_tests_template_tap,
      output: test_name + '.test',
      install_dir: installed_tests_metadir,
      install_tag: 'tests',
      configuration: test_conf
    )
  endif

  exe = executable(test_name, source, 'test-common.c',
    c_args: test_cargs + extra_args.get('c_args', []),
    cpp_args: test_cpp_args + extra_args.get('cpp_args', []),
    link_args: extra_args.get('link_args', []),
    override_options: extra_args.get('override_options', []),
    dependencies: test_deps + extra_args.get('dependencies', []),
    link_with: extra_args.get('link_with', []),
    install_dir: installed_tests_execdir,
    install_tag: 'tests',
    install: install,
  )

  depends = [extra_args.get('depends', [])]
  suite = ['girepository', 'core'] + extra_args.get('suite', [])
  timeout = suite.contains('slow') ? test_timeout_slow : test_timeout

  if extra_args.get('can_fail', false)
    suite += 'failing'
  endif

  foreach program : extra_args.get('extra_programs', [])
    depends += test_extra_programs_targets[program]
  endforeach

  local_test_env = test_env
  foreach var, value : extra_args.get('env', {})
    local_test_env.append(var, value)
  endforeach

  test(test_name, exe,
    args: extra_args.get('args', []),
    protocol: extra_args.get('protocol', test_protocol),
    depends: depends,
    env: local_test_env,
    timeout: timeout,
    suite: suite,
    should_fail: extra_args.get('should_fail', false),
  )
endforeach
