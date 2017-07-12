gio_c_args = [
  '-DG_LOG_DOMAIN="GLib-GIO"',
  '-DGIO_COMPILATION',
  '-DGIO_MODULE_DIR="@0@"'.format(glib_giomodulesdir),
]

gio_c_args += glib_hidden_visibility_args

# FIXME: Install empty glib_giomodulesdir

gnetworking_h_conf = configuration_data()

gnetworking_h_wspiapi_include = ''
gnetworking_h_nameser_compat_include = ''

if host_system == 'windows'
  # <wspiapi.h> in the Windows SDK and in mingw-w64 has wrappers for
  # inline workarounds for getaddrinfo, getnameinfo and freeaddrinfo if
  # they aren't present at run-time (on Windows 2000).
  gnetworking_h_wspiapi_include = '#include <wspiapi.h>'
endif

if host_system.contains('android')
  # Android does not have C_IN in public headers, we define it wherever necessary
  if not cc.compiles('''#include <sys/types.h>
                        #include <arpa/nameser.h>
                        int qclass = C_IN;''',
                     name : 'C_IN in public headers (no arpa/nameser_compat.h needed)')
    if cc.compiles('''#include <sys/types.h>
                      #include <arpa/nameser.h>
                      #include <arpa/nameser_compat.h>
                      int qclass = C_IN;''',
                   name : 'arpa/nameser_compat.h needed for C_IN')
      gnetworking_h_nameser_compat_include = '#include <arpa/nameser_compat.h>'
    else
      error('Could not find required includes for ARPA C_IN')
    endif
  endif
endif

network_libs = [ ]
network_args = [ ]
if host_system != 'windows'
  # res_query()
  res_query_test = '''#include <resolv.h>
                      int main (int argc, char ** argv) {
                        return res_query("test", 0, 0, (void *)0, 0);
                      }'''
  res_query_test_full = '''#include <sys/types.h>
                           #include <netinet/in.h>
                           #include <arpa/nameser.h>
                        ''' + res_query_test
  if not cc.links(res_query_test_full, name : 'res_query()')
    if cc.links(res_query_test_full, args : '-lresolv', name : 'res_query() in -lresolv')
      network_libs += [ cc.find_library('resolv') ]
      network_args += [ '-lresolv' ]
    elif cc.links(res_query_test, args : '-lbind', name : 'res_query() in -lbind')
      network_libs += [ cc.find_library('bind') ]
      network_args += [ '-lbind' ]
    else
      error('Could not find res_query()')
    endif
  endif

  # socket()
  socket_test = '''#include <sys/types.h>
                   #include <sys/socket.h>
                   int main (int argc, char ** argv) {
                     return socket(1, 2, 3);
                   }'''
  if not cc.links(socket_test, name : 'socket()')
    if cc.links(socket_test, args : '-lsocket', name : 'socket() in -lsocket')
      network_libs += [ cc.find_library('socket') ]
      network_args += [ '-lsocket' ]
    else
      error('Could not find socket()')
    endif
  endif

  # res_init()
  if cc.links('''#include <sys/types.h>
                 #include <netinet/in.h>
                 #include <arpa/nameser.h>
                 #include <resolv.h>
                 int main (int argc, char ** argv) {
                   return res_init();
                 }''', args : network_args, name : 'res_init()')
    glib_conf.set('HAVE_RES_INIT', 1)
  endif

  if cc.compiles('''#include <netinet/in.h>
                    struct ip_mreqn foo;''',
                 name : 'struct ip_mreqn')
    glib_conf.set('HAVE_IP_MREQN', '/**/')
  endif
endif

network_args_string = ''
foreach arg : network_args
  network_args_string += arg + ' '
endforeach
glib_conf.set('NETWORK_LIBS', network_args_string)

gnetworking_h_conf.set('WSPIAPI_INCLUDE', gnetworking_h_wspiapi_include)
gnetworking_h_conf.set('NAMESER_COMPAT_INCLUDE', gnetworking_h_nameser_compat_include)

gnetworking_h = configure_file(input : 'gnetworking.h.in',
                               output : 'gnetworking.h',
                               install_dir : join_paths(get_option('includedir'), 'glib-2.0/gio'),
                               configuration : gnetworking_h_conf)

gdbus_headers = files([
  'gdbusauthobserver.h',
  'gcredentials.h',
  'gdbusutils.h',
  'gdbuserror.h',
  'gdbusaddress.h',
  'gdbusconnection.h',
  'gdbusmessage.h',
  'gdbusnameowning.h',
  'gdbusnamewatching.h',
  'gdbusproxy.h',
  'gdbusintrospection.h',
  'gdbusmethodinvocation.h',
  'gdbusserver.h',
  'gdbusinterface.h',
  'gdbusinterfaceskeleton.h',
  'gdbusobject.h',
  'gdbusobjectskeleton.h',
  'gdbusobjectproxy.h',
  'gdbusobjectmanager.h',
  'gdbusobjectmanagerclient.h',
  'gdbusobjectmanagerserver.h',
  'gtestdbus.h',
])

gdbus_sources = [
  'gdbusutils.c',
  'gdbusaddress.c',
  'gdbusauthobserver.c',
  'gdbusauth.c',
  'gdbusauthmechanism.c',
  'gdbusauthmechanismanon.c',
  'gdbusauthmechanismexternal.c',
  'gdbusauthmechanismsha1.c',
  'gdbuserror.c',
  'gdbusconnection.c',
  'gdbusmessage.c',
  'gdbusnameowning.c',
  'gdbusnamewatching.c',
  'gdbusproxy.c',
  'gdbusprivate.c',
  'gdbusintrospection.c',
  'gdbusmethodinvocation.c',
  'gdbusserver.c',
  'gdbusinterface.c',
  'gdbusinterfaceskeleton.c',
  'gdbusobject.c',
  'gdbusobjectskeleton.c',
  'gdbusobjectproxy.c',
  'gdbusobjectmanager.c',
  'gdbusobjectmanagerclient.c',
  'gdbusobjectmanagerserver.c',
  'gtestdbus.c',
]

# Generate gdbus-codegen
subdir('gdbus-2.0/codegen')

# Generate xdp-dbus.{c,h}
xdp_dbus_generated = custom_target('xdp-dbus',
    input : ['org.freedesktop.portal.Documents.xml',
             'org.freedesktop.portal.NetworkMonitor.xml',
             'org.freedesktop.portal.ProxyResolver.xml'],
    output : ['xdp-dbus.h', 'xdp-dbus.c'],
    command : [python, gdbus_codegen,
               '--interface-prefix', 'org.freedesktop.portal.',
               '--output-directory', '@OUTDIR@',
               '--generate-c-code', 'xdp-dbus',
               '--c-namespace', 'GXdp',
               '--annotate', 'org.freedesktop.portal.Documents.Add()',
                             'org.gtk.GDBus.C.UnixFD', 'true',
               '--annotate', 'org.freedesktop.portal.Documents.AddNamed()',
                             'org.gtk.GDBus.C.UnixFD', 'true',
               '@INPUT@'])

# Generate gdbus-generated.{c,h}
gdbus_daemon_generated = custom_target('gdbus-daemon-generated',
    input : ['dbus-daemon.xml'],
    output : ['gdbus-daemon-generated.h', 'gdbus-daemon-generated.c'],
    command : [python, gdbus_codegen,
               '--interface-prefix', 'org.',
               '--output-directory', '@OUTDIR@',
               '--generate-c-code', 'gdbus-daemon-generated',
               '--c-namespace', '_G', '@INPUT@'])

settings_headers = files([
  'gsettingsbackend.h',
  'gsettingsschema.h',
  'gsettings.h',
])

settings_sources = [
  'gvdb/gvdb-reader.c',
  'gdelayedsettingsbackend.c',
  'gkeyfilesettingsbackend.c',
  'gmemorysettingsbackend.c',
  'gnullsettingsbackend.c',
  'gsettingsbackend.c',
  'gsettingsschema.c',
  'gsettings-mapping.c',
  'gsettings.c',
]

if host_system == 'windows'
  settings_sources += [ 'gregistrysettingsbackend.c' ]
endif

# FIXME:
#if OS_COCOA
#settings_sources += [ 'gnextstepsettingsbackend.c' ]
#endif

application_headers = files([
  'gapplication.h',
  'gapplicationcommandline.h',

  'gactiongroup.h',
  'gactionmap.h',
  'gsimpleactiongroup.h',
  'gremoteactiongroup.h',
  'gactiongroupexporter.h',
  'gdbusactiongroup.h',
  'gaction.h',
  'gpropertyaction.h',
  'gsimpleaction.h',

  'gmenumodel.h',
  'gmenu.h',
  'gmenuexporter.h',
  'gdbusmenumodel.h',
  'gnotification.h',
])

application_sources = [
  'gapplication.c',
  'gapplicationcommandline.c',
  'gapplicationimpl-dbus.c',

  'gactiongroup.c',
  'gactionmap.c',
  'gsimpleactiongroup.c',
  'gremoteactiongroup.c',
  'gactiongroupexporter.c',
  'gdbusactiongroup.c',
  'gaction.c',
  'gpropertyaction.c',
  'gsimpleaction.c',

  'gmenumodel.c',
  'gmenu.c',
  'gmenuexporter.c',
  'gdbusmenumodel.c',
  'gnotification.c',
  'gnotificationbackend.c',
]

local_sources = [
  'ghttpproxy.c',
  'glocalfile.c',
  'glocalfileenumerator.c',
  'glocalfileinfo.c',
  'glocalfileinputstream.c',
  'glocalfilemonitor.c',
  'glocalfileoutputstream.c',
  'glocalfileiostream.c',
  'glocalvfs.c',
  'gsocks4proxy.c',
  'gsocks4aproxy.c',
  'gsocks5proxy.c',
  'thumbnail-verify.c',
]

platform_deps = []
internal_deps = []
appinfo_sources = []
contenttype_sources = []
portal_sources = []
unix_sources = []

if host_system != 'windows'
  unix_sources = [
    'gfiledescriptorbased.c',
    'gunixconnection.c',
    'gunixcredentialsmessage.c',
    'gunixfdlist.c',
    'gunixfdmessage.c',
    'gunixmount.c',
    'gunixmounts.c',
    'gunixsocketaddress.c',
    'gunixvolume.c',
    'gunixvolumemonitor.c',
    'gunixinputstream.c',
    'gunixoutputstream.c',
    'gfdonotificationbackend.c',
    'ggtknotificationbackend.c',
  ]

  portal_sources = [
    'gdocumentportal.c',
    'gnetworkmonitorportal.c',
    'gproxyresolverportal.c',
    'gportalsupport.c',
    'gportalnotificationbackend.c',
    xdp_dbus_generated
  ]

  gio_unix_include_headers = [
    'gfiledescriptorbased.h',
    'gunixconnection.h',
    'gunixcredentialsmessage.h',
    'gunixmounts.h',
    'gunixfdlist.h',
    'gunixfdmessage.h',
    'gunixinputstream.h',
    'gunixoutputstream.h',
    'gunixsocketaddress.h',
  ]

  if glib_have_cocoa
    contenttype_sources += ['gosxcontenttype.c']
    appinfo_sources += ['gosxappinfo.c']
    if glib_have_os_x_9_or_later
      unix_sources += ['gcocoanotificationbackend.c']
    endif
  else
    contenttype_sources += ['gcontenttype.c']
    appinfo_sources += ['gdesktopappinfo.c']
    gio_unix_include_headers += ['gdesktopappinfo.h']
    subdir('xdgmime')
    internal_deps += [xdgmime_lib]
  endif

  install_headers(gio_unix_include_headers, subdir : 'gio-unix-2.0/gio')

  if glib_conf.has('HAVE_NETLINK')
    unix_sources += [
      'gnetworkmonitornetlink.c',
      'gnetworkmonitornm.c',
    ]
  endif
endif

# This is also used by tests/gdbus-daemon, so use files() to include the path
gdbus_daemon_sources = [
  files('gdbusdaemon.c'),
  gdbus_daemon_generated,
]

win32_sources = gdbus_daemon_sources
if host_system == 'windows'
  appinfo_sources += ['gwin32appinfo.c']
  contenttype_sources += ['gcontenttype-win32.c']
  platform_deps += [cc.find_library('shlwapi'),
                    cc.find_library('dnsapi'),
                    cc.find_library('iphlpapi'),
                    winsock2]
  win32_sources += [
    'gwin32registrykey.c',
    'gwin32mount.c',
    'gwin32volumemonitor.c',
    'gwin32inputstream.c',
    'gwin32outputstream.c',
  ]

  gio_win32_include_headers = [
    'gwin32inputstream.h',
    'gwin32outputstream.h',
  ]
  install_headers(gio_win32_include_headers, subdir : 'gio-win32-2.0/gio')
endif

gio_sources = [
  'gappinfo.c',
  'gasynchelper.c',
  'gasyncinitable.c',
  'gasyncresult.c',
  'gbufferedinputstream.c',
  'gbufferedoutputstream.c',
  'gbytesicon.c',
  'gcancellable.c',
  'gcharsetconverter.c',
  'gcontextspecificgroup.c',
  'gconverter.c',
  'gconverterinputstream.c',
  'gconverteroutputstream.c',
  'gcredentials.c',
  'gdatagrambased.c',
  'gdatainputstream.c',
  'gdataoutputstream.c',
  'gdrive.c',
  'gdummyfile.c',
  'gdummyproxyresolver.c',
  'gdummytlsbackend.c',
  'gemblem.c',
  'gemblemedicon.c',
  'gfile.c',
  'gfileattribute.c',
  'gfileenumerator.c',
  'gfileicon.c',
  'gfileinfo.c',
  'gfileinputstream.c',
  'gfilemonitor.c',
  'gfilenamecompleter.c',
  'gfileoutputstream.c',
  'gfileiostream.c',
  'gfilterinputstream.c',
  'gfilteroutputstream.c',
  'gicon.c',
  'ginetaddress.c',
  'ginetaddressmask.c',
  'ginetsocketaddress.c',
  'ginitable.c',
  'ginputstream.c',
  'gioerror.c',
  'giomodule.c',
  'gioscheduler.c',
  'giostream.c',
  'gloadableicon.c',
  'gmount.c',
  'gmemoryinputstream.c',
  'gmemoryoutputstream.c',
  'gmountoperation.c',
  'gnativevolumemonitor.c',
  'gnativesocketaddress.c',
  'gnetworkaddress.c',
  'gnetworking.c',
  'gnetworkmonitor.c',
  'gnetworkmonitorbase.c',
  'gnetworkservice.c',
  'goutputstream.c',
  'gpermission.c',
  'gpollableinputstream.c',
  'gpollableoutputstream.c',
  'gpollableutils.c',
  'gpollfilemonitor.c',
  'gproxy.c',
  'gproxyaddress.c',
  'gproxyaddressenumerator.c',
  'gproxyresolver.c',
  'gresolver.c',
  'gresource.c',
  'gresourcefile.c',
  'gseekable.c',
  'gsimpleasyncresult.c',
  'gsimpleiostream.c',
  'gsimplepermission.c',
  'gsocket.c',
  'gsocketaddress.c',
  'gsocketaddressenumerator.c',
  'gsocketclient.c',
  'gsocketconnectable.c',
  'gsocketconnection.c',
  'gsocketcontrolmessage.c',
  'gsocketinputstream.c',
  'gsocketlistener.c',
  'gsocketoutputstream.c',
  'gsubprocesslauncher.c',
  'gsubprocess.c',
  'gsocketservice.c',
  'gsrvtarget.c',
  'gsimpleproxyresolver.c',
  'gtask.c',
  'gtcpconnection.c',
  'gtcpwrapperconnection.c',
  'gthreadedsocketservice.c',
  'gthemedicon.c',
  'gthreadedresolver.c',
  'gthreadedresolver.h',
  'gtlsbackend.c',
  'gtlscertificate.c',
  'gtlsclientconnection.c',
  'gtlsconnection.c',
  'gtlsdatabase.c',
  'gtlsfiledatabase.c',
  'gtlsinteraction.c',
  'gtlspassword.c',
  'gtlsserverconnection.c',
  'gdtlsconnection.c',
  'gdtlsclientconnection.c',
  'gdtlsserverconnection.c',
  'gunionvolumemonitor.c',
  'gvfs.c',
  'gvolume.c',
  'gvolumemonitor.c',
  'gzlibcompressor.c',
  'gzlibdecompressor.c',
  'glistmodel.c',
  'gliststore.c',
]

gio_sources += appinfo_sources
gio_sources += contenttype_sources
gio_sources += unix_sources
gio_sources += win32_sources
gio_sources += application_sources
gio_sources += settings_sources
gio_sources += gdbus_sources
gio_sources += portal_sources
gio_sources += local_sources

MISSING_STUFF = '''
if OS_WIN32_AND_DLL_COMPILATION
gio_win32_res = gio-win32-res.o
gio_win32_res_ldflag = -Wl,$(gio_win32_res)
endif

gio-win32-res.o: gio.rc
  '$(WINDRES) gio.rc $@

# This is read by gobject-introspection/misc/ and gtk-doc
gio-public-headers.txt: Makefile
  '$(AM_V_GEN) echo $(gioinclude_HEADERS) $(giowin32include_HEADERS) $(giounixinclude_HEADERS) > $@.tmp && mv $@.tmp $@

gio.def: libgio-2.0.la
  '$(AM_V_GEN) dumpbin.exe -exports .libs/libgio-2.0-0.dll | awk 'BEGIN { print "EXPORTS" } / +[[:digit:]]+ +[[:xdigit:]]+ +[[:xdigit:]]+/{ print $$4 }' > gio.def.tmp && mv gio.def.tmp gio.def

gio-2.0.lib: libgio-2.0.la gio.def
  '$(AM_V_GEN) lib.exe -machine:@LIB_EXE_MACHINE_FLAG@ -name:libgio-2.0-$(LT_CURRENT_MINUS_AGE).dll -def:$(builddir)/gio.def -out:$@
'''

gio_headers = files([
  'gappinfo.h',
  'gasyncinitable.h',
  'gasyncresult.h',
  'gbufferedinputstream.h',
  'gbufferedoutputstream.h',
  'gbytesicon.h',
  'gcancellable.h',
  'gcontenttype.h',
  'gcharsetconverter.h',
  'gconverter.h',
  'gconverterinputstream.h',
  'gconverteroutputstream.h',
  'gdatagrambased.h',
  'gdatainputstream.h',
  'gdataoutputstream.h',
  'gdrive.h',
  'gemblem.h',
  'gemblemedicon.h',
  'gfile.h',
  'gfileattribute.h',
  'gfileenumerator.h',
  'gfileicon.h',
  'gfileinfo.h',
  'gfileinputstream.h',
  'gfilemonitor.h',
  'gfilenamecompleter.h',
  'gfileoutputstream.h',
  'gfileiostream.h',
  'gfilterinputstream.h',
  'gfilteroutputstream.h',
  'gicon.h',
  'ginetaddress.h',
  'ginetaddressmask.h',
  'ginetsocketaddress.h',
  'ginputstream.h',
  'ginitable.h',
  'gio.h',
  'gio-autocleanups.h',
  'giotypes.h',
  'gioenums.h',
  'gioerror.h',
  'giomodule.h',
  'gioscheduler.h',
  'giostream.h',
  'gloadableicon.h',
  'gmount.h',
  'gmemoryinputstream.h',
  'gmemoryoutputstream.h',
  'gmountoperation.h',
  'gnativevolumemonitor.h',
  'gnetworkaddress.h',
  'gnetworkmonitor.h',
  'gnetworkservice.h',
  'goutputstream.h',
  'gpermission.h',
  'gpollableinputstream.h',
  'gpollableoutputstream.h',
  'gpollableutils.h',
  'gproxyaddress.h',
  'gproxy.h',
  'gproxyaddressenumerator.h',
  'gproxyresolver.h',
  'gresolver.h',
  'gresource.h',
  'gseekable.h',
  'gsimpleasyncresult.h',
  'gsimpleiostream.h',
  'gsimplepermission.h',
  'gsocket.h',
  'gsocketaddress.h',
  'gsocketaddressenumerator.h',
  'gsocketclient.h',
  'gsocketconnectable.h',
  'gsocketconnection.h',
  'gsocketcontrolmessage.h',
  'gsocketlistener.h',
  'gsocketservice.h',
  'gsrvtarget.h',
  'gsimpleproxyresolver.h',
  'gtask.h',
  'gsubprocess.h',
  'gsubprocesslauncher.h',
  'gtcpconnection.h',
  'gtcpwrapperconnection.h',
  'gthreadedsocketservice.h',
  'gthemedicon.h',
  'gtlsbackend.h',
  'gtlscertificate.h',
  'gtlsclientconnection.h',
  'gtlsconnection.h',
  'gtlsdatabase.h',
  'gtlsfiledatabase.h',
  'gtlsinteraction.h',
  'gtlspassword.h',
  'gtlsserverconnection.h',
  'gdtlsconnection.h',
  'gdtlsclientconnection.h',
  'gdtlsserverconnection.h',
  'gvfs.h',
  'gvolume.h',
  'gvolumemonitor.h',
  'gzlibcompressor.h',
  'gzlibdecompressor.h',
  'glistmodel.h',
  'gliststore.h',
])

gio_headers += application_headers
gio_headers += settings_headers
gio_headers += gdbus_headers
install_headers(gio_headers, subdir : 'glib-2.0/gio/')

# We can't use gnome.mkenums() because the GNOME module looks for glib-mkenums
# in PATH, which means you can't bootstrap glib with its own glib-mkenums.
gioenumtypes_h = custom_target('gioenumtypes_h',
  output : 'gioenumtypes.h',
  capture : true,
  input : gio_headers,
  install : true,
  install_dir : join_paths(get_option('includedir'), 'glib-2.0/gio'),
  command : [python, glib_mkenums,
             '--template', meson.current_source_dir() + '/gioenumtypes.h.template',
             '@INPUT@', gnetworking_h])

gioenumtypes_c = custom_target('gioenumtypes_c',
  output : 'gioenumtypes.c',
  capture : true,
  input : gio_headers,
  depends : [gioenumtypes_h],
  command : [python, glib_mkenums,
             '--template', meson.current_source_dir() + '/gioenumtypes.c.template',
             '@INPUT@', gnetworking_h])

gioenumtypes_dep = declare_dependency(sources : [gioenumtypes_h])

# inotify
if glib_conf.has('HAVE_SYS_INOTIFY_H') and have_func_inotify_init1
  subdir('inotify')
  internal_deps += [ inotify_lib ]
endif

# kevent
if have_func_kqueue and have_func_kevent
  subdir('kqueue')
  internal_deps += [ kqueue_lib ]
endif

if host_system == 'windows'
  subdir('win32')
  internal_deps += [ giowin32_lib ]
endif

# FIXME: FAM support
#if HAVE_FAM
#  subdir('fam')
#endif

install_data([
  'completion/gapplication',
  'completion/gdbus',
  'completion/gsettings',
  'completion/gresource'
],
install_dir: join_paths(get_option('datadir'), 'bash-completion/completions'))

if enable_dtrace
  gio_dtrace_obj = dtrace_obj_gen.process('gio_probes.d')
  gio_dtrace_hdr = dtrace_hdr_gen.process('gio_probes.d')
else
  gio_dtrace_obj = []
  gio_dtrace_hdr = []
endif

libgio = shared_library('gio-2.0',
  gioenumtypes_h, gioenumtypes_c, gnetworking_h, gio_sources,
  gio_dtrace_hdr, gio_dtrace_obj,
  version : library_version,
  soversion : soversion,
  install : true,
  include_directories : [configinc, gioinc],
  link_with : internal_deps,
  #libgio_2_0_la_LDFLAGS = $(GLIB_LINK_FLAGS)',
  #  '$(gio_win32_res_ldflag)',
  #$(SELINUX_LIBS)
  #$(XATTR_LIBS)
  dependencies : [libintl, libz_dep, libdl_dep, libmount_dep, libglib_dep,
                  libgobject_dep, libgmodule_dep] + platform_deps + network_libs,
  c_args : gio_c_args,
  # intl.lib is not compatible with SAFESEH
  link_args : noseh_link_args,
)

libgio_dep = declare_dependency(link_with : libgio,
  dependencies : [gioenumtypes_dep],
  # We sadly need to export configinc here because everyone includes <gio/*.h>
  include_directories : [configinc, gioinc])

# Dependencies used by executables below
libelf = dependency('libelf', version : '>= 0.8.12', required : false)
if libelf.found()
  glib_conf.set('HAVE_LIBELF', 1)
endif

gconstructor_as_data_h = custom_target('gconstructor_as_data.h',
    input : ['data-to-c.py', files('../glib/gconstructor.h')],
    output : ['gconstructor_as_data.h'],
    command : [python, '@INPUT0@', '@INPUT1@', 'gconstructor_code', '@OUTPUT@'])

# Several installed executables
gio_tool_sources = [
  'gio-tool.c',
  'gio-tool.h',
  'gio-tool-cat.c',
  'gio-tool-copy.c',
  'gio-tool-info.c',
  'gio-tool-list.c',
  'gio-tool-mime.c',
  'gio-tool-mkdir.c',
  'gio-tool-monitor.c',
  'gio-tool-mount.c',
  'gio-tool-move.c',
  'gio-tool-open.c',
  'gio-tool-rename.c',
  'gio-tool-remove.c',
  'gio-tool-save.c',
  'gio-tool-set.c',
  'gio-tool-trash.c',
  'gio-tool-tree.c',
]

# FIXME: are all these HAVE_CONFIG_H needed/used here?
executable('gio', gio_tool_sources,
  install : true,
  c_args : ['-DHAVE_CONFIG_H=1'] + gio_c_args,
  # intl.lib is not compatible with SAFESEH
  link_args : noseh_link_args,
  dependencies : [libintl, libgio_dep, libgobject_dep, libgmodule_dep, libglib_dep])

executable('gresource', 'gresource-tool.c',
  install : true,
  c_args : ['-DHAVE_CONFIG_H=1'],
  # intl.lib is not compatible with SAFESEH
  link_args : noseh_link_args,
  dependencies : [libelf, libintl, libgio_dep, libgobject_dep, libgmodule_dep, libglib_dep])

executable('gio-querymodules', 'gio-querymodules.c',
  install : true,
  c_args : ['-DHAVE_CONFIG_H=1'] + gio_c_args,
  # intl.lib is not compatible with SAFESEH
  link_args : noseh_link_args,
  dependencies : [libgio_dep, libgobject_dep, libgmodule_dep, libglib_dep])

glib_compile_schemas = executable('glib-compile-schemas',
  [gconstructor_as_data_h, 'gvdb/gvdb-builder.c', 'glib-compile-schemas.c'],
  install : true,
  c_args : ['-DHAVE_CONFIG_H=1'],
  # intl.lib is not compatible with SAFESEH
  link_args : noseh_link_args,
  dependencies : [libintl, libgio_dep, libgobject_dep, libgmodule_dep, libglib_dep])

glib_compile_resources = executable('glib-compile-resources',
  [gconstructor_as_data_h, 'gvdb/gvdb-builder.c', 'glib-compile-resources.c'],
  install : true,
  c_args : ['-DHAVE_CONFIG_H=1'] + gio_c_args,
  # intl.lib is not compatible with SAFESEH
  link_args : noseh_link_args,
  dependencies : [libintl, libgio_dep, libgobject_dep, libgmodule_dep, libglib_dep])

executable('gsettings', 'gsettings-tool.c',
  install : true,
  c_args : ['-DHAVE_CONFIG_H=1'] + gio_c_args,
  # intl.lib is not compatible with SAFESEH
  link_args : noseh_link_args,
  dependencies : [libintl, libgio_dep, libgobject_dep, libgmodule_dep, libglib_dep])
install_data('gschema.dtd',
  install_dir : join_paths(get_option('datadir'), 'glib-2.0/schemas'))

install_data(['gschema.loc', 'gschema.its'],
  install_dir : join_paths(get_option('datadir'), 'gettext/its'))

executable('gdbus', 'gdbus-tool.c',
  install : true,
  c_args : ['-DHAVE_CONFIG_H=1'] + gio_c_args,
  # intl.lib is not compatible with SAFESEH
  link_args : noseh_link_args,
  dependencies : [libintl, libgio_dep, libgobject_dep, libgmodule_dep, libglib_dep])

if host_system != 'windows'
  executable('gapplication', 'gapplication-tool.c',
    install : true,
    c_args : ['-DHAVE_CONFIG_H=1'] + gio_c_args,
    # intl.lib is not compatible with SAFESEH
    link_args : noseh_link_args,
    dependencies : [libintl, libgio_dep, libgobject_dep, libgmodule_dep, libglib_dep])
endif

if enable_systemtap
  gio_stp = configure_file(input : 'gio.stp.in',
    output : '@0@.stp'.format(libgio.full_path().split('/').get(-1)),
    configuration : stp_cdata,
    install_dir : tapset_install_dir,
    install : true)
endif

if host_system != 'windows'
  subdir('tests')
endif
