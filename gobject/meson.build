gobject_install_headers = [
  'gobject-autocleanups.h',
  'glib-types.h',
  'gbinding.h',
  'gboxed.h',
  'gclosure.h',
  'genums.h',
  'gmarshal.h',
  'gobject.h',
  'gparam.h',
  'gparamspecs.h',
  'gsignal.h',
  'gsourceclosure.h',
  'gtype.h',
  'gtypemodule.h',
  'gtypeplugin.h',
  'gvalue.h',
  'gvaluearray.h',
  'gvaluecollector.h',
  'gvaluetypes.h',
  'gobjectnotifyqueue.c', # sic
]
install_headers(gobject_install_headers, subdir : 'glib-2.0/gobject')

gobject_c_sources = [
#  'gobject_probes.d',
  'gatomicarray.c',
  'gbinding.c',
  'gboxed.c',
  'gclosure.c',
  'genums.c',
  'gmarshal.c',
  'gobject.c',
  'gobject_trace.h',
  'gparam.c',
  'gparamspecs.c',
  'gsignal.c',
  'gsourceclosure.c',
  'gtype.c',
  'gtypemodule.c',
  'gtypeplugin.c',
  'gvalue.c',
  'gvaluearray.c',
  'gvaluetransform.c',
  'gvaluetypes.c',
]

libgobject = shared_library('gobject-2.0',
  sources : [gobject_c_sources],
  version : library_version,
  soversion : soversion,
  install : true,
  include_directories : [configinc],
  dependencies : [libffi_dep, libglib_dep],
  c_args : ['-DG_LOG_DOMAIN="GLib-GObject"', '-DGOBJECT_COMPILATION'])

libgobject_dep = declare_dependency(link_with : libgobject,
  include_directories : gobjectinc)

glib_mkenums_conf = configuration_data()
glib_mkenums_conf.set('GLIB_VERSION', glib_version)
glib_mkenums_conf.set('PERL_PATH', perl.path())

# FIXME: Set permissions
glib_mkenums = configure_file(input : 'glib-mkenums.in',
  output : 'glib-mkenums',
  install : true,
  install_dir : 'bin', configuration : glib_mkenums_conf)

executable('gobject-query', 'gobject-query.c',
  install : true,
  c_args : ['-DHAVE_CONFIG_H=1'],
  dependencies : [libglib_dep, libgobject_dep])

gmarshal_strings = custom_target('gmarshal.strings',
    input : ['gmarshal-list-to-strings.py', 'gmarshal.list'],
    output : ['gmarshal.strings'],
    command : [python, '@INPUT0@', '@INPUT1@', '@OUTPUT@'])

glib_genmarshal = executable('glib-genmarshal',
  gmarshal_strings, 'glib-genmarshal.c',
  install : true,
  c_args : ['-DHAVE_CONFIG_H=1'],
  dependencies : [libglib_dep, libgobject_dep])

install_data('gobject_gdb.py', install_dir : join_paths(glib_pkgdatadir + 'gdb'))
gdb_conf = configuration_data()
gdb_conf.set('datadir', glib_datadir)
configure_file(
  input: 'libgobject-gdb.py.in',
  output: 'libgobject-2.0.so.@0@-gdb.py'.format(library_version),
  configuration: gdb_conf,
  install: true,
  install_dir: join_paths(get_option('datadir'), 'gdb/auto-load/' + glib_libdir)
)

subdir('tests')
