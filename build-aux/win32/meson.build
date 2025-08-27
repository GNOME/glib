
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
