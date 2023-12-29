gir_dir_prefix = get_option('gir_dir_prefix')
if gir_dir_prefix == '' or gir_dir_prefix == get_option('datadir')
  gir_dir_prefix = get_option('datadir')
  gir_dir_pc_prefix = '${datadir}'
else
  gir_dir_pc_prefix = join_paths('${prefix}', gir_dir_prefix)
endif

glib_girdir = get_option('prefix') / gir_dir_prefix / 'gir-1.0'

gir_includedir = glib_includedir / 'girepository'

gir_c_args = [
  '-DGI_COMPILATION',
  '-DGIR_SUFFIX="gir-1.0"',
  '-DGIR_DIR="@0@"'.format(glib_girdir),
  '-DGOBJECT_INTROSPECTION_LIBDIR="@0@"'.format(glib_libdir),
  '-DGOBJECT_INTROSPECTION_DATADIR="@0@"'.format(glib_datadir),
]

custom_c_args = [
  '-DG_LOG_DOMAIN="GLib-GirCompiler"',  
]

if cc.get_id() != 'msvc'
  custom_c_args = cc.get_supported_arguments([
    '-Wno-old-style-definition',
    '-Wno-cast-align',
  ])
endif

if cc.get_id() != 'msvc'
  custom_c_args = cc.get_supported_arguments([
    '-Wno-unused-parameter',
    '-Wno-duplicated-branches',
    '-Wno-cast-align',
  ])
endif

gircompiler_internals = static_library('gircompiler-internals',
  sources: [
    'girmodule.c',
    'girnode.c',
    'giroffsets.c',
    'girparser.c',
  ],
  c_args: gir_c_args + custom_c_args,
  include_directories : [configinc, girepoinc],
  dependencies: [libglib_dep, libgmodule_dep, libgitypelib_dep, libffi_dep],
)

gircompiler_internals_dep = declare_dependency(
  link_with: gircompiler_internals,
  include_directories: [girepoinc],
)

gircompiler = executable('gir-compiler2', 'compiler.c',
  dependencies: [
    gircompiler_internals_dep,
    libgio_dep,
  ],
  install: true,
  c_args: custom_c_args,
)
meson.override_find_program('g-ir-compiler', gircompiler)
