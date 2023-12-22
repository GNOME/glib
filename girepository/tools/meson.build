gircompiler = executable('g-ir-compiler2', 'compiler.c',
  dependencies: [
    libgirepository_internals_dep,
    libgirepository_dep,
    libgio_dep,
  ],
  install: true,
  c_args: custom_c_args,
)
meson.override_find_program('g-ir-compiler', gircompiler)
