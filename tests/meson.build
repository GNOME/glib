# tests

# Not entirely random of course, but at least it changes over time
random_number = minor_version + meson.version().split('.').get(1).to_int()

test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('G_DEBUG', 'gc-friendly')
test_env.set('MALLOC_CHECK_', '2')
test_env.set('MALLOC_PERTURB_', '@0@'.format(random_number % 256))

test_cargs = ['-DG_LOG_DOMAIN="GLib"', '-UG_DISABLE_ASSERT']

test_extra_programs = {
  'assert-msg-test' : {},
}

common_c_args = test_cargs + ['-DGLIB_DISABLE_DEPRECATION_WARNINGS']
common_deps = [libm, thread_dep, libglib_dep]

foreach program_name, extra_args : test_extra_programs
  source = extra_args.get('source', program_name + '.c')
  extra_sources = extra_args.get('extra_sources', [])
  install = installed_tests_enabled and extra_args.get('install', true)
  executable(program_name, [source, extra_sources],
      c_args : common_c_args,
      dependencies : common_deps + extra_args.get('dependencies', []),
      install_dir : installed_tests_execdir,
      install : install,
      win_subsystem : extra_args.get('win_subsystem', 'console'),
  )
endforeach
