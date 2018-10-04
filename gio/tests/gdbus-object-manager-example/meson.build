# FIXME: set UNINSTALLED_GLIB_{SRC|BUILD}DIR=top_{src|build}dir ?
gdbus_example_objectmanager_generated = custom_target('gdbus-example-objectmanager-gen',
  input  : ['example.xml'],
  output : ['gdbus-example-objectmanager-gen.h',
            'gdbus-example-objectmanager-gen.c',
            'gdbus-example-objectmanager-gen-org.gtk.GDBus.Example.ObjectManager.Animal.xml',
            'gdbus-example-objectmanager-gen-org.gtk.GDBus.Example.ObjectManager.Cat.xml'],
  command : [python, gdbus_codegen,
             '--interface-prefix', 'org.gtk.GDBus.Example.ObjectManager.',
             '--c-namespace', 'Example',
             '--c-generate-object-manager',
             '--output-directory', '@OUTDIR@',
             '--generate-c-code', 'gdbus-example-objectmanager-gen',
             '--generate-docbook', 'gdbus-example-objectmanager-gen',
             '@INPUT@'])

libgdbus_example_objectmanager = library('gdbus-example-objectmanager',
    gdbus_example_objectmanager_generated,
    c_args : gio_c_args,
    dependencies : [libglib_dep, libgmodule_dep, libgobject_dep, libgio_dep],
    install : installed_tests_enabled,
    install_dir : installed_tests_execdir)

libgdbus_example_objectmanager_dep = declare_dependency(sources : gdbus_example_objectmanager_generated,
  link_with : libgdbus_example_objectmanager)
