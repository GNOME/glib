expand_content_files = [
  'modules.md',
]

gmodule_gir = meson.current_source_dir() / 'GModule-2.0.gir'
gmodule_toml = configure_file(input: 'gmodule.toml.in', output: 'gmodule.toml', configuration: toml_conf)

custom_target('gmodule-docs',
  input: [ gmodule_toml, gmodule_gir ],
  output: 'gmodule',
  command: [
    gidocgen,
    'generate',
    gidocgen_common_args,
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '--add-include-path=@0@'.format(meson.current_source_dir() / '../glib'),
    '@INPUT1@',
  ],
  build_by_default: true,
  depend_files: expand_content_files,
)
