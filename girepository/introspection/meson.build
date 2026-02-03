
gi_identifier_prefix = 'G'
gi_symbol_prefix = 'g'

gi_gen_shared_sources = [
  # Required to compile gdump
  gmodule_visibility_h,
]

gi_gen_shared_dependencies = [
  # libgirepository is not needed by most of the modules below, but it is needed
  # by the g-ir-scanner generated dumper program. If we don’t explicitly include
  # the local version of it here, Meson will implicitly link against it anyway,
  # and that might pull in a different version, or try to link against a
  # half-built local version as the build ordering dependency tree won’t
  # reflect this relationship.
  libgirepository_dep,
]

gi_gen_env_variables = environment()

if 'address' in glib_sanitizers
  gi_gen_env_variables.append(
    'ASAN_OPTIONS', 'verify_asan_link_order=0', separator: ',')
endif

if host_system == 'windows'
  # Use gio-2.0-uninstalled.pc to find the paths where the GLib DLLs (and their dependent non-system
  # DLLs that are built as subprojects) are located
  check_built_dll_paths_cmd = [get_dll_paths_script, '--build-path=@0@'.format(meson.project_build_root())]
  meson_pkgconfig_paths = get_option('pkg_config_path')
  if meson_pkgconfig_paths.length() > 0
    check_built_dll_paths_cmd += '--pkg-config-path=@0@'.format(''.join(meson_pkgconfig_paths))
  endif
  check_built_dll_paths = run_command(
    check_built_dll_paths_cmd,
	capture: true,
    check: true,
  )
  # hmm, no os.pathsep in Meson to make this more portable, if needed?
  gi_scanner_dll_paths = check_built_dll_paths.stdout().strip().split(';')

  # Also assume the existing paths in %PATH% to be considered for DLLs
  message('Ensure that all of GLib\'s dependent non-system DLLs that are not built')
  message('alongside with GLib can be found in paths in %PATH%')
  message('Check this if building .gir files fail due to \'ImportError: DLL load failed while importing _giscanner\'')

  gi_gen_env_variables.set('GI_EXTRA_BASE_DLL_DIRS', gi_scanner_dll_paths)
endif

# GLib
glib_gir_sources = [
  gi_gen_shared_sources,
  glibconfig_h,
  gversionmacros_h,
  glib_visibility_h,
  glib_headers,
  glib_deprecated_headers,
  glib_sub_headers,
  glib_enumtypes_h,
  glib_types_h,
  glib_deprecated_sources,
  glib_sources,
  files('../../glib/docs.c'),
]

glib_gi_libdir = meson.project_build_root() / 'glib'
gobject_gi_libdir = meson.project_build_root() / 'gobject'
gmodule_gi_libdir = meson.project_build_root() / 'gmodule'
gio_gi_libdir = meson.project_build_root() / 'gio'

glib_gi_libpaths = [
  '-L' + glib_gi_libdir,
  '-L' + gobject_gi_libdir,
  '-L' + gmodule_gi_libdir,
  '-L' + gio_gi_libdir,
]
# Includes that will be used to compile the scanner executable
glib_gi_incdir = meson.project_source_root() / 'glib'
gobject_gi_incdir = meson.project_source_root() / 'gobject'
gmodule_gi_incdir = meson.project_source_root() / 'gmodule'
gio_gi_incdir = meson.project_source_root() / 'gio'

glib_gi_includes = [
  '-I' + meson.project_source_root(),
  '-I' + meson.project_build_root(),
  '-I' + glib_gi_libdir,
  '-I' + glib_gi_incdir,
  '-I' + gobject_gi_incdir,
  '-I' + gmodule_gi_incdir,
  '-I' + gio_gi_incdir,
]

giscanner_common_args = [
    '--identifier-prefix=G',
    '--symbol-prefix=g',
]

glib_gir = []

glib_typelib = []

glib_gir += custom_target('glib-gir',
  input: [ glib_gir_sources ],
  output: 'GLib-2.0.gir',
  depends: [
    giscanner_pymod,
  ],
  command: [
    giscanner_bin,
    giscanner_common_args,
    '--symbol-prefix=glib',
    '--c-include=glib.h',
    '--namespace=GLib',
    '--nsversion=2.0',
    '--library=glib-2.0',
    '--library=gobject-2.0',
    '--pkg=glib-2.0',
    glib_gi_libpaths,
    '--cflags-begin',
    glib_gi_includes,
    '-DGLIB_COMPILATION',
    '-DGOBJECT_COMPILATION',
    '-D__G_I18N_LIB_H__',
    '-DGETTEXT_PACKAGE=dummy',
    '--cflags-end',
    '@INPUT@',
  ],
  install: true,
  install_dir: glib_girdir,
)

if host_system == 'windows'
  glib_win32_gir = gnome.generate_gir(libglib,
    sources: [
      gi_gen_shared_sources,
      glib_win32_headers,
      glib_win32_sources,
    ],
    namespace: 'GLibWin32',
    nsversion: '2.0',
    identifier_prefix: gi_identifier_prefix,
    symbol_prefix: gi_symbol_prefix,
    export_packages: 'glib-2.0',
    header: 'glib.h',
    includes: ['GLib-2.0'],
    install: true,
    install_dir_gir: glib_girdir,
    dependencies: gi_gen_shared_dependencies + [
      libgobject_dep,
      glib_gir,
    ],
    env: gi_gen_env_variables,
    extra_args: gir_args + [
      '-DGLIB_COMPILATION',
      '-DGETTEXT_PACKAGE="dummy"',
      '--symbol-prefix=glib',
      '--symbol-prefix=g_win32',
      '--identifier-prefix=GWin32',
      '--library-path=' + meson.current_build_dir(),
      '--library=gobject-2.0',
    ],
  )
else
  glib_unix_gir = gnome.generate_gir(libglib,
    sources: [
      gi_gen_shared_sources,
      glib_unix_headers,
      glib_unix_sources,
    ],
    namespace: 'GLibUnix',
    nsversion: '2.0',
    identifier_prefix: gi_identifier_prefix,
    symbol_prefix: gi_symbol_prefix,
    export_packages: 'glib-2.0',
    header: 'glib.h',
    includes: ['GLib-2.0'],
    install: true,
    install_dir_gir: glib_girdir,
    dependencies: gi_gen_shared_dependencies + [
      libgobject_dep,
      glib_gir,
    ],
    env: gi_gen_env_variables,
    extra_args: gir_args + [
      '-DGLIB_COMPILATION',
      '-DGETTEXT_PACKAGE="dummy"',
      '--symbol-prefix=glib',
      '--symbol-prefix=g_unix',
      '--identifier-prefix=GUnix',
      '--library-path=' + meson.current_build_dir(),
      '--library=gobject-2.0',
      '--c-include=glib-unix.h',
    ],
  )
endif

# GObject
gobject_gir = gnome.generate_gir(libgobject,
  sources: [
    gi_gen_shared_sources,
    gobject_visibility_h,
    gobject_install_headers,
    gobject_sources,
  ],
  namespace: 'GObject',
  nsversion: '2.0',
  identifier_prefix: gi_identifier_prefix,
  symbol_prefix: gi_symbol_prefix,
  export_packages: 'gobject-2.0',
  header: 'glib-object.h',
  includes: ['GLib-2.0'],
  install: true,
  install_dir_gir: glib_girdir,
  dependencies: [gi_gen_shared_dependencies, glib_gir],
  env: gi_gen_env_variables,
  extra_args: gir_args + [
      '-DGOBJECT_COMPILATION',
      '--symbol-prefix=gobject',
  ],
)

# GModule
gmodule_gir = gnome.generate_gir(libgmodule,
  sources: [
    gi_gen_shared_sources,
    gmoduleconf_h,
    gmodule_h,
    gmodule_c,
    gmodule_deprecated_c,
    gmodule_visibility_h,
  ],
  namespace: 'GModule',
  nsversion: '2.0',
  identifier_prefix: gi_identifier_prefix,
  symbol_prefix: gi_symbol_prefix,
  export_packages: 'gmodule-2.0',
  header: 'gmodule.h',
  includes: ['GLib-2.0'],
  install: true,
  install_dir_gir: glib_girdir,
  dependencies: gi_gen_shared_dependencies + [
    libglib_dep,
    glib_gir,
  ],
  env: gi_gen_env_variables,
  extra_args: gir_args + [
      '-DGMODULE_COMPILATION',
      '-DGETTEXT_PACKAGE="dummy"',
      '--symbol-prefix=gmodule',
  ],
)

# Gio
gio_gir_sources = [
  gi_gen_shared_sources,
  gio_visibility_h,
  gioenumtypes_h,
  gnetworking_h,
  gio_headers,
  gio_base_sources,
  application_sources,
  gdbus_sources,
  contenttype_sources,
  settings_sources,
]
gio_gir_packages = [ 'gio-2.0' ]
gio_gir_args = [
  '-DGIO_COMPILATION',
  '-DG_SETTINGS_ENABLE_BACKEND',
  '--symbol-prefix=gio',
]

gio_gir = gnome.generate_gir(libgio,
  sources: gio_gir_sources,
  namespace: 'Gio',
  nsversion: '2.0',
  identifier_prefix: gi_identifier_prefix,
  symbol_prefix: gi_symbol_prefix,
  export_packages: gio_gir_packages,
  header: 'gio/gio.h',
  includes: [ gmodule_gir[0], gobject_gir[0] ],
  install: true,
  install_dir_gir: glib_girdir,
  dependencies: gi_gen_shared_dependencies + [
    libglib_dep,
    libgobject_dep,
    libgmodule_dep,
    glib_gir,
  ],
  env: gi_gen_env_variables,
  extra_args: gir_args + gio_gir_args,
)

if host_system == 'windows'
  gio_win32_gir_c_includes = []
  foreach h: gio_win32_include_headers
    gio_win32_gir_c_includes += '--c-include=gio/' + fs.name(h)
  endforeach

  gio_win32_gir = gnome.generate_gir(libgio,
    sources: gio_win32_include_headers + win32_sources,
    namespace: 'GioWin32',
    nsversion: '2.0',
    identifier_prefix: gi_identifier_prefix,
    symbol_prefix: gi_symbol_prefix,
    export_packages: [ 'gio-windows-2.0' ],
    header: 'gio/gio.h',
    includes: [ gmodule_gir[0], gobject_gir[0], gio_gir[0] ],
    install: true,
    install_dir_gir: glib_girdir,
    dependencies: gi_gen_shared_dependencies + [
      libglib_dep,
      libgobject_dep,
      libgmodule_dep,
      glib_gir,
    ],
    env: gi_gen_env_variables,
    extra_args: gir_args + gio_gir_args + gio_win32_gir_c_includes + [
      '--pkg=gio-windows-2.0',
      '--symbol-prefix=g_win32',
      '--identifier-prefix=GWin32'
    ],
  )
  gio_platform_gir = gio_win32_gir
else
  gio_unix_gir_c_includes = []
  foreach h: gio_unix_include_headers
    gio_unix_gir_c_includes += '--c-include=gio/' + fs.name(h)
  endforeach

  gio_unix_gir = gnome.generate_gir(libgio,
    sources: gio_unix_include_headers + unix_sources,
    namespace: 'GioUnix',
    nsversion: '2.0',
    identifier_prefix: gi_identifier_prefix,
    symbol_prefix: gi_symbol_prefix,
    export_packages: [ 'gio-unix-2.0' ],
    header: 'gio/gio.h',
    includes: [ gmodule_gir[0], gobject_gir[0], gio_gir[0] ],
    install: true,
    install_dir_gir: glib_girdir,
    dependencies: gi_gen_shared_dependencies + [
      libglib_dep,
      libgobject_dep,
      libgmodule_dep,
      glib_gir,
    ],
    env: gi_gen_env_variables,
    extra_args: gir_args + gio_gir_args + gio_unix_gir_c_includes + [
      '--pkg=gio-unix-2.0',
      '--symbol-prefix=g_unix',
      '--identifier-prefix=GUnix'
    ],
  )
  gio_platform_gir = gio_unix_gir
endif

# GIRepository
libgirepository_gir_sources = [
  gi_visibility_h,
  girepo_headers,
  girepo_sources,
]
libgirepository_gir_packages = [ 'girepository-2.0' ]
libgirepository_gir_args = [
  '-DGI_COMPILATION',
  '--symbol-prefix=gi',
  '--identifier-prefix=GI',
]

gi_libgirepository_gen_env_variables = environment()

if host_system == 'windows'
  gi_libgirepository_gen_env_variables.set('GI_EXTRA_BASE_DLL_DIRS', gi_scanner_dll_paths)
endif

girepository_gir = gnome.generate_gir(libgirepository,
  sources: libgirepository_gir_sources,
  namespace: 'GIRepository',
  nsversion: '3.0',
  identifier_prefix: 'GI',
  symbol_prefix: 'gi',
  export_packages: libgirepository_gir_packages,
  header: 'girepository/girepository.h',
  includes: [ gmodule_gir[0], gobject_gir[0], gio_gir[0] ],
  install: true,
  install_dir_gir: glib_girdir,
  dependencies: gi_gen_shared_dependencies + [
    libglib_dep,
    libgobject_dep,
    libgmodule_dep,
    libgio_dep,
    glib_gir,
  ],
  extra_args: gir_args + libgirepository_gir_args,
  env: gi_libgirepository_gen_env_variables,
)

foreach gir: glib_gir
  glib_typelib += custom_target(
    input: gir,
    output: '@BASENAME@.typelib',
    command: [
      gicompilerepository,
      '-o', '@OUTPUT@',
      '@INPUT@',
      '--includedir', meson.current_build_dir(),
      '--includedir', meson.current_source_dir(),
    ],
    install: true,
    install_dir: get_option('libdir') / 'girepository-1.0',
  )
endforeach
