charset_lib = static_library('charset', 'localcharset.c',
  include_directories : configinc,
  pic : true,
  c_args : [ '-DLIBDIR="@0@"'.format(get_option('libdir')) ] + glib_hidden_visibility_args)
