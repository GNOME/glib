dbus_service_files = [
  'org.freedesktop.portal.Documents.service'
]

cdata = configuration_data()
if installed_tests_enabled
  cdata.set('installed_tests_dir', installed_tests_execdir)
else
  cdata.set('installed_tests_dir', meson.current_build_dir() / '..')
endif

foreach service_file : dbus_service_files
  if installed_tests_enabled
    configure_file(
      input: service_file + '.in',
      output: service_file,
      install_dir: installed_tests_execdir / 'services',
      configuration: cdata,
    )
  else
    configure_file(
      input: service_file + '.in',
      output: service_file,
      configuration: cdata,
    )
  endif
endforeach
