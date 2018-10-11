m4macros_tests = {
  'am-path-glib.sh' : {},
}

if have_sh
  test_env = environment()
  test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
  test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())

  foreach test_name, extra_args : m4macros_tests
    if installed_tests_enabled
      install_data(test_name, install_dir : installed_tests_execdir)

      test_conf = configuration_data()
      test_conf.set('installed_tests_dir', installed_tests_execdir)
      test_conf.set('program', test_name)
      configure_file(
        input: installed_tests_template_tap,
        output: test_name + '.test',
        install_dir: installed_tests_metadir,
        configuration: test_conf
      )
    endif

    test(test_name,
      sh,
      args : files('am-path-glib.sh'),
      env : test_env,
      suite : ['m4macros'],
    )
  endforeach
endif
