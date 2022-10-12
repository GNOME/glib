glib_tests = {
  'array-test' : {},
  'asyncqueue' : {},
  'atomic' : {
    'c_args' : cc.get_id() == 'gcc' ? ['-Wstrict-aliasing=2'] : [],
  },
  'base64' : {},
  'bitlock' : {},
  'bookmarkfile' : {},
  'bytes' : {},
  'cache' : {},
  'charset' : {},
  'checksum' : {},
  'collate' : {},
  'completion' : {},
  'cond' : {},
  'convert' : {},
  'dataset' : {},
  'date' : {
    # FIXME: https://gitlab.gnome.org/GNOME/glib/-/issues/1392
    'should_fail' : host_system == 'darwin',
  },
  'dir' : {},
  'environment' : {
    # FIXME: https://gitlab.gnome.org/GNOME/glib/-/issues/1392
    'should_fail' : host_system == 'darwin',
  },
  'error' : {},
  'fileutils' : {},
  'gdatetime' : {
    'suite' : ['slow'],
  },
  'guuid' : {},
  'gvariant' : {
    'suite' : ['slow'],
  },
  'gwakeup' : {
    'source' : ['gwakeuptest.c', '../gwakeup.c'],
    'install' : false,
  },
  'hash' : {},
  'hmac' : {},
  'hook' : {},
  'hostutils' : {},
  'io-channel-basic' : {},
  'io-channel' : {},
  'keyfile' : {},
  'list' : {},
  'logging' : {},
  'macros' : {},
  'mainloop' : {},
  'mappedfile' : {},
  'mapping' : {},
  'markup' : {},
  'markup-parse' : {},
  'markup-collect' : {},
  'markup-escape' : {},
  'markup-subparser' : {},
  'memchunk' : {},
  'mem-overflow' : {
    'link_args' : cc.get_id() == 'gcc' and cc.version().version_compare('> 6')
      ? ['-Wno-alloc-size-larger-than'] : [],
  },
  'mutex' : {},
  'node' : {},
  'once' : {},
  'onceinit' : {},
  'option-context' : {},
  'option-argv0' : {},
  'overflow' : {},
  'overflow-fallback' : {
    'source' : 'overflow.c',
    'c_args' : ['-D_GLIB_TEST_OVERFLOW_FALLBACK'],
  },
  'pattern' : {},
  'private' : {},
  'protocol' : {},
  'queue' : {},
  'rand' : {},
  'rcbox' : {},
  'rec-mutex' : {},
  'refcount' : {},
  'refcount-macro' : {
    'source' : 'refcount.c',
    'c_args' : ['-DG_DISABLE_CHECKS'],
  },
  'refstring' : {},
  'regex' : {
    'dependencies' : [pcre2],
    'c_args' : use_pcre2_static_flag ? ['-DPCRE2_STATIC'] : [],
  },
  'relation' : {},
  'rwlock' : {},
  'scannerapi' : {},
  'search-utils' : {},
  'sequence' : {
    'suite' : ['slow'],
  },
  'shell' : {},
  'slice' : {},
  'slice-color' : {},
  'slice-concurrent' : {},
  'slice-known-pages' : {'suite' : ['no-valgrind']},
  'slice-glib' : {},
  'slice-slab' : {},
  'slice-malloc' : {},
  'slice-memchunk' : {
    'source' : ['slice-memchunk.c', 'memchunks.c'],
  },
  'slice-eager-freeing' : {},
  'slist' : {},
  'sort' : {},
  'spawn-multithreaded' : {},
  'spawn-path-search' : {},
  'spawn-singlethread' : {
    'dependencies' : [winsock2],
  },
  'spawn-test' : {},
  'strfuncs' : {},
  'string' : {},
  'strvbuilder' : {},
  'testing' : {},
  'test-printf' : {},
  'thread' : {},
  'thread-deprecated' : {},
  'thread-pool' : {},
  'thread-pool-slow' : {'suite' : ['slow']},
  'timeout' : {},
  'timer' : {},
  'tree' : {},
  'types' : {},
  'utf8-performance' : {},
  'utf8-pointer' : {},
  'utf8-validate' : {},
  'utf8-misc' : {},
  'utils' : {},
  'unicode' : {},
  'unicode-encoding' : {},
  'unicode-normalize': {},
  'uri' : {},
  '1bit-mutex' : {},
  '1bit-emufutex' : {
    'source' : '1bit-mutex.c',
    'c_args' : ['-DTEST_EMULATED_FUTEX'],
    'install' : false,
    'suite' : ['slow'],
  },
  '642026' : {
    'suite' : ['slow'],
  },
  '642026-ec' : {
    'source' : '642026.c',
    'c_args' : ['-DG_ERRORCHECK_MUTEXES'],
    'suite' : ['slow'],
  },
}

if have_cxx
  glib_tests += {
    'cxx' : {
      'source' : ['cxx.cpp'],
      'suite': ['cpp'],
    }
  }

  foreach std, arg: cxx_standards
    glib_tests += {
      'cxx-@0@'.format(std) : {
        'source' : ['cxx.cpp'],
        'suite' : ['cpp'],
        'cpp_args' : [arg],
      },
    }
  endforeach
endif

if cc.get_id() != 'msvc'
  glib_tests += {'autoptr' : {}}
endif

if glib_conf.has('HAVE_EVENTFD')
  glib_tests += {
    'gwakeup-fallback' : {
      'source' : ['gwakeuptest.c', '../gwakeup.c'],
      'c_args' : ['-DTEST_EVENTFD_FALLBACK'],
      'install' : false,
    },
  }
endif

if host_machine.system() == 'windows'
  if winsock2.found()
    glib_tests += {
      'gpoll' : {
        'dependencies' : [winsock2],
      },
    }
  endif
  glib_tests += {
    'win32' : {},
  }
else
  glib_tests += {
    'include' : {},
    'unix' : {},
  }
  if have_rtld_next and get_option('default_library') != 'static'
    glib_tests += {
      'gutils-user-database' : {
        'depends' : [
          shared_library('getpwuid-preload',
            'getpwuid-preload.c',
            name_prefix : '',
            dependencies: libdl_dep,
            install_dir : installed_tests_execdir,
            install_tag : 'tests',
            install: installed_tests_enabled,
          ),
        ],
        'env' : {
          'LD_PRELOAD': '@0@/getpwuid-preload.so'.format(meson.current_build_dir()),
        },
        'installed_tests_env' : {
          'LD_PRELOAD': '@0@/getpwuid-preload.so'.format(installed_tests_execdir),
        },
      },
    }
  endif
endif

if installed_tests_enabled
  install_data(
    '4096-random-bytes',
    'casefold.txt',
    'casemap.txt',
    'echo-script',
    'echo-script.bat',
    'empty',
    'iochannel-test-infile',
    'keyfile.c',
    'keyfiletest.ini',
    'NormalizationTest.txt',
    'pages.ini',
    install_dir : installed_tests_execdir,
    install_tag : 'tests',
  )
  install_subdir('bookmarks',
    install_dir : installed_tests_execdir,
    install_tag : 'tests',
  )
  install_subdir('markups',
    install_dir : installed_tests_execdir,
    install_tag : 'tests',
  )
  install_subdir('time-zones',
    install_dir : installed_tests_execdir,
    install_tag : 'tests',
  )
endif

test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('G_DEBUG', 'gc-friendly')
test_env.set('MALLOC_CHECK_', '2')

test_deps = [libm, thread_dep, libglib_dep]
test_cargs = ['-DG_LOG_DOMAIN="GLib"', '-UG_DISABLE_ASSERT']
test_cpp_args = test_cargs

foreach test_name, extra_args : glib_tests
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
    install_dir: installed_tests_execdir,
    install_tag: 'tests',
    install: install,
  )

  suite = ['glib'] + extra_args.get('suite', [])
  timeout = suite.contains('slow') ? test_timeout_slow : test_timeout
  test(test_name, exe,
    env : test_env,
    timeout : timeout,
    suite : suite,
    should_fail : extra_args.get('should_fail', false),
  )
endforeach

if installed_tests_enabled
  install_data(
    'utf8.txt',
    install_dir : installed_tests_execdir,
    install_tag : 'tests',
  )
endif

python_tests = [
  'assert-msg-test.py',
]

executable('assert-msg-test', ['assert-msg-test.c'],
  c_args : test_cargs,
  dependencies :  test_deps,
  install_dir : installed_tests_execdir,
  install_tag : 'tests',
  install : installed_tests_enabled,
  win_subsystem : extra_args.get('win_subsystem', 'console'),
)

foreach test_name : python_tests
  test(
    test_name,
    python,
    args: ['-B', files(test_name)],
    env: test_env,
    suite: ['glib', 'no-valgrind'],
  )

  if installed_tests_enabled
    install_data(
      files(test_name),
      install_dir: installed_tests_execdir,
      install_tag: 'tests',
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
      install_tag: 'tests',
      configuration: test_conf,
    )
  endif
endforeach

executable('spawn-path-search-helper', 'spawn-path-search-helper.c',
  c_args : test_cargs,
  dependencies : test_deps,
  install_dir: installed_tests_execdir,
  install_tag: 'tests',
  install: installed_tests_enabled,
)

executable('spawn-test-helper', 'spawn-test-helper.c',
  c_args : test_cargs,
  dependencies : test_deps,
  install_dir: installed_tests_execdir,
  install_tag: 'tests',
  install: installed_tests_enabled,
)

# test-spawn-echo helper binary required by the spawn tests above
executable('test-spawn-echo', 'test-spawn-echo.c',
  c_args : test_cargs,
  dependencies : test_deps,
  install_dir: installed_tests_execdir,
  install_tag: 'tests',
  install: installed_tests_enabled,
)

if host_machine.system() == 'windows'
  # test-spawn-sleep helper binary required by the spawn tests above
  executable('test-spawn-sleep', 'test-spawn-sleep.c',
    c_args : test_cargs,
    dependencies : test_deps,
    install_dir: installed_tests_execdir,
    install_tag: 'tests',
    install: installed_tests_enabled,
  )
  executable('spawn-test-win32-gui', 'spawn-test-win32-gui.c',
    c_args : test_cargs,
    dependencies : test_deps,
    install_dir: installed_tests_execdir,
    install_tag: 'tests',
    install: installed_tests_enabled,
    win_subsystem: 'windows',
  )
endif

executable('testing-helper', 'testing-helper.c',
  c_args : test_cargs,
  dependencies : test_deps,
  install_dir: installed_tests_execdir,
  install_tag: 'tests',
  install: installed_tests_enabled,
)

# some testing of gtester functionality
if meson.can_run_host_binaries() and host_system != 'windows'
  xmllint = find_program('xmllint', required: false)
  if xmllint.found()
    tmpsample_xml = custom_target('tmpsample.xml',
      output : 'tmpsample.xml',
      command : [ gtester, '-k', '--quiet', '--i-know-this-is-deprecated', '-o', '@OUTPUT@',
                  '--test-arg=--gtester-selftest', gtester])

    test('gtester-xmllint-check', xmllint,
      args : ['--noout', tmpsample_xml],
      env : test_env,
      suite : ['glib'],
    )
  endif
endif

subdir('path-test-subdir')
