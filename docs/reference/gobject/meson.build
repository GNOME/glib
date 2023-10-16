if get_option('gtk_doc')
  subdir('xml')

  ignore_headers = [
    'tests',
    'gatomicarray.h',
    'gobject_probes.h',
    'gobject_trace.h',
    'gtype-private.h',
    'glib-enumtypes.h',
    'gobject-visibility.h',
  ]

  docpath = join_paths(glib_datadir, 'gtk-doc', 'html')
  version_conf = configuration_data()
  version_conf.set('GLIB_VERSION', meson.project_version())
  configure_file(
    input: 'version.xml.in',
    output: 'version.xml',
    configuration: version_conf
  )

  gtkdocincl = include_directories('.')

  gnome.gtkdoc('gobject',
    main_xml : 'gobject-docs.xml',
    namespace : 'g',
    mode : 'none',
    dependencies : [libgobject_dep, libglib_dep],
    include_directories : [gtkdocincl],
    src_dir : 'gobject',
    scan_args : [
      '--ignore-decorators=' + '|'.join(ignore_decorators.replace('GLIB', 'GOBJECT')),
      '--rebuild-types',
      '--ignore-headers=' + ' '.join(ignore_headers),
    ],
    content_files : [
      'glib-mkenums.xml',
      'glib-genmarshal.xml',
      'gobject-query.xml',
      'tut_tools.xml'
    ],
    html_assets : [
      'images/glue.png'
    ],
    fixxref_args: [
      '--html-dir=' + docpath,
      '--extra-dir=' + join_paths('gobject', '..', 'glib', 'html'),
    ],
    install: true,
    check: false,
  )
endif

if get_option('man')
  manpages = ['glib-mkenums', 'glib-genmarshal', 'gobject-query']
  foreach page : manpages
    custom_target(page + '-man',
      input: page + '.xml',
      output: page + '.1',
      command: xsltproc_command,
      install: true,
      install_dir: man1_dir)
  endforeach
endif

# gi-docgen version
expand_content_files = [
  'boxed.md',
  'concepts.md',
  'enum-types.md',
  'floating-refs.md',
  'gvalue.md',
  'tutorial.md',
  'types.md',
  'signals.md',
]

gobject_gir = meson.current_source_dir() / 'GObject-2.0.gir'
gobject_toml = configure_file(input: 'gobject.toml.in', output: 'gobject.toml', configuration: toml_conf)

custom_target('gobject-docs',
  input: [ gobject_toml, gobject_gir ],
  output: 'gobject',
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
