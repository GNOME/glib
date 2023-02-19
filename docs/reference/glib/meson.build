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
                 'cross.xml',
                 'running.xml',
                 'building.xml',
                 'changes.xml',
                 'compiling.xml',
                 'programming.xml',
                 'resources.xml',
                 'regex-syntax.xml',
                 'glib-gettextize.xml',
                 'gtester.xml',
                 'gtester-report.xml',
                 'gvariant-varargs.xml',
                 'gvariant-text.xml',
               ],
               expand_content_files : [
                 'compiling.xml',
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
               check: true)
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