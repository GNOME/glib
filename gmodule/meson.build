gmoduleconf_conf = configuration_data()

g_module_need_uscore = false
g_module_broken_rtld_global = false
g_module_have_dlerror = false

libdl_dep = [ ]
g_module_lib_args = [ ]
g_module_impl = 'G_MODULE_IMPL_NONE'
g_module_supported = false

# Check whether we need preceding underscores
g_module_need_uscore = cc.symbols_have_underscore_prefix()

# On Windows force native WIN32 shared lib loader
if host_system == 'windows'
  g_module_impl = 'G_MODULE_IMPL_WIN32'
# Force native AIX library loader
# dlopen() filepath must be of the form /path/libname.a(libname.so)
elif host_system == 'aix'
  g_module_impl = 'G_MODULE_IMPL_AR'
elif cc.has_function('dlopen', prefix : '#include <dlfcn.h>')
  g_module_impl = 'G_MODULE_IMPL_DL'
# NSLinkModule (dyld) in system libraries (Darwin)
elif cc.has_function('NSLinkModule')
  g_module_impl = 'G_MODULE_IMPL_DYLD'
elif cc.has_function('dlopen', prefix : '#include <dlfcn.h>', args : '-ldl')
  g_module_impl = 'G_MODULE_IMPL_DL'
  libdl_dep = cc.find_library('dl')
  g_module_lib_args = '-ldl'
endif

# additional checks for G_MODULE_IMPL_DL
# FIXME: check for OSF1/5.0 RTLD_GLOBAL brokenness (is this still relevant?)
if g_module_impl == 'G_MODULE_IMPL_DL' and cc.has_function('dlerror', args : g_module_lib_args)
  g_module_have_dlerror = true
endif

# Done, have we got an implementation?
if g_module_impl == 'G_MODULE_IMPL_NONE'
  message('WARNING: No suitable GModule implementation found!')
else
  g_module_supported = true
endif

gmoduleconf_conf.set('G_MODULE_IMPL', g_module_impl)
gmoduleconf_conf.set10('G_MODULE_SUPPORTED', g_module_supported)
gmoduleconf_conf.set10('G_MODULE_HAVE_DLERROR', g_module_have_dlerror)
gmoduleconf_conf.set10('G_MODULE_NEED_USCORE', g_module_need_uscore)
gmoduleconf_conf.set10('G_MODULE_BROKEN_RTLD_GLOBAL', g_module_broken_rtld_global)

gmoduleconf_h = configure_file(input : 'gmoduleconf.h.in',
                               output : 'gmoduleconf.h',
                               configuration : gmoduleconf_conf)

install_headers(['gmodule.h'], subdir : 'glib-2.0')

gmodule_sources = ['gmodule.c']
if host_system == 'windows'
  gmodule_win_rc = configure_file(
    input: 'gmodule.rc.in',
    output: 'gmodule.rc',
    configuration: glibconfig_conf,
  )
  gmodule_win_res = windows.compile_resources(gmodule_win_rc)
  gmodule_sources += [gmodule_win_res]
endif

libgmodule = library('gmodule-2.0',
  sources : gmodule_sources,
  version : library_version,
  soversion : soversion,
  darwin_versions : darwin_versions,
  install : true,
  include_directories : [configinc, gmoduleinc],
  dependencies : [libdl_dep, libglib_dep],
  c_args : ['-DG_LOG_DOMAIN="GModule"'] + glib_hidden_visibility_args,
  link_args : [glib_link_flags],
)

supported_var = 'gmodule_supported=@0@'.format(g_module_supported)

pkg.generate(libgmodule,
  libraries : [thread_dep],
  requires : ['glib-2.0'],
  version : glib_version,
  variables : [supported_var],
  install_dir : glib_pkgconfigreldir,
  filebase : 'gmodule-no-export-2.0',
  name : 'GModule',
  description : 'Dynamic module loader for GLib',
)

pkg.generate(libraries : [libgmodule, export_dynamic_ldflags],
  requires : ['glib-2.0'],
  version : glib_version,
  variables : [supported_var],
  install_dir : glib_pkgconfigreldir,
  filebase : 'gmodule-export-2.0',
  name : 'GModule',
  description : 'Dynamic module loader for GLib',
)

pkg.generate(libraries : [libgmodule, export_dynamic_ldflags],
  requires : ['glib-2.0'],
  version : glib_version,
  variables : [supported_var],
  install_dir : glib_pkgconfigreldir,
  filebase : 'gmodule-2.0',
  name : 'GModule',
  description : 'Dynamic module loader for GLib',
)

libgmodule_dep = declare_dependency(link_with : libgmodule,
  include_directories : [gmoduleinc],
  dependencies : [libglib_dep])
