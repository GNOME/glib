fuzz_targets = [
  'fuzz_bookmark',
  'fuzz_canonicalize_filename',
  'fuzz_date_parse',
  'fuzz_date_time_new_from_iso8601',
  'fuzz_dbus_message',
  'fuzz_inet_address_mask_new_from_string',
  'fuzz_inet_address_new_from_string',
  'fuzz_inet_socket_address_new_from_string',
  'fuzz_key',
  'fuzz_network_address_parse',
  'fuzz_network_address_parse_uri',
  'fuzz_paths',
  'fuzz_uri_escape',
  'fuzz_uri_parse',
  'fuzz_uri_parse_params',
  'fuzz_variant_binary',
  'fuzz_variant_text',
]

deps = [libgmodule_dep, libgio_dep, libglib_dep, libgobject_dep]

extra_sources = []
extra_c_args = cc.get_supported_arguments('-Werror=unused-function')

# Links in a static library provided by oss-fuzz, else a standalone driver.
# https://github.com/google/oss-fuzz/blob/master/docs/new_project_guide.md#buildsh-script-environment
fuzzing_engine = cxx.find_library('FuzzingEngine', required : get_option('oss_fuzz'))
if fuzzing_engine.found()
  deps += fuzzing_engine
else
  extra_sources += 'driver.c'
endif

foreach target_name : fuzz_targets
  exe = executable(target_name, [extra_sources, target_name + '.c'],
    c_args : extra_c_args,
    dependencies : deps,
  )
endforeach
