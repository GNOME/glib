kqueue_sources = [
  'gkqueuefilemonitor.c',
  'kqueue_fnm.c',
]

kqueue_lib = static_library('kqueue',
  sources : kqueue_sources,
  include_directories : [configinc, glibinc, gmoduleinc],
  dependencies : [gioenumtypes_dep],
  pic : true,
  c_args : [ '-DG_DISABLE_DEPRECATED' ] + gio_c_args)
