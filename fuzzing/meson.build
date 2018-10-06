fuzz_targets = [
  'fuzz_bookmark',
  'fuzz_dbus_message',
  'fuzz_key',
  'fuzz_variant_binary',
  'fuzz_variant_text',
]

deps = [libgmodule_dep, libgio_dep, libglib_dep, libgobject_dep]
extra_sources = []

# Links in a static library provided by oss-fuzz, else a standalone driver.
fuzzing_engine = cxx.find_library('FuzzingEngine', required : false)
if fuzzing_engine.found()
  deps += fuzzing_engine
else
  extra_sources += 'fuzzing_driver.c'
endif

foreach target_name : fuzz_targets
  exe = executable(target_name, [extra_sources, target_name + '.c'],
   cpp_args : [
      '-Wno-missing-prototypes',
      '-Wno-unused-function',
      '-Wno-unused-variable',
    ],
    dependencies : deps,
  )
endforeach
