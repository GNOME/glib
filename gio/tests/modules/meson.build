libtestmodulea = shared_library('testmodulea', 'test-module-a.c',
  install : false,
  include_directories : inc_dirs,
  link_with : [ libglib, libgobject, libgmodule, libgio ],
  c_args : [ ]
)

libtestmoduleb = shared_library('testmoduleb', 'test-module-b.c',
  install : false,
  include_directories : inc_dirs,
  link_with : [ libglib, libgobject, libgmodule, libgio ],
  c_args : [ ]
)
