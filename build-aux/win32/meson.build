
if host_system == 'windows'
  static_lib = static_library('os-profile-app-static-lib',
                              sources: ['app.c'],
                              include_directories: [configinc])

  resources = windows.compile_resources('app.rc',
                                        depend_files: ['app.manifest.xml'])

  app_profile_dep = declare_dependency(link_whole: [static_lib],
                                       sources: [resources])
else
  app_profile_dep = declare_dependency()
endif

rc_conf_base = configuration_data({
  'MAJOR': major_version,
  'MINOR': minor_version,
  'MICRO': micro_version,
  'COPYRIGHTYEAR': '2026',
})

lib_rc_in = files('lib.rc.in')
