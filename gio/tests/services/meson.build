dbus_service_files = [
  'org.freedesktop.portal.Documents.service',
  'org.gtk.GDBus.FakeService.service'
]

srcdir_cdata = configuration_data()
srcdir_cdata.set('installed_tests_dir', meson.current_build_dir() / '..')

installed_cdata = configuration_data()
installed_cdata.set('installed_tests_dir', installed_tests_execdir)

foreach service_file : dbus_service_files
  configure_file(
    input: service_file + '.in',
    output: service_file,
    configuration: srcdir_cdata,
  )
  if installed_tests_enabled
    # Build a second copy of the service file for the installed
    # version of the tests.
    configure_file(
      input: service_file + '.in',
      output: service_file + '.to-install',
      configuration: installed_cdata,
    )
    install_data(meson.current_build_dir() / service_file + '.to-install',
      install_dir: installed_tests_execdir / 'services',
      install_tag: 'tests',
      rename: [service_file],
    )
  endif
endforeach
