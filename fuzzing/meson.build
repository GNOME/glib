fuzz_targets = [
  'fuzz_bookmark',
  'fuzz_dbus_message',
  'fuzz_key',
  'fuzz_variant_binary',
  'fuzz_variant_text',
]

deps = [libgmodule_dep, libgio_dep, libglib_dep, libgobject_dep]

# Links in a static library provided by oss-fuzz, else a standalone driver.
fuzzing_engine = meson.get_compiler('c').find_library('FuzzingEngine',
  required : false,
)
if fuzzing_engine.found()
  deps += fuzzing_engine
else
  fuzzing_driver = declare_dependency(
    sources : 'StandaloneFuzzTargetMain.c',
    compile_args : '-std=c99',
  )
  deps += fuzzing_driver
endif

foreach target_name : fuzz_targets
  exe = executable(target_name, target_name + '.cc',
   cpp_args : [
      '-std=c++11',
      '-Wno-missing-prototypes',
      '-Wno-unused-function',
      '-Wno-unused-variable',
    ],
    dependencies : deps,
  )
endforeach
