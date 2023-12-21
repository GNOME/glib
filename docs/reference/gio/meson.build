if get_option('man-pages').enabled()
  manpages = ['gapplication', 'gio-querymodules', 'glib-compile-schemas',
              'glib-compile-resources', 'gsettings', 'gresource', 'gdbus',
              'gio', 'gdbus-codegen']
  foreach page : manpages
    custom_target(page + '-man',
      input: page + '.rst',
      output: page + '.1',
      command: [
        rst2man,
        rst2man_flags,
        '@INPUT@',
      ],
      capture: true,
      install: true,
      install_dir: man1_dir,
      install_tag: 'doc',
    )
  endforeach
endif

expand_content_files = [
  'dbus-error.md',
  'dbus-introspection.md',
  'dbus-name-owning.md',
  'dbus-name-watching.md',
  'dbus-utils.md',
  'error.md',
  'file-attributes.md',
  'io-scheduler.md',
  'menu-exporter.md',
  'migrating-gconf.md',
  'migrating-gdbus.md',
  'migrating-gnome-vfs.md',
  'migrating-posix.md',
  'networking.md',
  'overview.md',
  'pollable-utils.md',
  'tls-overview.md',
  'unix-mounts.md',
]

gio_toml = configure_file(input: 'gio.toml.in', output: 'gio.toml', configuration: toml_conf)

custom_target('gio-docs',
  input: [ gio_toml, gio_gir[0] ],
  output: 'gio',
  command: [
    gidocgen,
    'generate',
    gidocgen_common_args,
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '--add-include-path=@0@'.format(meson.current_build_dir() / '../../../gobject'),
    '--add-include-path=@0@'.format(meson.current_build_dir() / '../../../gmodule'),
    '--add-include-path=@0@'.format(meson.current_build_dir() / '../../../gio'),
    '@INPUT1@',
  ],
  build_by_default: true,
  depend_files: expand_content_files,
)
