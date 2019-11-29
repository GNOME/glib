# The list of minor versions in the 2.x.x series which have had
# GLIB_AVAILABLE_IN_* macros. This should include the current unreleased stable
# version.
#
# FIXME: It would be good to be able to generate this list:
# https://github.com/mesonbuild/meson/issues/5026
stable_2_series_versions = [
  '26', '28', '30', '32', '34', '36', '38',
  '40', '42', '44', '46', '48', '50', '52', '54', '56', '58',
  '60', '62', '64',
]

ignore_decorators = [
  'GLIB_VAR',
  'G_GNUC_INTERNAL',
  'G_GNUC_WARN_UNUSED_RESULT',
  'GLIB_AVAILABLE_IN_ALL',
]

foreach version : stable_2_series_versions
  ignore_decorators += [
    'GLIB_AVAILABLE_IN_2_' + version,
    'GLIB_DEPRECATED_IN_2_' + version,
    'GLIB_DEPRECATED_IN_2_' + version + '_FOR',
  ]
endforeach

gtkdoc_common_scan_args = [
  '--ignore-decorators=' + '|'.join(ignore_decorators),
]

if get_option('gtk_doc')
  # Check we have the minimum gtk-doc version required. Older versions won't
  # generate correct documentation.
  dependency('gtk-doc', version : '>=1.32')
endif

subdir('gio')
subdir('glib')
subdir('gobject')