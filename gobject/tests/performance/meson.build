common_c_args = test_cargs + ['-DGLIB_DISABLE_DEPRECATION_WARNINGS']
common_deps = [libm, thread_dep, libglib_dep, libgobject_dep]

# Don't install these ones, and keep them out of 'meson test' because they take too long...
executable('performance', 'performance.c',
    c_args : common_c_args,
    dependencies : common_deps,
    install : false)

executable('performance-threaded', 'performance-threaded.c',
    c_args : common_c_args,
    dependencies : common_deps,
    install : false)