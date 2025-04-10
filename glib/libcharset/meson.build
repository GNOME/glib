charset_lib = static_library('charset', 'localcharset.c',
  include_directories : configinc,
  pic : true,
  c_args : [
    '-DGLIB_CHARSETALIAS_DIR="@0@"'.format(glib_charsetaliasdir),
    '-DENABLE_RELOCATABLE=0',
    '-DHAVE_DECL_GETC_UNLOCKED=@0@'.format(have_func_getc_unlocked ? 1 : 0),
  ],
  gnu_symbol_visibility : 'hidden',
)
