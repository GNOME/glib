#!/bin/sh

LANG=C

status=0

if ! which readelf 2>/dev/null >/dev/null; then
	echo "'readelf' not found; skipping test"
	exit 0
fi

SKIP='\<g_access\|\<g_array_\|\<g_ascii\|\<g_list_\|\<g_assertion_message\|\<g_warn_message\|\<g_atomic\|\<g_build_filename\|\<g_byte_array\|\<g_child_watch\|\<g_convert\|\<g_dir_\|\<g_error_\|\<g_clear_error\|\<g_file_error_quark\|\<g_file_get_contents\|\<g_file_set_contents\|\<g_file_test\|\<g_file_read_link\|\<g_filename_\|\<g_find_program_in_path\|\<g_free\|\<g_get_\|\<g_getenv\|\<g_hash_table_\|\<g_idle_\|\<g_intern_static_string\|\<g_io_channel_\|\<g_key_file_\|\<g_listenv\|\<g_locale_to_utf8\|\<g_log\|\<g_main_context_wakeup\|\<g_malloc\|\<g_markup_\|\<g_mkdir_\|\<g_mkstemp\|\<g_module_\|\<g_object_\|\<g_once_\|\<g_param_spec_\|\<g_path_\|\<g_printerr\|\<g_propagate_error\|\<g_ptr_array_\|\<g_qsort_\|\<g_quark_\|\<g_queue_\|\<g_realloc\|\<g_return_if_fail\|\<g_set_error\|\<g_shell_\|\<g_signal_\|\<g_slice_\|\<g_slist_\|\<g_snprintf\|\<g_source_\|\<g_spawn_\|\<g_static_\|\<g_str\|\<g_thread_pool_\|\<g_time_val_add\|\<g_timeout_\|\<g_type_\|\<g_unlink\|\<g_uri_\|\<g_utf8_\|\<g_value_\|\<g_enum_\|\<g_flags_\|\<g_checksum\|\<g_io_add_watch\|\<g_bit_\|\<g_poll\|\<g_boxed'

for so in .libs/lib*.so; do
	echo Checking $so for local PLT entries
	readelf -r $so | grep 'JU\?MP_SLOT\?' | grep '\<g_' | grep -v $SKIP && status=1
done

exit $status
