
gi_identifier_prefix = 'G'
gi_symbol_prefix = 'g'

gi_gen_shared_sources = [
  # Required to compile gdump
  gmodule_visibility_h,
]

gi_gen_env_variables = environment()

if get_option('b_sanitize') != ''
  gi_gen_env_variables.append(
    'ASAN_OPTIONS', 'verify_asan_link_order=0', separator: ',')
endif

# GLib
glib_gir = gnome.generate_gir(libglib,
  sources: [
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
  ],
  namespace: 'GLib',
  nsversion: '2.0',
  identifier_prefix: gi_identifier_prefix,
  symbol_prefix: gi_symbol_prefix,
  export_packages: 'glib-2.0',
  header: 'glib.h',
  install: true,
  dependencies: [
    libgobject_dep,
  ],
  env: gi_gen_env_variables,
  extra_args: gir_args + [
    '-DGLIB_COMPILATION',
    '-DGETTEXT_PACKAGE="dummy"',
    '--symbol-prefix=glib',
    '--library-path=' + meson.current_build_dir(),
    '--library=gobject-2.0',
  ],
)

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
  includes: [ glib_gir[0] ],
  install: true,
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
  includes: [ glib_gir[0] ],
  install: true,
  dependencies: [
    libglib_dep,
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
  appinfo_sources,
  contenttype_sources,
  unix_sources,
  win32_sources,
  settings_sources,
]
gio_gir_packages = [ 'gio-2.0' ]
gio_gir_args = [
  '-DGIO_COMPILATION',
  '-DG_SETTINGS_ENABLE_BACKEND',
  '--symbol-prefix=gio',
]
if host_system == 'windows'
  gio_gir_sources += gio_win32_include_headers
  foreach h: gio_win32_include_headers
    gio_gir_args += '--c-include=@0@'.format(h)
  endforeach
  gio_gir_packages += 'gio-win32-2.0'
  gio_gir_args += '--pkg=gio-win32-2.0'
else
  gio_gir_sources += gio_unix_include_headers
  foreach h: gio_unix_include_headers
    gio_gir_args += '--c-include=@0@'.format(h)
  endforeach
  gio_gir_packages += 'gio-unix-2.0'
  gio_gir_args += '--pkg=gio-unix-2.0'
endif

gio_gir = gnome.generate_gir(libgio,
  sources: gio_gir_sources,
  namespace: 'Gio',
  nsversion: '2.0',
  identifier_prefix: gi_identifier_prefix,
  symbol_prefix: gi_symbol_prefix,
  export_packages: gio_gir_packages,
  header: 'gio/gio.h',
  includes: [ glib_gir[0], gmodule_gir[0], gobject_gir[0] ],
  install: true,
  dependencies: [
    libglib_dep,
    libgobject_dep,
    libgmodule_dep,
  ],
  env: gi_gen_env_variables,
  extra_args: gir_args + gio_gir_args,
)

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

girepository_gir = gnome.generate_gir(libgirepository,
  sources: libgirepository_gir_sources,
  namespace: 'GIRepository',
  nsversion: '3.0',
  identifier_prefix: 'GI',
  symbol_prefix: 'gi',
  export_packages: libgirepository_gir_packages,
  header: 'girepository/girepository.h',
  includes: [ glib_gir[0], gmodule_gir[0], gobject_gir[0], gio_gir[0] ],
  install: true,
  dependencies: [ libglib_dep, libgobject_dep, libgmodule_dep, libgio_dep ],
  extra_args: gir_args + libgirepository_gir_args,
)

