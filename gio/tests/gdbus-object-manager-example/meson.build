# FIXME: set UNINSTALLED_GLIB_{SRC|BUILD}DIR=top_{src|build}dir ?
gdbus_example_objectmanager_xml = files('gdbus-example-objectmanager.xml')
gdbus_example_objectmanager_generated = custom_target('objectmanager-gen',
  input  : gdbus_example_objectmanager_xml,
  output : ['objectmanager-gen.h',
            'objectmanager-gen.c',
            'objectmanager-gen-org.gtk.GDBus.Example.ObjectManager.Animal.xml',
            'objectmanager-gen-org.gtk.GDBus.Example.ObjectManager.Cat.xml'],
  command : [python, gdbus_codegen,
             '--interface-prefix', 'org.gtk.GDBus.Example.ObjectManager.',
             '--c-namespace', 'Example',
             '--c-generate-object-manager',
             '--output-directory', '@OUTDIR@',
             '--generate-c-code', 'objectmanager-gen',
             '--generate-docbook', 'objectmanager-gen',
             '--symbol-decorator', '_GLIB_EXTERN',
             '--symbol-decorator-define', 'HAVE_CONFIG_H',
             '@INPUT@'])

gdbus_example_objectmanager_rst_gen = custom_target('objectmanager-rst-gen',
  input: gdbus_example_objectmanager_xml,
  output: [
    'objectmanager-rst-gen-org.gtk.GDBus.Example.ObjectManager.Animal.rst',
    'objectmanager-rst-gen-org.gtk.GDBus.Example.ObjectManager.Cat.rst',
  ],
  command: [
    python,
    gdbus_codegen,
    '--interface-prefix', 'org.gtk.GDBus.Example.ObjectManager.',
    '--generate-rst', 'objectmanager-rst-gen',
    '--output-directory', '@OUTDIR@',
    '@INPUT@',
  ],
)

libgdbus_example_objectmanager = library('gdbus-example-objectmanager',
    gdbus_example_objectmanager_generated,
    c_args : test_c_args,
    dependencies : [libglib_dep, libgmodule_dep, libgobject_dep, libgio_dep],
    install : installed_tests_enabled,
    install_dir : installed_tests_execdir)

libgdbus_example_objectmanager_dep = declare_dependency(
  sources : [
    gdbus_example_objectmanager_generated[0],
    gdbus_example_objectmanager_rst_gen[0],
  ],
  link_with : libgdbus_example_objectmanager,
  dependencies : [libgio_dep])
