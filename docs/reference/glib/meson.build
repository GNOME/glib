if get_option('gtk_doc')
  subdir('xml')

  ignore_headers = [
    'gallocator.h',
    'gdatasetprivate.h',
    'glibintl.h',
    'gbsearcharray.h',
    'glib-private.h',
    'gmoduleconf.h',
    'grcboxprivate.h',
    'gstdioprivate.h',
    'gthreadprivate.h',
    'gunibreak.h',
    'gunicomp.h',
    'gunidecomp.h',
    'gunichartables.h',
    'glib_probes.h',
    'glib_trace.h',
    'libcharset.h',
    'gdebug.h',
    'gprintfint.h',
    'gmirroringtable.h',
    'gscripttable.h',
    'gtrace-private.h',
    'glib-mirroring-tab',
    'gnulib',
    'gbytesprivate.h',
    'gvariant-internal.h',
    'gvariant-serialiser.h',
    'gvariant-core.h',
    'gvarianttypeinfo.h',
    'gwakeup.h',
    'gtranslit-data.h',
    'glib-init.h',
    'gconstructor.h',
    'gconstructorprivate.h',
    'testutils.h',
    'valgrind.h',
    'gutilsprivate.h',
    'gvalgrind.h',
    'dirent.h',
    'glib-unixprivate.h',
    'glib-visibility.h',
    'gmodule-visibility.h',
  ]

  docpath = join_paths(glib_datadir, 'gtk-doc', 'html')
  version_conf = configuration_data()
  version_conf.set('GLIB_VERSION', meson.project_version())
  configure_file(
    input: 'version.xml.in',
    output: 'version.xml',
    configuration: version_conf
  )

  configure_file(
    input: 'glib-sections.txt.in',
    output: 'glib-sections.txt',
    command: [gen_visibility_macros, meson.project_version(), 'doc-sections', '@INPUT@', '@OUTPUT@'],
  )

  gnome.gtkdoc('glib',
               main_xml : 'glib-docs.xml',
               namespace : 'g',
               mode : 'none',
               src_dir : [ 'glib', 'gmodule' ],
               dependencies : libglib_dep,
               scan_args : [
                 '--ignore-decorators=' + ignore_decorators + '|' + ignore_decorators.replace('GLIB', 'GMODULE'),
                 '--ignore-headers=' + ' '.join(ignore_headers),
               ],
               content_files : [
                 'changes.xml',
                 'programming.xml',
                 'resources.xml',
                 'regex-syntax.xml',
                 'glib-gettextize.xml',
                 'gtester.xml',
                 'gtester-report.xml',
               ],
               expand_content_files : [
               ],
               html_assets : [
                 'file-name-encodings.png',
                 'mainloop-states.gif',
                 'Sorted_binary_tree_breadth-first_traversal.svg',
                 'Sorted_binary_tree_inorder.svg',
                 'Sorted_binary_tree_postorder.svg',
                 'Sorted_binary_tree_preorder.svg',
               ],
               fixxref_args: [
                 '--html-dir=' + docpath,
               ],
               install: true,
               check: false)
endif

if get_option('man')
  manpages = ['glib-gettextize', 'gtester', 'gtester-report']
  foreach page : manpages
    custom_target(page + '-man',
      input: page + '.xml',
      output: page + '.1',
      command: xsltproc_command,
      install: true,
      install_dir: man1_dir)
  endforeach
endif

if get_option('gtk_doc')
  # GVariant specification is currently standalone
  rst2html5 = find_program('rst2html5', 'rst2html5.py', required: false)

  if rst2html5.found()
    spec_path = glib_datadir / 'doc' / 'glib-2.0'

    figures = files(
      'gvariant-byte-boundaries.svg',
      'gvariant-integer-and-string-structure.svg',
      'gvariant-integer-array.svg',
      'gvariant-string-array.svg',
    )

    custom_target('gvariant-specification-1.0',
      input: 'gvariant-specification-1.0.rst',
      output: 'gvariant-specification-1.0.html',
      command: [
        rst2html5,
        '@INPUT@',
      ],
      capture: true,
      install: true,
      install_dir: spec_path,
      install_tag: 'doc',
      depend_files: figures,
    )

    install_data(figures,
      install_dir : spec_path,
      install_tag : 'doc',
    )
  endif
endif

# gi-docgen version
expand_content_files = [
  'building.md',
  'character-set.md',
  'compiling.md',
  'cross-compiling.md',
  'error-reporting.md',
  'gvariant-format-strings.md',
  'gvariant-text-format.md',
  'i18n.md',
  'logging.md',
  'main-loop.md',
  'memory-slices.md',
  'reference-counting.md',
  'running.md',
  'testing.md',
  'threads.md',
  'markup.md',
  'goption.md',
  'data-structures.md',
  'unicode.md',
]

glib_toml = configure_file(input: 'glib.toml.in', output: 'glib.toml', configuration: toml_conf)

custom_target('glib-docs',
  input: [ glib_toml, glib_gir[0] ],
  output: 'glib',
  command: [
    gidocgen,
    'generate',
    gidocgen_common_args,
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  build_by_default: true,
  depend_files: expand_content_files,
)
