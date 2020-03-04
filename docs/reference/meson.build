# The list of minor versions in the 2.x.x series which have had
# GLIB_AVAILABLE_IN_* macros. This should include the current unreleased stable
# version.
#
# FIXME: It would be good to be able to generate this list:
# https://github.com/mesonbuild/meson/issues/5026
stable_2_series_versions = [
  '26', '28', '30', '32', '34', '36', '38',
  '40', '42', '44', '46', '48', '50', '52', '54', '56', '58',
  '60', '62', '64', '66',
]

ignore_decorators = [
  'GLIB_VAR',
  'G_GNUC_INTERNAL',
  'G_GNUC_WARN_UNUSED_RESULT',
  'GLIB_AVAILABLE_IN_ALL',
]

foreach version : stable_2_series_versions
  ignore_decorators += [
    # Note that gtkdoc is going to use those in regex, and the longest match
    # must come first. That's why '_FOR()' variant comes first.
    # gtkdoc special-case '()' and replace it by a regex matching a symbol name.
    'GLIB_AVAILABLE_IN_2_' + version,
    'GLIB_DEPRECATED_IN_2_' + version + '_FOR()',
    'GLIB_DEPRECATED_IN_2_' + version,

    'GLIB_AVAILABLE_ENUMERATOR_IN_2_' + version,
    'GLIB_DEPRECATED_ENUMERATOR_IN_2_' + version + '_FOR()',
    'GLIB_DEPRECATED_ENUMERATOR_IN_2_' + version,

    'GLIB_AVAILABLE_MACRO_IN_2_' + version,
    'GLIB_DEPRECATED_MACRO_IN_2_' + version + '_FOR()',
    'GLIB_DEPRECATED_MACRO_IN_2_' + version,

    'GLIB_AVAILABLE_TYPE_IN_2_' + version,
    'GLIB_DEPRECATED_TYPE_IN_2_' + version + '_FOR()',
    'GLIB_DEPRECATED_TYPE_IN_2_' + version,
  ]
endforeach

gtkdoc_common_scan_args = [
  '--ignore-decorators=' + '|'.join(ignore_decorators),
]

if get_option('gtk_doc')
  if not meson.version().version_compare('>=0.52.0')
    error('Building documentation requires Meson >= 0.52.0.')
  endif
  # Check we have the minimum gtk-doc version required. Older versions won't
  # generate correct documentation.
  dependency('gtk-doc', version : '>=1.32',
    fallback : ['gtk-doc', 'dummy_dep'],
    default_options : ['tests=false'])
endif

subdir('gio')
subdir('glib')
subdir('gobject')