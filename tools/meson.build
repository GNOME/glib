# Install glib-gettextize executable, if a UNIX-style shell is found
if have_sh
  # These should not contain " quotes around the values
  gettextize_conf = configuration_data()
  gettextize_conf.set('PACKAGE', 'glib')
  gettextize_conf.set('VERSION', meson.project_version())
  gettextize_conf.set('prefix', glib_prefix)
  gettextize_conf.set('datarootdir', glib_datadir)
  gettextize_conf.set('datadir', glib_datadir)
  configure_file(input : 'glib-gettextize.in',
    install_dir : glib_bindir,
    output : 'glib-gettextize',
    configuration : gettextize_conf)
endif
