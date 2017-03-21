if get_option('with-docs') != 'no'
  subdir('xml')

  ignore_headers = [
    'gallocator.h',
    'gdatasetprivate.h',
    'glibintl.h',
    'gbsearcharray.h',
    'glib-private.h',
    'gmoduleconf.h',
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
    'glib-mirroring-tab',
    'gnulib',
    'pcre',
    'update-pcre',
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
  ]

  docpath = join_paths(glib_datadir, 'gtk-doc', 'html')
  version_conf = configuration_data()
  version_conf.set('GLIB_VERSION', meson.project_version())
  configure_file(
    input: 'version.xml.in',
    output: 'version.xml',
    configuration: version_conf
  )

  gnome.gtkdoc('glib',
               main_xml : 'glib-docs.xml',
               namespace : 'g',
               mode : 'none',
               src_dir : [ 'glib', 'gmodule' ],
               dependencies : libglib_dep,
               scan_args : [
                 '--rebuild-types',
                 '--deprecated-guards=G_DISABLE_DEPRECATED',
                 '--ignore-decorators=GLIB_VAR|G_GNUC_WARN_UNUSED_RESULT',
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
                 # TODO: gobject/gio extra dirs
               ],
               install: true)
endif

if get_option('with-man') != 'no' and xsltproc.found()
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
