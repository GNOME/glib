giscanner_files = [
  '__init__.py',
  'annotationmain.py',
  'annotationparser.py',
  'cachestore.py',
  'ccompiler.py',
  'codegen.py',
  'dumper.py',
  'introspectablepass.py',
  'girast.py',
  'girparser.py',
  'girwriter.py',
  'gdumpparser.py',
  'maintransformer.py',
  'mdextensions.py',
  'message.py',
  'msvccompiler.py',
  'pkgconfig.py',
  'shlibs.py',
  'scannermain.py',
  'sectionparser.py',
  'sourcescanner.py',
  'testcodegen.py',
  'transformer.py',
  'utils.py',
  'xmlwriter.py',
]

pkglibdir = get_option('libdir') / meson.project_name()
giscannerdir = pkglibdir / 'giscanner'

giscanner_built_files = []
giscanner_conf_data = configuration_data()
giscanner_conf_data.set('VERSION', meson.project_version())
giscanner_built_files += configure_file(
  input: '_version.py.in',
  output: '_version.py',
  install_dir: giscannerdir,
  configuration: giscanner_conf_data,
)

foreach f: giscanner_files
  giscanner_built_files += configure_file(
    input: f,
    output: f,
    install_dir: giscannerdir,
    configuration: giscanner_conf_data,
  )
endforeach

# Copy to builddir for use with dumper.py when running uninstalled
gdump = fs.copyfile('../gdump.c', 'gdump.c')

flex = find_program('flex', 'win_flex')
bison = find_program('bison', 'win_bison')

scannerparser = custom_target('scannerparser',
    input: 'scannerparser.y',
   output: ['scannerparser.c', 'scannerparser.h'],
  command: [bison, '@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@']

)

scannerlexer = custom_target('scannerlexer',
    input: 'scannerlexer.l',
   output: 'scannerlexer.c',
  command: [flex, '-o', '@OUTPUT@', '@INPUT@']
)

giscanner_args = []
if not cc.has_header('unistd.h')
  giscanner_args += '-DYY_NO_UNISTD_H'
endif

giscanner_c_args = []
if cc.get_id() != 'msvc'
  giscanner_c_args = cc.get_supported_arguments([
    '-Wno-missing-field-initializers',
    '-Wno-unused-parameter',
    '-Wno-misleading-indentation',
  ])
endif

giscanner_lib = static_library('giscanner',
  sources: [
    'sourcescanner.c',
    scannerparser,
    scannerlexer,
  ],
  c_args: giscanner_args,
  include_directories: configinc,
  dependencies: [
    libglib_dep,
    libgobject_dep,
    libgio_dep,
    libgmodule_dep,
  ],
)

python_ext_dep = python.dependency()
if not python_ext_dep.found()
  # For example if Python is 32bit but we require a 64bit variant
  error('Python installation not useable')
endif

giscanner_pymod = python.extension_module('_giscanner', ['giscannermodule.c'],
  link_with: giscanner_lib,
  c_args: giscanner_c_args,
  include_directories: configinc,
  dependencies: [
    libglib_dep,
    libgobject_dep,
    libgio_dep,
    libgmodule_dep,
    python_ext_dep,
  ],
  gnu_symbol_visibility: 'hidden',
  install: true,
  install_dir: giscannerdir,
)

libdir_abs = get_option('prefix') / get_option('libdir')
datadir_abs = get_option('prefix') / get_option('datadir')

if not python.full_path().startswith('/usr/bin')
  python_cmd = '/usr/bin/env ' + python.full_path()
else
  python_cmd = '/usr/bin/env python@0@'.format(python.language_version().split('.')[0])
endif

giscanner_conf = configuration_data()
giscanner_conf.set('libdir', get_option('libdir'))
giscanner_conf.set('libdir_abs', libdir_abs)
giscanner_conf.set('datarootdir_abs', datadir_abs)
giscanner_conf.set('gir_dir_prefix', gir_dir_prefix)
giscanner_conf.set('GIR_DIR', glib_girdir)
giscanner_conf.set('PYTHON_CMD', python_cmd)

giscanner_bin = configure_file(
  input: 'gi-generate-repository.in',
  output: 'gi-generate-repository',
  configuration: giscanner_conf,
  install: true,
  install_dir: get_option('bindir'),
)

# Only override the user-installed compiler if we need to generate the GIRs
# GLib, Gio, and GObject...
if enable_gir
  # Replace the default g-ir-scanner target with the version we
  # just built.
  meson.override_find_program('g-ir-scanner', giscanner_bin)
endif
