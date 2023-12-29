# SPDX-License-Identifier: LGPL-2.1-or-later
# SPDX-FileCopyrightText: 2017 Patrick Griffis
# SPDX-FileCopyrightText: 2017 Danny Forghieri
# SPDX-FileCopyrightText: 2017 Nirbheek Chauhan
# SPDX-FileCopyrightText: 2017, 2021, 2022 Emmanuele Bassi
# SPDX-FileCopyrightText: 2018 Mathieu Duponchelle
# SPDX-FileCopyrightText: 2018, 2019, 2020 Christoph Reiter
# SPDX-FileCopyrightText: 2018 Kai Kang
# SPDX-FileCopyrightText: 2018 Carlos Garnacho
# SPDX-FileCopyrightText: 2018, 2019, 2020 Alexander Kanavin
# SPDX-FileCopyrightText: 2019, 2020 Chun-wei Fan
# SPDX-FileCopyrightText: 2019 Aaron Boxer
# SPDX-FileCopyrightText: 2019 Thibault Saunier
# SPDX-FileCopyrightText: 2019 Joshua Watt
# SPDX-FileCopyrightText: 2020 Xavier Claessens
# SPDX-FileCopyrightText: 2020 Philip Chimento
# SPDX-FileCopyrightText: 2021 John Ericson
# SPDX-FileCopyrightText: 2021 Cimbali
# SPDX-FileCopyrightText: 2021, 2023 Simon McVittie
# SPDX-FileCopyrightText: 2022 Andoni Morales Alastruey

subdir('cmph')

gir_dir_prefix = get_option('gir_dir_prefix')
if gir_dir_prefix == '' or gir_dir_prefix == get_option('datadir')
  gir_dir_prefix = get_option('datadir')
  gir_dir_pc_prefix = '${datadir}'
else
  gir_dir_pc_prefix = join_paths('${prefix}', gir_dir_prefix)
endif

glib_girdir = get_option('prefix') / gir_dir_prefix / 'gir-1.0'

gir_includedir = glib_includedir / 'gitypelib'

gi_visibility_h = custom_target(
  output: 'gi-visibility.h',
  command: [gen_visibility_macros, meson.project_version(), 'visibility-macros', 'GI', '@OUTPUT@'],
  install: true,
  install_dir: gir_includedir,
  install_tag: 'devel',
)

gitypelib_headers = files(
  'gitypelib.h',
)
gitypelib_private_headers = files(
  'gitypelib-private.h',
)
install_headers(gitypelib_headers + gitypelib_private_headers, install_dir: gir_includedir)

gir_c_args = [
  '-DGI_COMPILATION',
  '-DG_LOG_DOMAIN="GLib-GITypelib"',
  '-DGIR_SUFFIX="gir-1.0"',
  '-DGIR_DIR="@0@"'.format(glib_girdir),
  '-DGOBJECT_INTROSPECTION_LIBDIR="@0@"'.format(glib_libdir),
  '-DGOBJECT_INTROSPECTION_DATADIR="@0@"'.format(glib_datadir),
]

custom_c_args = []

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

gitypelib_gthash_lib = static_library('girepository-gthash',
  sources: ['gthash.c', gi_visibility_h],
  include_directories : [configinc],
  c_args: gir_c_args + custom_c_args,
  dependencies: [
    cmph_dep,
    libglib_dep,
    libgmodule_dep,
    libgobject_dep,
  ],
)

gitypelib_gthash_dep = declare_dependency(
  link_with: gitypelib_gthash_lib,
  dependencies: [libglib_dep, libgmodule_dep, libgobject_dep],
  include_directories: [],
)

gitypelib_sources = files(
  'gitypelib.c',
)

if cc.get_id() != 'msvc'
  custom_c_args = cc.get_supported_arguments([
    '-Wno-unused-parameter',
    '-Wno-duplicated-branches',
    '-Wno-type-limits',
    '-Wno-cast-align',
    '-Wno-missing-field-initializers',
  ])
endif

libgitypelib = shared_library('gitypelib-2.0',
  sources: gitypelib_sources + [gi_visibility_h],
  include_directories: [configinc],
  c_args: gir_c_args,
  version: library_version,
  soversion: soversion,
  darwin_versions: darwin_versions,
  gnu_symbol_visibility: 'hidden',
  link_args: glib_link_flags,
  dependencies: [
    libglib_dep,
    libgobject_dep,
    libgmodule_dep,
    libgio_dep,
    gitypelib_gthash_dep,
  ],
  install: true,
)

libgitypelib_dep = declare_dependency(
  link_with: libgitypelib,
  dependencies: [libglib_dep, libgobject_dep, libgio_dep, libgmodule_dep],
  sources: [gi_visibility_h],
  include_directories: [],
)

pkgconfig_variables = [
  'gidatadir=${datadir}/gobject-introspection-1.0',
  'girdir=' + gir_dir_pc_prefix / 'gir-1.0',
  'typelibdir=${libdir}/girepository-1.0',
]

pkg.generate(libgitypelib,
  name: 'gitypelib',
  version: glib_version,
  filebase: 'gitypelib-2.0',
  install_dir: glib_pkgconfigreldir,
  description: 'GObject Introspection repository parser',
  variables: pkgconfig_variables,
  libraries: [libglib_dep, libgobject_dep],
)
