gio_c_args = [
  '-DG_LOG_DOMAIN="GLib-GIO"',
  '-DGIO_COMPILATION',
  '-DGIO_MODULE_DIR="@0@/gio/modules"'.format(get_option('libdir')),
]

# FIXME: subdir('gdbus-2.0/codegen')

gnetworking_h_conf = configuration_data()

gnetworking_h_wspiapi_include = ''
gnetworking_h_nameser_compat_include = ''

if host_machine.system() == 'windows'
  # <wspiapi.h> in the Windows SDK and in mingw-w64 has wrappers for
  # inline workarounds for getaddrinfo, getnameinfo and freeaddrinfo if
  # they aren't present at run-time (on Windows 2000).
  gnetworking_h_wspiapi_include = '#include <wspiapi.h>'
endif

if host_machine.system().contains('android')
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
if host_machine.system() != 'windows'
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
      network_libs += [ find_library('resolv') ]
      network_args += [ '-lresolv' ]
    elif cc.links(res_query_test, args : '-lbind', name : 'res_query() in -lbind')
      network_libs += [ find_library('bind') ]
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
      network_libs += [ find_library('socket') ]
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

gnetworking_h_conf.set('WSPIAPI_INCLUDE', gnetworking_h_wspiapi_include)
gnetworking_h_conf.set('NAMESER_COMPAT_INCLUDE', gnetworking_h_nameser_compat_include)

gnetworking_h = configure_file(input : 'gnetworking.h.in',
                               output : 'gnetworking.h',
                               install_dir : 'include/glib-2.0/gio/',
                               configuration : gnetworking_h_conf)

gdbus_headers = [
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
]

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

# FIXME: These are not built into the library yet
#EXTRA_DIST += gdbusdaemon.c gdbusdaemon.h dbus-daemon.xml
#gdbus-daemon-generated.h gdbus-daemon-generated.c: $(srcdir)/dbus-daemon.xml $(srcdir)/gdbus-2.0/codegen/gdbus-codegen.in
#  $(AM_V_GEN) UNINSTALLED_GLIB_SRCDIR=$(top_srcdir) ',
#    UNINSTALLED_GLIB_BUILDDIR=$(top_builddir) ',
#    $(PYTHON) $(srcdir)/gdbus-2.0/codegen/gdbus-codegen.in ',
#    --interface-prefix org. ',
#    --generate-c-code gdbus-daemon-generated ',
#    --c-namespace _G ',
#    $(srcdir)/dbus-daemon.xml

settings_headers = [
  'gsettingsbackend.h',
  'gsettingsschema.h',
  'gsettings.h',
]

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

if host_machine.system() == 'windows'
  settings_sources += [ 'gregistrysettingsbackend.c' ]
endif

# FIXME:
#if OS_COCOA
#settings_sources += [ 'gnextstepsettingsbackend.c' ]
#endif

application_headers = [
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
]

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
  'ghttpproxy.h',
  'glocalfile.c',
  'glocalfile.h',
  'glocalfileenumerator.c',
  'glocalfileenumerator.h',
  'glocalfileinfo.c',
  'glocalfileinfo.h',
  'glocalfileinputstream.c',
  'glocalfileinputstream.h',
  'glocalfilemonitor.c',
  'glocalfilemonitor.h',
  'glocalfileoutputstream.c',
  'glocalfileoutputstream.h',
  'glocalfileiostream.c',
  'glocalfileiostream.h',
  'glocalvfs.c',
  'glocalvfs.h',
  'gsocks4proxy.c',
  'gsocks4proxy.h',
  'gsocks4aproxy.c',
  'gsocks4aproxy.h',
  'gsocks5proxy.c',
  'gsocks5proxy.h',
  'thumbnail-verify.h',
  'thumbnail-verify.c',
]

platform_deps = [ ]
internal_deps = [ ]
appinfo_sources = [ ]

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

if host_machine.system() == 'windows'
  subdir('win32')
  internal_deps += [ giowin32_lib ]
endif

# FIXME: FAM support
#if HAVE_FAM
#  subdir('fam')
#endif

unix_sources = [ ]
if host_machine.system() != 'windows'
  appinfo_sources += [ 'gdesktopappinfo.c' ]
  subdir('xdgmime')
  internal_deps += [ xdgmime_lib ]
  unix_sources = [
    'gfiledescriptorbased.c',
    'gunixconnection.c',
    'gunixcredentialsmessage.c',
    'gunixfdlist.c',
    'gunixfdmessage.c',
    'gunixmount.c',
    'gunixmount.h',
    'gunixmounts.c',
    'gunixsocketaddress.c',
    'gunixvolume.c',
    'gunixvolume.h',
    'gunixvolumemonitor.c',
    'gunixvolumemonitor.h',
    'gunixinputstream.c',
    'gunixoutputstream.c',
    'gcontenttype.c',
    'gcontenttypeprivate.h',
    'gfdonotificationbackend.c',
    'ggtknotificationbackend.c',
  ]

  # FIXME
  #if OS_COCOA
  #unix_sources += gcocoanotificationbackend.c
  #endif

  gio_unix_include_headers = [
    'gdesktopappinfo.h',
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
  install_headers(gio_unix_include_headers, subdir : 'gio-unix-2.0/gio')

  if glib_conf.has('HAVE_NETLINK')
    unix_sources += [
      'gnetworkmonitornetlink.c',
      'gnetworkmonitornm.c',
    ]
  endif
endif

gdbus_daemon_sources = [
  'gdbusdaemon.c',
  'gdbus-daemon-generated.c',
]

win32_actual_sources = gdbus_daemon_sources + [
  'gwin32registrykey.c',
  'gcontenttype-win32.c',
  'gwin32mount.c',
  'gwin32volumemonitor.c',
  'gwin32inputstream.c',
  'gwin32outputstream.c',
]

if host_machine.system() == 'windows'
  appinfo_sources += [ 'gwin32appinfo.c' ]
  platform_deps += [ find_library('shlwapi'),
                     find_library('ws2_32'),
                     find_library('dnsapi'),
                     find_library('iphlpapi') ]
  # win32_sources = $(win32_actual_sources)

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
# FIXME  'gioenumtypes.c',
  'glistmodel.c',
  'gliststore.c',
]

# FIXME
gio_sources += appinfo_sources
gio_sources += unix_sources
#gio_sources += win32_sources
gio_sources += application_sources
gio_sources += settings_sources
gio_sources += gdbus_sources
gio_sources += local_sources

foo = '''
if OS_WIN32_AND_DLL_COMPILATION
gio_win32_res = gio-win32-res.o
gio_win32_res_ldflag = -Wl,$(gio_win32_res)
endif


if OS_COCOA
# This is dumb.  The ObjC source file should be properly named .m
libgio_2_0_la_CFLAGS += -xobjective-c
libgio_2_0_la_LDFLAGS += -Wl,-framework,Foundation -Wl,-framework,AppKit
endif

libgio_2_0_la_DEPENDENCIES = $(gio_win32_res) $(gio_def) $(platform_deps)

gio-win32-res.o: gio.rc
  '$(WINDRES) gio.rc $@

gioincludedir=$(includedir)/glib-2.0/gio/
gioinclude_HEADERS =',
  '$(gio_headers)',
  'gioenumtypes.h

# these sources (also mentioned above) are generated.
BUILT_SOURCES +=',
  'gconstructor_as_data.h',
  'gioenumtypes.h',
  'gioenumtypes.c',
  'gdbus-daemon-generated.c',
  'gdbus-daemon-generated.h',
  'gnetworking.h',
  '$(NULL)

BUILT_EXTRA_DIST +=',
  'gio.rc

# This is read by gobject-introspection/misc/ and gtk-doc
gio-public-headers.txt: Makefile
  '$(AM_V_GEN) echo $(gioinclude_HEADERS) $(giowin32include_HEADERS) $(giounixinclude_HEADERS) > $@.tmp && mv $@.tmp $@

all-local: gio-public-headers.txt

gioenumtypes.h: $(gio_headers) gioenumtypes.h.template
  '$(AM_V_GEN) $(top_builddir)/gobject/glib-mkenums --template $(filter %.template,$^) $(filter-out %.template,$^) >',
  '    gioenumtypes.h.tmp && mv gioenumtypes.h.tmp gioenumtypes.h

gioenumtypes.c: $(gio_headers) gioenumtypes.c.template
  '$(AM_V_GEN) $(top_builddir)/gobject/glib-mkenums --template $(filter %.template,$^) $(filter-out %.template,$^) >',
  '    gioenumtypes.c.tmp && mv gioenumtypes.c.tmp gioenumtypes.c

gio.def: libgio-2.0.la
  '$(AM_V_GEN) dumpbin.exe -exports .libs/libgio-2.0-0.dll | awk 'BEGIN { print "EXPORTS" } / +[[:digit:]]+ +[[:xdigit:]]+ +[[:xdigit:]]+/{ print $$4 }' > gio.def.tmp && mv gio.def.tmp gio.def

gio-2.0.lib: libgio-2.0.la gio.def
  '$(AM_V_GEN) lib.exe -machine:@LIB_EXE_MACHINE_FLAG@ -name:libgio-2.0-$(LT_CURRENT_MINUS_AGE).dll -def:$(builddir)/gio.def -out:$@

bin_PROGRAMS = gio-querymodules glib-compile-schemas glib-compile-resources gsettings

glib_compile_resources_LDADD = libgio-2.0.la',
  '$(top_builddir)/gobject/libgobject-2.0.la',
  '$(top_builddir)/gmodule/libgmodule-2.0.la',
  '$(top_builddir)/glib/libglib-2.0.la',
  '$(NULL)

glib_compile_resources_SOURCES =',
  'gvdb/gvdb-format.h',
  'gvdb/gvdb-builder.h',
  'gvdb/gvdb-builder.c',
  'glib-compile-resources.c

gio_querymodules_SOURCES = gio-querymodules.c
gio_querymodules_LDADD	 = libgio-2.0.la',
  '$(top_builddir)/gobject/libgobject-2.0.la',
  '$(top_builddir)/gmodule/libgmodule-2.0.la',
  '$(top_builddir)/glib/libglib-2.0.la',
  '$(NULL)

gconstructor_as_data.h: $(top_srcdir)/glib/gconstructor.h data-to-c.pl
  '$(AM_V_GEN) $(srcdir)/data-to-c.pl $(top_srcdir)/glib/gconstructor.h gconstructor_code > $@.tmp && mv $@.tmp $@

glib_compile_schemas_LDADD = $(top_builddir)/glib/libglib-2.0.la
glib_compile_schemas_SOURCES =',
  'gconstructor_as_data.h',
  'gvdb/gvdb-format.h',
  'gvdb/gvdb-builder.h',
  'gvdb/gvdb-builder.c',
  'glib-compile-schemas.c

gsettings_LDADD = libgio-2.0.la',
  '$(top_builddir)/gobject/libgobject-2.0.la',
  '$(top_builddir)/gmodule/libgmodule-2.0.la',
  '$(top_builddir)/glib/libglib-2.0.la',
  '$(NULL)
gsettings_SOURCES = gsettings-tool.c

schemadir = $(datadir)/glib-2.0/schemas
dist_schema_DATA = gschema.dtd

itsdir = $(datadir)/gettext/its
dist_its_DATA = gschema.loc gschema.its

# ------------------------------------------------------------------------
# gdbus(1) tool

bin_PROGRAMS += gdbus
gdbus_SOURCES = gdbus-tool.c
gdbus_LDADD = libgio-2.0.la',
  '$(top_builddir)/gobject/libgobject-2.0.la',
  '$(top_builddir)/gmodule/libgmodule-2.0.la',
  '$(top_builddir)/glib/libglib-2.0.la',
  '$(NULL)

if OS_UNIX
# ------------------------------------------------------------------------
# gapplication(1) tool
bin_PROGRAMS += gapplication
gapplication_SOURCES = gapplication-tool.c
gapplication_LDADD = libgio-2.0.la',
  '$(top_builddir)/gobject/libgobject-2.0.la',
  '$(top_builddir)/gmodule/libgmodule-2.0.la',
  '$(top_builddir)/glib/libglib-2.0.la',
  '$(NULL)
endif

completiondir = $(datadir)/bash-completion/completions
completion_DATA =',
  'completion/gapplication',
  'completion/gdbus',
  'completion/gsettings',
  'completion/gresource
EXTRA_DIST += $(completion_DATA)
'''

gio_headers = [
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
]
# FIXME: 'gnetworking.h', - generated? was in nodist_gioinclude_HEADERS
gio_headers += application_headers
gio_headers += settings_headers
gio_headers += gdbus_headers
install_headers(gio_headers, subdir : 'glib-2.0/gio/')

gio_mkenums = find_program('build_mkenum.py')

gioenumtypes_h = custom_target('gioenumtypes_h',
  output : 'gioenumtypes.h',
  input : gio_headers,
  install : true,
  install_dir : 'include/glib-2.0/gio/',
  depends : [ ],
  command : [ gio_mkenums, '@OUTPUT@', meson.source_root(), '@INPUT@', gnetworking_h ])

gioenumtypes_c = custom_target('gioenumtypes_c',
  output : 'gioenumtypes.c',
  input : gio_headers,
  depends : [ gioenumtypes_h ],
  command : [ gio_mkenums, '@OUTPUT@', meson.source_root(), '@INPUT@', gnetworking_h, ])

libgio = shared_library('gio',
  gioenumtypes_h, gioenumtypes_c, gnetworking_h, gio_sources,
  version : glib_version,
  soversion : interface_version,
  install : true,
  include_directories : inc_dirs,
  link_with : [ libglib, libgobject, libgmodule ] + internal_deps,
  #libgio_2_0_la_LDFLAGS = $(GLIB_LINK_FLAGS)',
  #  '$(gio_win32_res_ldflag)',
  #  '-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)',
  #  '-export-dynamic $(no_undefined)
  #$(ZLIB_LIBS)
  #$(SELINUX_LIBS)
  #$(GLIB_LIBS)
  #$(XATTR_LIBS)
  #$(NETWORK_LIBS)
  dependencies : [ libz_dep, libdl_dep ] + platform_deps + network_libs,
  c_args : gio_c_args
)

# gresource tool
libelf = dependency('libelf', version : '>= 0.8.12', required : false)
if libelf.found()
  glib_conf.set('HAVE_LIBELF', 1)
endif
executable('gresource', 'gresource-tool.c',
  include_directories : inc_dirs,
  c_args : [ '-DHAVE_CONFIG_H=1' ],
  link_with : [ libgio, libgobject, libgmodule, libglib ],
  dependencies : libelf,
)

subdir('tests')
