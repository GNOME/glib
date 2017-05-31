if get_option('with-docs') != 'no'
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
    gobject_typesfile : join_paths(meson.current_source_dir(), 'gobject.types'),
    mode : 'none',
    dependencies : [libgobject_dep, libglib_dep],
    src_dir : 'gobject',
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

if get_option('with-man') != 'no' and xsltproc.found()
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
