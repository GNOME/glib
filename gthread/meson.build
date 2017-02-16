# Just a skeleton lib for backwards compatibility since all the functionaliy
# has been moved into glib now
libgthread = shared_library('gthread-2.0',
  sources : [ 'gthread-impl.c' ],
  version : library_version,
  soversion : soversion,
  install : true,
  dependencies : [libglib_dep],
  c_args : ['-DG_LOG_DOMAIN="GThread"' ] + glib_hidden_visibility_args)
