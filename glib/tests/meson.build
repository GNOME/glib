glib_tests = {
  'array-test' : {},
  'asyncqueue' : {},
  'atomic' : {
    'c_args' : cc.get_id() == 'gcc' ? ['-Wstrict-aliasing=2'] : [],
    'c_standards': c_standards.keys(),
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
    'can_fail' : host_system == 'darwin',
  },
  'dir' : {},
  'environment' : {
    # FIXME: https://gitlab.gnome.org/GNOME/glib/-/issues/1392
    'can_fail' : host_system == 'darwin',
  },
  'error' : {},
  'fileutils' : {},
  'gdatetime' : {
    'suite' : ['slow'],
    'can_fail' : host_system == 'windows',
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
  'macros' : {
    'c_standards': c_standards.keys(),
  },
  'mainloop' : {},
  'mappedfile' : {},
  'mapping' : {},
  'markup' : {},
  'markup-parse' : {},
  'markup-collect' : {},
  'markup-escape' : {},
  'markup-subparser' : {},
  'max-version' : {'install' : false},
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
  'pathbuf' : {},
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
  'slist' : {},
  'sort' : {},
  'spawn-multithreaded' : {
    'can_fail': glib_build_static and host_system == 'windows',
    'suite': host_system == 'windows' ? ['flaky'] : [],
    'extra_programs' : ['test-spawn-echo'] + (
      host_machine.system() == 'windows' ? ['test-spawn-sleep'] : []),
  },
  'spawn-path-search' : {
    'extra_programs' : [
      'spawn-path-search-helper',
      'spawn-test-helper',
      'spawn-test-helper-subdir',
    ],
  },
  'spawn-singlethread' : {
    'dependencies' : [winsock2],
    'extra_programs' : ['test-spawn-echo'],
  },
  'spawn-test' : {
    'extra_programs' : host_machine.system() == 'windows' ? ['spawn-test-win32-gui'] : [],
  },
  'strfuncs' : {},
  'string' : {},
  'string-macro' : {
    'source' : 'string.c',
    'c_args' : cc.get_id() == 'gcc' ? ['-Werror=sign-conversion'] : [],
    'install' : false,
  },
  'strvbuilder' : {},
  'testing' : {
    'args': [ '--verbose' ],
    'extra_programs' : ['testing-helper'],
  },
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
  'utils' : {
    'c_standards': c_standards.keys(),
  },
  'utils-isolated' : {},
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
        'cpp_args' : [arg, '-D_G_EXPECTED_CXX_STANDARD="@0@"'.format(std)],
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
  if have_rtld_next and glib_build_shared
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

test_extra_programs = {
  'assert-msg-test' : {},
  'markup-example' : {'install' : false},
  'spawn-path-search-helper' : {},
  'spawn-test-helper' : {},
  'testing-helper' : {},
  # test-spawn-echo helper binary required by the spawn tests above
  'test-spawn-echo' : {},
}

if have_dlopen_dlsym and have_rtld_next
  test_extra_programs += {
    'messages-low-memory' : {
      'dependencies' : libdl_dep,
      'override_options' : ['b_asneeded=false'],
    },
  }
endif

if host_machine.system() == 'windows'
  # test-spawn-sleep helper binary required by the spawn tests above
  test_extra_programs += {
    'test-spawn-sleep' : {},
    'spawn-test-win32-gui' : { 'win_subsystem': 'windows' },
  }
endif

foreach test_name, extra_args : glib_tests
  foreach std: extra_args.get('c_standards', [])
    if c_standards.has_key(std)
      glib_tests += {
        '@0@-c-@1@'.format(test_name, std) : extra_args + {
          'source' : extra_args.get('source', test_name + '.c'),
          'suite' : ['cc'] + extra_args.get('suite', []),
          'c_args' : [
            c_standards.get(std),
            '-D_G_EXPECTED_C_STANDARD="@0@"'.format(std)
          ] + extra_args.get('c_args', []),
        }
      }
    endif
  endforeach
endforeach

test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())

test_deps = [libm, thread_dep, libglib_dep]
test_cargs = ['-DG_LOG_DOMAIN="GLib"', '-UG_DISABLE_ASSERT']
test_cpp_args = test_cargs

test_extra_programs_targets = {}
foreach program_name, extra_args : test_extra_programs
  source = extra_args.get('source', program_name + '.c')
  extra_sources = extra_args.get('extra_sources', [])
  install = installed_tests_enabled and extra_args.get('install', true)
  test_extra_programs_targets += {
    program_name : executable(program_name,
      sources: [source, extra_sources],
      c_args : test_cargs,
      cpp_args: test_cpp_args,
      override_options : extra_args.get('override_options', []),
      dependencies : test_deps + extra_args.get('dependencies', []),
      install_dir : installed_tests_execdir,
      install_tag : 'tests',
      install : install,
      win_subsystem : extra_args.get('win_subsystem', 'console'),
    )
  }
endforeach

subdir('path-test-subdir')

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
    override_options : extra_args.get('override_options', []),
    dependencies : test_deps + extra_args.get('dependencies', []),
    install_dir: installed_tests_execdir,
    install_tag: 'tests',
    install: install,
  )

  depends = [extra_args.get('depends', [])]
  suite = ['glib', 'core'] + extra_args.get('suite', [])
  timeout = suite.contains('slow') ? test_timeout_slow : test_timeout

  if extra_args.get('can_fail', false)
    suite += 'failing'
  endif

  foreach program : extra_args.get('extra_programs', [])
    depends += test_extra_programs_targets[program]
  endforeach

  test(test_name, exe,
    args: extra_args.get('args', []),
    protocol : extra_args.get('protocol', test_protocol),
    depends : depends,
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

python_tests = {
  'assert-msg-test.py' : {
    'can_fail' : host_system == 'windows',
    'extra_programs': ['assert-msg-test'],
  },
}

if 'messages-low-memory' in test_extra_programs
  python_tests += {
    'messages-low-memory.py' : {
      'extra_programs': ['messages-low-memory'],
    },
  }
endif

foreach test_name, extra_args : python_tests
  depends = [extra_args.get('depends', [])]
  suite = ['glib', 'core', 'no-valgrind']

  if extra_args.get('can_fail', false)
    suite += 'failing'
  endif

  foreach program : extra_args.get('extra_programs', [])
    depends += test_extra_programs_targets[program]
  endforeach

  test(
    test_name,
    python,
    protocol : extra_args.get('protocol', test_protocol),
    depends: depends,
    args: ['-B', files(test_name)],
    env: test_env,
    suite: suite,
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

# some testing of gtester functionality
# FIXME: Work around Meson not automatically using exe_wrapper around gtester
# https://github.com/mesonbuild/meson/issues/11029
if not meson.is_cross_build() and host_system != 'windows'
  xmllint = find_program('xmllint', required: false)
  if xmllint.found()
    tmpsample_xml = custom_target('tmpsample.xml',
      output : 'tmpsample.xml',
      command : [ gtester, '-k', '--quiet', '--i-know-this-is-deprecated', '-o', '@OUTPUT@',
                  '--test-arg=--gtester-selftest', gtester])

    test('gtester-xmllint-check', xmllint,
      args : ['--noout', tmpsample_xml],
      env : test_env,
      suite : ['glib', 'core'],
    )
  endif
endif

