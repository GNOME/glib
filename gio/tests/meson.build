common_gio_tests_deps = [
  libglib_dep,
  libgmodule_dep,
  libgobject_dep,
  libgio_dep,
]

test_c_args = [
  '-DG_LOG_DOMAIN="GLib-GIO"',
  '-DGLIB_MKENUMS="@0@"'.format(glib_mkenums),
  '-DGLIB_COMPILE_SCHEMAS="@0@"'.format(glib_compile_schemas.full_path()),
  '-UG_DISABLE_ASSERT',
]

libutil_dep = dependency('', required : false)

if cc.has_header('pty.h')
  have_openpty = cc.has_function('openpty', prefix : '#include <pty.h>')

  if host_machine.system() == 'linux' and not have_openpty
    libutil_dep = cc.find_library('util', required : false)
    have_openpty = cc.has_function('openpty', dependencies : libutil_dep, prefix : '#include <pty.h>')
  endif

  if have_openpty
    test_c_args += '-DHAVE_OPENPTY'
  endif
endif

test_cpp_args = test_c_args

if host_machine.system() == 'windows'
  common_gio_tests_deps += [iphlpapi_dep, winsock2, cc.find_library ('secur32')]
endif

subdir('gdbus-object-manager-example')

gengiotypefuncs_prog = find_program('gengiotypefuncs.py')
giotypefuncs_inc = custom_target(
  'giotypefuncs.inc',
  output : 'giotypefuncs.inc',
  input : gio_headers + [gioenumtypes_h] + gobject_install_headers,
  command: [gengiotypefuncs_prog, '@OUTPUT@', '@INPUT@'])

#  Test programs buildable on all platforms
gio_tests = {
  'application-command-line': {},
  'appmonitor' : {
    # FIXME: https://gitlab.gnome.org/GNOME/glib/-/issues/1392
    'should_fail' : host_system == 'darwin',
  },
  'async-close-output-stream' : {},
  'async-splice-output-stream' : {},
  'buffered-input-stream' : {},
  'buffered-output-stream' : {},
  'cancellable' : {},
  'contexts' : {},
  'contenttype' : {
    # FIXME: https://gitlab.gnome.org/GNOME/glib/-/issues/1392 / https://gitlab.gnome.org/GNOME/glib/-/issues/1251
    'should_fail' : host_system == 'darwin',
  },
  'converter-stream' : {},
  'credentials' : {},
  'data-input-stream' : {},
  'data-output-stream' : {},
  'error': {},
  'file-thumbnail' : {},
  'fileattributematcher' : {},
  'filter-streams' : {},
  'giomodule' : {
    # FIXME: https://gitlab.gnome.org/GNOME/glib/-/issues/1392
    'should_fail' : host_system == 'darwin',
  },
  'gsubprocess' : {},
  'g-file' : {},
  'g-file-info' : {
    # FIXME: https://gitlab.gnome.org/GNOME/glib/-/issues/1392
    'should_fail' : host_system == 'darwin',
  },
  'g-icon' : {},
  'gdbus-addresses' : {},
  'gdbus-message' : {},
  'gdbus-peer' : {
    'dependencies' : [libgdbus_example_objectmanager_dep],
    'install_rpath' : installed_tests_execdir,
    # FIXME: https://gitlab.gnome.org/GNOME/glib/-/issues/1392
    'should_fail' : host_system == 'darwin',
  },
  'inet-address' : {},
  'io-stream' : {},
  'memory-input-stream' : {},
  'memory-monitor' : {},
  'memory-output-stream' : {},
  'memory-settings-backend' : {},
  'mount-operation' : {},
  'network-address' : {'extra_sources': ['mock-resolver.c']},
  'network-monitor' : {},
  'network-monitor-race' : {},
  'null-settings-backend' : {},
  'permission' : {},
  'pollable' : {'dependencies' : [libutil_dep]},
  'power-profile-monitor' : {},
  'proxy-test' : {},
  'readwrite' : {},
  'simple-async-result' : {},
  'simple-proxy' : {},
  'sleepy-stream' : {},
  'socket' : {
    # FIXME: https://gitlab.gnome.org/GNOME/glib/-/issues/1392
    'should_fail' : host_system == 'darwin',
  },
  'socket-listener' : {},
  'socket-service' : {},
  'srvtarget' : {},
  'task' : {},
  'vfs' : {},
  'volumemonitor' : {},
  'glistmodel' : {},
  'testfilemonitor' : {'suite' : ['slow', 'flaky']},
  'thumbnail-verification' : {},
  'tls-certificate' : {'extra_sources' : ['gtesttlsbackend.c']},
  'tls-interaction' : {'extra_sources' : ['gtesttlsbackend.c']},
  'tls-database' : {'extra_sources' : ['gtesttlsbackend.c']},
  'tls-bindings' : {'extra_sources' : ['gtesttlsbackend.c']},
  'unix-fd' : {},
  'gdbus-address-get-session' : {
    # FIXME: https://gitlab.gnome.org/GNOME/glib/-/issues/1392
    'should_fail' : host_system == 'darwin',
  },
  'win32-appinfo' : {},
}

if have_cxx
  gio_tests += {
    'cxx' : {
      'source' : ['cxx.cpp'],
      'suite': ['C++'],
    },
  }

  foreach std, arg: cxx_standards
    gio_tests += {
      'cxx-@0@'.format(std) : {
        'source' : ['cxx.cpp'],
        'suite' : ['cpp'],
        'cpp_args' : [arg],
      },
    }
  endforeach
endif

test_extra_programs = {
  'gdbus-connection-flush-helper' : {},
  'gdbus-testserver' : {},
  'gsubprocess-testprog' : {},
}

python_tests = [
  'codegen.py',
]

test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('GIO_MODULE_DIR', '')

if host_system != 'windows' and not glib_have_cocoa
  test_env.set('GIO_LAUNCH_DESKTOP', gio_launch_desktop.full_path())
endif

# Check for libdbus1 - Optional - is only used in the GDBus test cases
# 1.2.14 required for dbus_message_set_serial
dbus1_dep = dependency('dbus-1', required : false, version : '>= 1.2.14')
if not dbus1_dep.found()
  if cc.get_id() == 'msvc' or cc.get_id() == 'clang-cl'
    # MSVC: Search for the DBus library by the configuration, which corresponds
    # to the output of CMake builds of DBus.  Note that debugoptimized
    # is really a Release build with .PDB files.
    if vs_crt == 'debug'
      dbus1_dep = cc.find_library('dbus-1d', required : false)
    else
      dbus1_dep = cc.find_library('dbus-1', required : false)
    endif
  endif
endif
if dbus1_dep.found()
  glib_conf.set('HAVE_DBUS1', 1)

  gio_tests += {
    'gdbus-serialization' : {
      'extra_sources' : ['gdbus-tests.c'],
      'dependencies' : [dbus1_dep],
    },
    'gdbus-server-auth' : {
      'dependencies' : [dbus1_dep],
    },
  }
else
  # We can build a cut-down version of this test without libdbus
  gio_tests += {
    'gdbus-server-auth' : {},
  }
endif

have_dbus_daemon = find_program('dbus-daemon', required : false).found()
if have_dbus_daemon
  gio_tests += {
    'debugcontroller' : {},
    'defaultvalue' : {'extra_sources' : [giotypefuncs_inc]},
  }
endif

#  Test programs buildable on UNIX only
if host_machine.system() != 'windows'
  gio_tests += {
    'file' : {},
    'gdbus-peer-object-manager' : {},
    'gdbus-sasl' : {},
    'live-g-file' : {},
    'resolver-parsing' : {'dependencies' : [network_libs]},
    'socket-address' : {},
    'stream-rw_all' : {},
    'unix-mounts' : {},
    'unix-streams' : {},
    'g-file-info-filesystem-readonly' : {},
    'gschema-compile' : {'install' : false},
    'trash' : {},
  }

  # LD_PRELOAD modules don't work so well with AddressSanitizer
  if have_rtld_next and get_option('default_library') != 'static' and get_option('b_sanitize') == 'none'
    gio_tests += {
      'gsocketclient-slow' : {
        'depends' : [
          shared_library('slow-connect-preload',
            'slow-connect-preload.c',
            name_prefix : '',
            dependencies: libdl_dep,
            install_dir : installed_tests_execdir,
            install_tag : 'tests',
            install: installed_tests_enabled,
          )
        ],
        'env' : {
          'LD_PRELOAD': '@0@/slow-connect-preload.so'.format(meson.current_build_dir())
        },
        'installed_tests_env' : {
          'LD_PRELOAD': '@0@/slow-connect-preload.so'.format(installed_tests_execdir),
        },
      },
    }
  endif

  # Uninstalled because of the check-for-executable logic in DesktopAppInfo
  # unable to find the installed executable
  if not glib_have_cocoa
    gio_tests += {
      'appinfo' : {
        'install' : false,
      },
      'desktop-app-info' : {
        'install' : false,
      },
    }
  endif

  test_extra_programs += {
    'basic-application' : {},
    'dbus-launch' : {},
    'appinfo-test' : {},
  }

  if not glib_have_cocoa
    test_extra_programs += {
      'apps' : {},
    }
    gio_tests += {
      'mimeapps' : {},
    }
  endif

  if have_dbus_daemon
    annotate_args = [
      '--annotate', 'org.project.Bar', 'Key1', 'Value1',
      '--annotate', 'org.project.Bar', 'org.gtk.GDBus.Internal', 'Value2',
      '--annotate', 'org.project.Bar.HelloWorld()', 'Key3', 'Value3',
      '--annotate', 'org.project.Bar::TestSignal', 'Key4', 'Value4',
      '--annotate', 'org.project.Bar:ay', 'Key5', 'Value5',
      '--annotate', 'org.project.Bar.TestPrimitiveTypes()[val_int32]', 'Key6', 'Value6',
      '--annotate', 'org.project.Bar.TestPrimitiveTypes()[ret_uint32]', 'Key7', 'Value7',
      '--annotate', 'org.project.Bar::TestSignal[array_of_strings]', 'Key8', 'Value8',
    ]
    # Generate gdbus-test-codegen-generated.{c,h}
    gdbus_test_codegen_generated = custom_target('gdbus-test-codegen-generated',
        input :   ['test-codegen.xml'],
        output :  ['gdbus-test-codegen-generated.h',
                   'gdbus-test-codegen-generated.c'],
        depend_files : gdbus_codegen_built_files,
        command : [python, gdbus_codegen,
                   '--interface-prefix', 'org.project.',
                   '--output-directory', '@OUTDIR@',
                   '--generate-c-code', 'gdbus-test-codegen-generated',
                   '--c-generate-object-manager',
                   '--c-generate-autocleanup', 'all',
                   '--c-namespace', 'Foo_iGen',
                   '--generate-docbook', 'gdbus-test-codegen-generated-doc',
                   annotate_args,
                   '@INPUT@'])
    # Generate gdbus-test-codegen-generated-min-required-2-64.{c,h}
    gdbus_test_codegen_generated_min_required_2_64 = custom_target('gdbus-test-codegen-generated-min-required-2-64',
        input :   ['test-codegen.xml'],
        output :  ['gdbus-test-codegen-generated-min-required-2-64.h',
                   'gdbus-test-codegen-generated-min-required-2-64.c'],
        depend_files : gdbus_codegen_built_files,
        command : [python, gdbus_codegen,
                   '--glib-min-required', '2.64',
                   '--interface-prefix', 'org.project.',
                   '--output-directory', '@OUTDIR@',
                   '--generate-c-code', 'gdbus-test-codegen-generated-min-required-2-64',
                   '--c-generate-object-manager',
                   '--c-generate-autocleanup', 'all',
                   '--c-namespace', 'Foo_iGen',
                   '--generate-docbook', 'gdbus-test-codegen-generated-doc',
                   annotate_args,
                   '@INPUT@'])
    gdbus_test_codegen_generated_interface_info = [
      custom_target('gdbus-test-codegen-generated-interface-info-h',
          input :   ['test-codegen.xml'],
          output :  ['gdbus-test-codegen-generated-interface-info.h'],
          depend_files : gdbus_codegen_built_files,
          command : [python, gdbus_codegen,
                     '--interface-info-header',
                     annotate_args,
                     '--output', '@OUTPUT@',
                     '@INPUT@']),
      custom_target('gdbus-test-codegen-generated-interface-info-c',
          input :   ['test-codegen.xml'],
          output :  ['gdbus-test-codegen-generated-interface-info.c'],
          depend_files : gdbus_codegen_built_files,
          command : [python, gdbus_codegen,
                     '--interface-info-body',
                     annotate_args,
                     '--output', '@OUTPUT@',
                     '@INPUT@']),
    ]

    extra_sources = ['gdbus-sessionbus.c', 'gdbus-tests.c']

    gio_tests += {
      'actions' : {
        'extra_sources' : extra_sources,
        'suite' : ['slow'],
      },
      'fdo-notification-backend': {},
      'gdbus-auth' : {'extra_sources' : extra_sources},
      'gdbus-bz627724' : {'extra_sources' : extra_sources},
      'gdbus-close-pending' : {'extra_sources' : extra_sources},
      'gdbus-connection' : {'extra_sources' : extra_sources},
      'gdbus-connection-loss' : {'extra_sources' : extra_sources},
      'gdbus-connection-slow' : {'extra_sources' : extra_sources},
      'gdbus-error' : {'extra_sources' : extra_sources},
      'gdbus-exit-on-close' : {'extra_sources' : extra_sources},
      'gdbus-export' : {
        'extra_sources' : extra_sources,
        'suite' : ['slow'],
      },
      'gdbus-introspection' : {'extra_sources' : extra_sources},
      'gdbus-method-invocation' : {'extra_sources' : extra_sources},
      'gdbus-names' : {'extra_sources' : extra_sources},
      'gdbus-proxy' : {'extra_sources' : extra_sources},
      'gdbus-proxy-threads' : {
        'extra_sources' : extra_sources,
        'dependencies' : [dbus1_dep],
      },
      'gdbus-proxy-unique-name' : {'extra_sources' : extra_sources},
      'gdbus-proxy-well-known-name' : {'extra_sources' : extra_sources},
      'gdbus-test-codegen' : {
        'extra_sources' : [extra_sources, gdbus_test_codegen_generated, gdbus_test_codegen_generated_interface_info],
        'c_args' : ['-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_32'],
      },
      'gdbus-threading' : {
        'extra_sources' : extra_sources,
        'suite' : ['slow'],
      },
      'gmenumodel' : {
        'extra_sources' : extra_sources,
        'suite' : ['slow'],
      },
      'gnotification' : {
        'extra_sources' : [extra_sources, 'gnotification-server.c'],
      },
      'gdbus-test-codegen-old' : {
        'source' : 'gdbus-test-codegen.c',
        'extra_sources' : [extra_sources, gdbus_test_codegen_generated, gdbus_test_codegen_generated_interface_info],
        'c_args' : ['-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_36',
                    '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_36'],
      },
      'gdbus-test-codegen-min-required-2-64' : {
        'source' : 'gdbus-test-codegen.c',
        'extra_sources' : [extra_sources, gdbus_test_codegen_generated_min_required_2_64, gdbus_test_codegen_generated_interface_info],
        'c_args' : ['-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_64'],
      },
      'gapplication' : {'extra_sources' : extra_sources},
    }

    if not glib_have_cocoa
      gio_tests += {
        'dbus-appinfo' : {
          'extra_sources' : extra_sources,
        },
      }
    endif

    fake_document_portal_generated = custom_target('fake-document-portal-generated',
        input :   ['../org.freedesktop.portal.Documents.xml'],
        output :  ['fake-document-portal-generated.h',
                   'fake-document-portal-generated.c'],
        depend_files : gdbus_codegen_built_files,
        command : [python, gdbus_codegen,
                   '--interface-prefix', 'org.freedesktop.portal.',
                   '--output-directory', '@OUTDIR@',
                   '--generate-c-code', 'fake-document-portal-generated',
                   '--c-namespace', 'Fake',
                   '@INPUT@'])

    test_extra_programs += {
      'fake-document-portal' : {
        'extra_sources': fake_document_portal_generated,
      },
      'fake-service-name' : {}
    }
  endif # have_dbus_daemon

  # This test is currently unreliable
  executable('gdbus-overflow', 'gdbus-overflow.c',
      c_args : test_c_args,
      dependencies : common_gio_tests_deps,
      install_dir : installed_tests_execdir,
      install_tag : 'tests',
      install : installed_tests_enabled)

  gio_tests += {
    'gdbus-connection-flush' : {
      'extra_sources' : ['test-io-stream.c', 'test-pipe-unix.c'],
    },
    'gdbus-non-socket' : {
      'extra_sources' : ['gdbus-tests.c', 'test-io-stream.c', 'test-pipe-unix.c'],
    },
  }

  # Generate test.mo from de.po using msgfmt
  msgfmt = find_program('msgfmt', required : false)
  if msgfmt.found()
    subdir('de/LC_MESSAGES')
    gio_tests += {
      'gsettings' : {
        'extra_sources' : [test_mo],
        'c_args' : ['-DSRCDIR="@0@"'.format(meson.current_source_dir()),
                    '-DTEST_LOCALE_PATH="@0@"'.format(test_mo_dir)],
        'install' : false,
      },
    }
  endif
endif # unix

#  Test programs buildable on Windows only
if host_machine.system() == 'windows'
  gio_tests += {'win32-streams' : {}}
endif

if cc.get_id() != 'msvc'
  gio_tests += {
    'autoptr-gio' : {
      'source' : 'autoptr.c',
    },
  }
endif

test_extra_programs += {
  'gio-du' : {'install' : false},
  'echo-server' : {'install' : false},
  'filter-cat' : {'install' : false},
  'gapplication-example-actions' : {'install' : false},
  'gapplication-example-cmdline' : {'install' : false},
  'gapplication-example-cmdline2' : {'install' : false},
  'gapplication-example-cmdline3' : {'install' : false},
  'gapplication-example-cmdline4' : {'install' : false},
  'gapplication-example-dbushooks' : {'install' : false},
  'gapplication-example-open' : {'install' : false},
  'gdbus-daemon' : {
    'extra_sources' : gdbus_daemon_sources,
    'install' : false,
  },
  'gdbus-example-export' : {'install' : false},
  'gdbus-example-own-name' : {'install' : false},
  'gdbus-example-peer' : {'install' : false},
  'gdbus-example-proxy-subclass' : {'install' : false},
  'gdbus-example-server' : {'install' : false},
  'gdbus-example-subtree' : {'install' : false},
  'gdbus-example-watch-name' : {'install' : false},
  'gdbus-example-watch-proxy' : {'install' : false},
  'httpd' : {'install' : false},
  'proxy' : {'install' : false},
  'resolver' : {'install' : false},
  'send-data' : {'install' : false},
  'socket-server' : {'install' : false},
  'socket-client' : {
    'extra_sources' : ['gtlsconsoleinteraction.c'],
    'install' : false,
  },
}

gdbus_example_objectmanager_sources = files(
  'gdbus-example-objectmanager-client.c',
  'gdbus-example-objectmanager-server.c',
)

if cc.get_id() != 'msvc' and cc.get_id() != 'clang-cl'
  test_extra_programs += {
    # These three are manual-run tests because they need a session bus but don't bring one up themselves
    # FIXME: these build but don't seem to work!
    'gdbus-example-objectmanager-client' : {
      'dependencies' : [libgdbus_example_objectmanager_dep],
      'install' : false,
    },
    'gdbus-example-objectmanager-server' : {
      'dependencies' : [libgdbus_example_objectmanager_dep],
      'install' : false,
    },
    'gdbus-test-fixture' : {
      'dependencies' : [libgdbus_example_objectmanager_dep],
      'install' : false,
    },
  }
endif

if host_machine.system() != 'windows'
  test_extra_programs += {
    'gdbus-example-unix-fd-client' : {
      'install' : false,
    },
  }
endif

appinfo_test_desktop_files = [
  'appinfo-test-gnome.desktop',
  'appinfo-test-notgnome.desktop',
  'appinfo-test.desktop',
  'appinfo-test2.desktop',
]

cdata = configuration_data()
if installed_tests_enabled
  cdata.set('installed_tests_dir', installed_tests_execdir)
else
  cdata.set('installed_tests_dir', meson.current_build_dir())
endif

foreach appinfo_test_desktop_file : appinfo_test_desktop_files
  if installed_tests_enabled
    configure_file(
      input: appinfo_test_desktop_file + '.in',
      output: appinfo_test_desktop_file,
      install_dir: installed_tests_execdir,
      install_tag: 'tests',
      configuration: cdata,
    )
  else
    configure_file(
      input: appinfo_test_desktop_file + '.in',
      output: appinfo_test_desktop_file,
      configuration: cdata,
    )
  endif
endforeach

if installed_tests_enabled
  install_data(
    'contexts.c',
    'g-icon.c',
    'appinfo-test-actions.desktop',
    'appinfo-test-static.desktop',
    'file.c',
    'org.gtk.test.dbusappinfo.desktop',
    'org.gtk.test.dbusappinfo.flatpak.desktop',
    'test1.overlay',
    install_dir : installed_tests_execdir,
    install_tag : 'tests',
  )
  install_subdir('x-content',
    install_dir : installed_tests_execdir,
    install_tag : 'tests',
  )
  install_subdir('desktop-files',
    install_dir : installed_tests_execdir,
    install_tag : 'tests',
  )
  install_subdir('thumbnails',
    install_dir : installed_tests_execdir,
    install_tag : 'tests',
  )
  install_subdir('cert-tests',
    install_dir : installed_tests_execdir,
    install_tag : 'tests',
  )

  cdata = configuration_data()
  cdata.set('installed_tests_dir', installed_tests_execdir)
  cdata.set('program', 'static-link.py ' + glib_pkgconfigreldir)
  configure_file(
    input: installed_tests_template,
    output: 'static-link.test',
    install_dir: installed_tests_metadir,
    install_tag: 'tests',
    configuration: cdata
  )
  install_subdir('static-link',
    install_dir : installed_tests_execdir,
    install_tag : 'tests',
  )
  install_data('static-link.py',
    install_dir : installed_tests_execdir,
    install_tag : 'tests',
  )

  monitor_tests = [
    'memory-monitor-dbus',
    'memory-monitor-portal',
    'power-profile-monitor-dbus',
    'power-profile-monitor-portal'
  ]

  foreach monitor_test : monitor_tests
    cdata = configuration_data()
    cdata.set('installed_tests_dir', installed_tests_execdir)
    cdata.set('program', monitor_test + '.py')
    cdata.set('env', '')
    configure_file(
      input: installed_tests_template_tap,
      output: monitor_test + '.test',
      install_dir: installed_tests_metadir,
      install_tag: 'tests',
      configuration: cdata
    )
    cdata = configuration_data()
    cdata.set('libexecdir', join_paths(glib_prefix, get_option('libexecdir')))
    configure_file(
      input: monitor_test + '.py.in',
      output: monitor_test + '.py',
      install_dir : installed_tests_execdir,
      install_tag : 'tests',
      configuration: cdata,
    )
  endforeach
endif

if meson.can_run_host_binaries()

  compiler_type = '--compiler=@0@'.format(cc.get_id())

  if get_option('default_library') != 'static'
    plugin_resources_c = custom_target('plugin-resources.c',
      input : 'test4.gresource.xml',
      output : 'plugin-resources.c',
      command : [glib_compile_resources,
                 compiler_type,
                 '--target=@OUTPUT@',
                 '--sourcedir=' + meson.current_source_dir(),
                 '--internal',
                 '--generate-source',
                 '--c-name', '_g_plugin',
                 '@INPUT@'])

    shared_module('resourceplugin', 'resourceplugin.c', plugin_resources_c,
      link_args : export_dynamic_ldflags,
      dependencies : common_gio_tests_deps,
      install_dir : installed_tests_execdir,
      install_tag : 'tests',
      install : installed_tests_enabled
    )
  endif

  # referenced by test2.gresource.xml
  big_test_resource = custom_target(
    'gresource-big-test.txt',
    input : ['gen-big-test-resource.py'],
    output : ['gresource-big-test.txt'],
    command : [python, '@INPUT0@', '@OUTPUT@'])

  test_gresource = custom_target('test.gresource',
    input : 'test.gresource.xml',
    output : 'test.gresource',
    command : [glib_compile_resources,
               compiler_type,
               '--target=@OUTPUT@',
               '--sourcedir=' + meson.current_source_dir(),
               '--sourcedir=' + meson.current_build_dir(),
               '--internal',
               '@INPUT@'],
    install_dir : installed_tests_execdir,
    install_tag : 'tests',
    install : installed_tests_enabled)

  test_resources2_c = custom_target('test_resources2.c',
    input : 'test3.gresource.xml',
    output : 'test_resources2.c',
    command : [glib_compile_resources,
               compiler_type,
               '--target=@OUTPUT@',
               '--sourcedir=' + meson.current_source_dir(),
               '--internal',
               '--generate',
               '--c-name', '_g_test2',
               '--manual-register',
               '@INPUT@'])

  test_resources2_h = custom_target('test_resources2.h',
    input : 'test3.gresource.xml',
    output : 'test_resources2.h',
    command : [glib_compile_resources,
               compiler_type,
               '--target=@OUTPUT@',
               '--sourcedir=' + meson.current_source_dir(),
               '--internal',
               '--generate',
               '--c-name', '_g_test2',
               '--manual-register',
               '@INPUT@'])

  test_resources_c = custom_target('test_resources.c',
    input : 'test2.gresource.xml',
    depends : big_test_resource,
    output : 'test_resources.c',
    command : [glib_compile_resources,
               compiler_type,
               '--target=@OUTPUT@',
               '--sourcedir=' + meson.current_source_dir(),
               '--sourcedir=' + meson.current_build_dir(),
               '--internal',
               '--generate-source',
               '--c-name', '_g_test1',
               '@INPUT@'])

  digit_test_resources_c = custom_target('digit_test_resources.c',
    input : '111_digit_test.gresource.xml',
    output : 'digit_test_resources.c',
    command : [glib_compile_resources,
               compiler_type,
               '--target=@OUTPUT@',
               '--sourcedir=' + meson.current_source_dir(),
               '--sourcedir=' + meson.current_build_dir(),
               '--internal',
               '--generate-source',
               '--manual-register',
               '@INPUT@'])

  digit_test_resources_h = custom_target('digit_test_resources.h',
    input : '111_digit_test.gresource.xml',
    output : 'digit_test_resources.h',
    command : [glib_compile_resources,
               compiler_type,
               '--target=@OUTPUT@',
               '--sourcedir=' + meson.current_source_dir(),
               '--internal',
               '--generate',
               '--manual-register',
               '@INPUT@'])

  # referenced by test.gresource.xml
  test_generated_txt = configure_file(input : 'test1.txt',
    output : 'test-generated.txt',
    copy : true,
  )

  resources_extra_sources = [
    test_gresource,
    test_resources_c,
    test_resources2_c,
    test_resources2_h,
    digit_test_resources_c,
    digit_test_resources_h,
  ]

  # Create object file containing resource data for testing the --external-data
  # option. Currently only GNU ld and objcopy, or (as of 2019) LLVM ld and
  # objcopy, support the right options.
  objcopy_supports_add_symbol = false
  objcopy = find_program('objcopy', required : false)
  if objcopy.found()
    # FIXME: This should be `check: true` because we never really expect
    # `objcopy --help` to fail, given that `objcopy` exists. However, it does
    # fail on FreeBSD because ELF Tool Chain has
    # [a bug](https://sourceforge.net/p/elftoolchain/code/3950/).
    # This can be changed back to `check: true` once our CI uses a FreeBSD
    # version which includes the fix.
    # See https://gitlab.gnome.org/GNOME/glib/-/merge_requests/2360#note_1318608
    objcopy_supports_add_symbol = run_command(objcopy, '--help', check: false).stdout().contains('--add-symbol')
  endif

  ld = find_program('ld', required : false)

  if build_machine.system() == 'linux' and objcopy.found() and objcopy_supports_add_symbol and ld.found()
    test_gresource_binary = custom_target('test5.gresource',
      input : 'test5.gresource.xml',
      output : 'test5.gresource',
      command : [glib_compile_resources,
                 compiler_type,
                 '--target=@OUTPUT@',
                 '--sourcedir=' + meson.current_source_dir(),
                 '--sourcedir=' + meson.current_build_dir(),
                 '--internal',
                 '@INPUT@'],
      install_dir : installed_tests_execdir,
      install_tag : 'tests',
      install : installed_tests_enabled)

    # Create resource data file
    test_resources_binary_c = custom_target('test_resources_binary.c',
      input : 'test5.gresource.xml',
      output : 'test_resources_binary.c',
      command : [glib_compile_resources,
                 compiler_type,
                 '--target=@OUTPUT@',
                 '--sourcedir=' + meson.current_source_dir(),
                 '--sourcedir=' + meson.current_build_dir(),
                 '--internal',
                 '--generate-source',
                 '--external-data',
                 '--c-name', '_g_binary_test1',
                 '@INPUT@'])

    # Create object file containing resource data
    test_resources_binary = custom_target('test_resources.o',
      input : test_gresource_binary,
      output : 'test_resources.o',
      command : [ld,
                 '-z', 'noexecstack',
                 '-r',
                 '-b','binary',
                 '@INPUT@',
                 '-o','@OUTPUT@'])

    # Rename symbol to match the one in the C file
    if cc.get_id() == 'gcc' and host_system == 'windows'
      underscore = '_'
    else
      underscore = ''
    endif
    test_resources_binary2 = custom_target('test_resources2.o',
      input : test_resources_binary,
      output : 'test_resources2.o',
      command : [objcopy,
                 '--strip-all',
                 '--add-symbol', underscore + '_g_binary_test1_resource_data=.data:0',
                 '@INPUT@',
                 '@OUTPUT@'])

    resources_extra_sources += [
      test_resources_binary_c,
      test_resources_binary2,
    ]
  endif

  gio_tests += {
    'resources' : {
      'extra_sources' : resources_extra_sources,
      # FIXME: https://gitlab.gnome.org/GNOME/glib/-/issues/1392
      'should_fail' : host_system == 'darwin',
    },
  }
endif

foreach test_name, extra_args : gio_tests
  source = extra_args.get('source', test_name + '.c')
  extra_sources = extra_args.get('extra_sources', [])
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

  exe = executable(test_name, [source, extra_sources],
    c_args : test_c_args + extra_args.get('c_args', []),
    cpp_args : test_cpp_args + extra_args.get('cpp_args', []),
    dependencies : common_gio_tests_deps + extra_args.get('dependencies', []),
    install_rpath : extra_args.get('install_rpath', ''),
    install_dir: installed_tests_execdir,
    install_tag: 'tests',
    install: install,
  )

  suite = ['gio'] + extra_args.get('suite', [])
  timeout = suite.contains('slow') ? test_timeout_slow : test_timeout
  local_test_env = test_env

  foreach var, value : extra_args.get('env', {})
    local_test_env.append(var, value)
  endforeach

  test(test_name, exe,
    env : local_test_env,
    timeout : timeout,
    suite : suite,
    is_parallel : extra_args.get('is_parallel', true),
    depends : extra_args.get('depends', []),
    should_fail : extra_args.get('should_fail', false),
  )
endforeach

foreach program_name, extra_args : test_extra_programs
  source = extra_args.get('source', program_name + '.c')
  extra_sources = extra_args.get('extra_sources', [])
  install = installed_tests_enabled and extra_args.get('install', true)
  executable(program_name, [source, extra_sources],
      c_args : test_c_args,
      dependencies : common_gio_tests_deps + extra_args.get('dependencies', []),
      install_dir : installed_tests_execdir,
      install_tag : 'tests',
      install : install,
  )
endforeach

foreach test_name : python_tests
  test(
    test_name,
    python,
    args: ['-B', files(test_name)],
    env: test_env,
    suite: ['gio', 'no-valgrind'],
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

# TAP test runner for Python tests
if installed_tests_enabled
  install_data(
    files('taptestrunner.py'),
    install_dir: installed_tests_execdir,
    install_tag: 'tests',
  )
endif

subdir('services')

if get_option('default_library') != 'static'
  subdir('modules')
endif
