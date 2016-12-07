if get_option('with-docs')
  subdir('xml')

  ignore_headers = [
    'tests',
    'gatomicarray.h',
    'gobject_trace.h',
    'gtype-private.h'
  ]

  docpath = join_paths(glib_datadir, 'gtk-doc', 'html')
  version_conf = configuration_data()
  version_conf.set('GLIB_VERSION', meson.project_version())
  configure_file(
    input: 'version.xml.in',
    output: 'version.xml',
    configuration: version_conf
  )

  gnome.gtkdoc('gobject',
    main_xml : 'gobject-docs.xml',
    namespace : 'g',
    gobject_typesfile : 'gobject.types',
    mode : 'none',
    dependencies : [libgobject_dep, libglib_dep],
    src_dir : [
      # TODO: Cleanup upstream
      join_paths(meson.source_root(), 'gobject'),
      join_paths(meson.build_root(), 'gobject'),
    ],
    scan_args : [
      '--deprecated-guards=G_DISABLE_DEPRECATED',
      '--ignore-decorators=G_GNUC_INTERNAL|G_GNUC_WARN_UNUSED_RESULT',
      '--ignore-headers=' + ' '.join(ignore_headers),
    ],
    content_files : [
      'glib-mkenums.xml',
      'glib-genmarshal.xml',
      'gobject-query.xml',
      'tut_gobject.xml',
      'tut_gsignal.xml',
      'tut_gtype.xml',
      'tut_howto.xml',
      'tut_intro.xml',
      'tut_tools.xml'
    ],
    html_assets : [
      'images/glue.png'
    ],
    fixxref_args: [
      '--html-dir=' + docpath,
      '--extra-dir=' + join_paths(meson.current_build_dir(), '../glib/html'),
    ],
    install: true
  )
endif

if get_option('with-man')
  custom_target('glib-mkenums-man',
    input: 'glib-mkenums.xml',
    output: 'glib-mkenums.1',
    command: xsltproc_command,
    install: true,
    install_dir: man1_dir,
  )

  custom_target('glib-genmarshal-man',
    input: 'glib-genmarshal.xml',
    output: 'glib-genmarshal.1',
    command: xsltproc_command,
    install: true,
    install_dir: man1_dir,
  )

  custom_target('gobject-query-man',
    input: 'gobject-query.xml',
    output: 'gobject-query.1',
    command: xsltproc_command,
    install: true,
    install_dir: man1_dir,
  )
endif
