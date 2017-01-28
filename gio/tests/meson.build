subdir('gdbus-object-manager-example')

gengiotypefuncs_prog = find_program('gengiotypefuncs.py')
giotypefuncs_inc = custom_target(
  'giotypefuncs.inc',
  output : 'giotypefuncs.inc',
  input : gio_headers + [gioenumtypes_h] + gobject_install_headers,
  command: [gengiotypefuncs_prog, '@OUTPUT@', '@INPUT@'])

#  Test programs buildable on all platforms
gio_tests = [
  'appmonitor',
  'async-close-output-stream',
  'async-splice-output-stream',
  'buffered-input-stream',
  'buffered-output-stream',
  'cancellable',
  'contexts',
  'contenttype',
  'converter-stream',
  'credentials',
  'data-input-stream',
  'data-output-stream',
  'defaultvalue',
  'fileattributematcher',
  'filter-streams',
  'giomodule',
  'gsubprocess',
  'g-file',
  'g-file-info',
  'g-icon',
  'gdbus-addresses',
  'gdbus-message',
  'inet-address',
  'io-stream',
  'memory-input-stream',
  'memory-output-stream',
  'monitor',
  'network-address',
  'network-monitor',
  'permission',
  'pollable',
  'proxy-test',
  'readwrite',
  'simple-async-result',
  'simple-proxy',
  'sleepy-stream',
  'socket',
  'socket-listener',
  'socket-service',
  'srvtarget',
  'task',
  'vfs',
  'volumemonitor',
  'glistmodel',
  'testfilemonitor',
  'thumbnail-verification',
]

test_extra_programs = [
  ['gdbus-connection-flush-helper'],
  ['gdbus-testserver'],
]

#  Test programs buildable on UNIX only
if host_machine.system() != 'windows'
  gio_tests += [
    'file',
    'gdbus-peer',
    'gdbus-peer-object-manager',
    'live-g-file',
    'socket-address',
    'stream-rw_all',
    'unix-fd',
    'unix-streams',
    'mimeapps',
  ]

  test_extra_programs += [
    ['basic-application'],
    ['dbus-launch'],
    ['appinfo-test'],
    ['apps'],
  ]

  have_dbus_daemon = find_program('dbus-daemon', required : false).found()
  if have_dbus_daemon
    gio_tests += ['gdbus-unix-addresses']
  endif
endif

if cc.get_id() != 'msvc'
  gio_tests += [ 'autoptr' ]
endif

test_env = [
  'G_TEST_SRCDIR=' + meson.current_source_dir(),
  'G_TEST_BUILDDIR=' + meson.current_build_dir(),
]

test_c_args = [
  '-DHAVE_CONFIG_H=1',
  '-DG_LOG_DOMAIN="GLib-GIO"',
  '-DTEST_SERVICES="@0@/gio/tests/services"'.format(meson.build_root()),
]

foreach test_name : gio_tests
  extra_deps = []
  srcs = ['@0@.c'.format(test_name)]
  # conflicts with glib/tests/autoptr, can't have two targets with same name
  if test_name == 'autoptr'
    test_name = 'autoptr-gio'
  elif test_name == 'defaultvalue'
    srcs += [giotypefuncs_inc]
  elif test_name == 'gdbus-peer'
    # This is peer to peer so it doesn't need a session bus, so we can run
    # it automatically as a test by default
    extra_deps = [libgdbus_example_objectmanager_dep]
  endif
  exe = executable(test_name, srcs,
      install : false,
      c_args : test_c_args,
      dependencies : [libglib_dep, libgmodule_dep, libgio_dep] + extra_deps)
  if test_name == 'testfilemonitor'
    test(test_name, exe, env : test_env, timeout : 45)
  else
    test(test_name, exe, env : test_env)
  endif
endforeach

uninstalled_test_extra_programs = [
  ['gio-du'],
  ['echo-server'],
  ['filter-cat'],
  ['gapplication-example-actions'],
  ['gapplication-example-cmdline'],
  ['gapplication-example-cmdline2'],
  ['gapplication-example-cmdline3'],
  ['gapplication-example-dbushooks'],
  ['gapplication-example-open'],
  ['gdbus-daemon', gdbus_daemon_sources],
  ['gdbus-example-export'],
  ['gdbus-example-own-name'],
  ['gdbus-example-peer'],
  ['gdbus-example-proxy-subclass'],
  ['gdbus-example-server'],
  ['gdbus-example-subtree'],
  ['gdbus-example-watch-name'],
  ['gdbus-example-watch-proxy'],
  ['gsubprocess-testprog'],
  ['httpd'],
  ['proxy'],
  ['resolver'],
  ['send-data'],
  ['socket-server'],
  ['socket-client', ['gtlsconsoleinteraction.c']],
  ['tls-certificate', ['gtesttlsbackend.c']],
  ['tls-interaction', ['gtesttlsbackend.c']],
  # These three are manual-run tests because they need a session bus but don't bring one up themselves
  # FIXME: these build but don't seem to work!
  ['gdbus-example-objectmanager-client', [], [libgdbus_example_objectmanager_dep]],
  ['gdbus-example-objectmanager-server', [], [libgdbus_example_objectmanager_dep]],
  ['gdbus-test-fixture', [], [libgdbus_example_objectmanager_dep]],
]

if host_machine.system() != 'windows'
  uninstalled_test_extra_programs += [['gdbus-example-unix-fd-client']]
endif

foreach extra_program : uninstalled_test_extra_programs + test_extra_programs
  srcs = ['@0@.c'.format(extra_program[0])]
  if extra_program.length() > 1
    srcs += extra_program[1]
  endif
  extra_deps = []
  if extra_program.length() > 2
    extra_deps = extra_program[2]
  endif
  executable(extra_program[0], srcs,
      install : false,
      c_args : test_c_args,
      dependencies : [libglib_dep, libgmodule_dep, libgio_dep] + extra_deps)
endforeach

# FIXME: Add resources test

foo = '''
c_args_atomic = [ ]
if cc.get_id() == 'gcc'
  c_args_atomic += [ '-Wstrict-aliasing=2' ]
endif

# FIXME: consolidate all of these into the array
exe = executable('overflow-fallback', 'overflow.c',
  c_args : [ '-DHAVE_CONFIG_H=1', '-D_GLIB_TEST_OVERFLOW_FALLBACK' ],
  dependencies : [libglib_dep],
)
test('overflow-fallback', exe, env : test_env)

exe = executable('642026-ec', '642026.c',
  c_args : [ '-DHAVE_CONFIG_H=1', '-DG_ERRORCHECK_MUTEXES' ],
  dependencies : [libglib_dep],
)
test('642026-ec', exe, env : test_env)

exe = executable('1bit-emufutex', '1bit-mutex.c',
  c_args : [ '-DHAVE_CONFIG_H=1', '-DTEST_EMULATED_FUTEX' ],
  dependencies : [libglib_dep],
)
test('1bit-emufutex', exe, env : test_env)

if glib_conf.has('HAVE_EVENTFD')
  exe = executable('gwakeup-fallback', 'gwakeuptest.c', '../gwakeup.c',
    c_args : [ '-DHAVE_CONFIG_H=1', '-DTEST_EVENTFD_FALLBACK' ],
    dependencies : [libglib_dep],
  )
  test('gwakeup-fallback', exe, env : test_env)
endif

# test-spawn-echo helper binary required by the spawn tests
executable('test-spawn-echo', 'test-spawn-echo.c',
           c_args : [ '-DHAVE_CONFIG_H=1', '-DG_LOG_DOMAIN="GLib"' ],
           dependencies : [libglib_dep],
)
'''

subdir('modules')
