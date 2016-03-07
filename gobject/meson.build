#FIXME
#if host_machine.system() == 'windows'
#  plat_src = []
#else
#  plat_src = []
#endif

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
install_headers(gobject_install_headers, subdir : 'glib-2.0/gobject/')

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

libgobject = shared_library('gobject',
sources : [ gobject_c_sources ],
version : glib_version,
soversion : interface_version,
install : true,
include_directories : inc_dirs,
link_with : libglib,
dependencies : libffi_dep,
c_args : ['-DG_LOG_DOMAIN="GLib-GObject"', '-DGOBJECT_COMPILATION' ])

subdir('tests')
