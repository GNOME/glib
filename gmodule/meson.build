gmoduleconf_conf = configuration_data()

g_module_need_uscore = 0
g_module_broken_rtld_global = 0
g_module_have_dlerror = 0

libdl_dep = [ ]
g_module_lib_args = [ ]
g_module_impl = ''

dlopen_dlsym_test_code = '''
#include <dlfcn.h>
int glib_underscore_test (void) { return 42; }
int main (int argc, char ** argv) {
  void *f1 = (void*)0, *f2 = (void*)0, *handle;
  handle = dlopen ((void*)0, 0);
  if (handle) {
    f1 = dlsym (handle, "glib_underscore_test");
    f2 = dlsym (handle, "_glib_underscore_test");
  }
  return (!f2 || f1);
}'''

# On Windows force native WIN32 shared lib loader
if host_machine.system() == 'windows'
  g_module_impl = 'G_MODULE_IMPL_WIN32'
# Force native AIX library loader
# dlopen() filepath must be of the form /path/libname.a(libname.so)
elif host_machine.system() == 'aix'
  g_module_impl = 'G_MODULE_IMPL_AR'
elif cc.links(dlopen_dlsym_test_code, name : 'dlopen() and dlsym() in system libraries')
  g_module_impl = 'G_MODULE_IMPL_DL'
# NSLinkModule (dyld) in system libraries (Darwin)
elif cc.has_function('NSLinkModule', prefix : '#include <mach-o/dyld.h>', name : 'NSLinkModule')
  g_module_impl = 'G_MODULE_IMPL_DYLD'
  g_module_need_uscore = 1
elif cc.links(dlopen_dlsym_test_code, args : '-ldl', name : 'dlopen() and dlsym() in libdl')
  g_module_impl = 'G_MODULE_IMPL_DL'
  libdl_dep = find_library('dl')
  g_module_lib_args = '-ldl'
endif

# additional checks for G_MODULE_IMPL_DL
if g_module_impl == 'G_MODULE_IMPL_DL'
  # FIXME: check for OSF1/5.0 RTLD_GLOBAL brokenness (is this still relevant?)

  # Check whether we need preceding underscores
  if not meson.is_cross_build()
    # FIXME: communicate result via stdout instead of return value, so non-0 return is not printed in bold red
    rres = cc.run(dlopen_dlsym_test_code,
                  args : g_module_lib_args,
                  name : 'dlsym() preceding underscores')
    if rres.compiled() and rres.returncode() == 0
      g_module_need_uscore = 1
    endif
  endif

  if cc.has_function('dlerror', prefix : '#include <dlfcn.h>', args : g_module_lib_args, name : 'dlerror')
    g_module_have_dlerror = 1
  endif
endif

# Done, have we got an implementation?
if g_module_impl == ''
  g_module_impl = '0'
  message('WARNING: No suitable GModule implementation found!')
endif

gmoduleconf_conf.set('G_MODULE_IMPL', g_module_impl)
gmoduleconf_conf.set('G_MODULE_SUPPORTED', g_module_impl != '0')
gmoduleconf_conf.set('G_MODULE_HAVE_DLERROR', g_module_have_dlerror)
gmoduleconf_conf.set('G_MODULE_NEED_USCORE', g_module_need_uscore)
gmoduleconf_conf.set('G_MODULE_BROKEN_RTLD_GLOBAL', g_module_broken_rtld_global)

gmoduleconf_h = configure_file(input : 'gmoduleconf.h.in',
                               output : 'gmoduleconf.h',
                               configuration : gmoduleconf_conf)

install_headers([ 'gmodule.h' ], subdir : 'glib-2.0/')

libgmodule = shared_library('gmodule',
  sources : [ 'gmodule.c' ],
  version : glib_version,
  soversion : interface_version,
  install : true,
  include_directories : inc_dirs,
  link_with : libglib,
  dependencies : libdl_dep,
  c_args : ['-DG_LOG_DOMAIN="GModule"', '-DG_DISABLE_DEPRECATED' ],
)
