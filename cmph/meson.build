cmph_sources = [
  'bdz.c',
  'bdz_ph.c',
  'bmz8.c',
  'bmz.c',
  'brz.c',
  'buffer_entry.c',
  'buffer_manager.c',
  'chd.c',
  'chd_ph.c',
  'chm.c',
  'cmph.c',
  'cmph_structs.c',
  'compressed_rank.c',
  'compressed_seq.c',
  'fch_buckets.c',
  'fch.c',
  'graph.c',
  'hash.c',
  'jenkins_hash.c',
  'miller_rabin.c',
  'select.c',
  'vqueue.c',
  'vstack.c',
]

cmph_deps = [
  glib_dep,
  cc.find_library('m', required: false),
]

cmph = static_library('cmph',
  sources: cmph_sources,
  c_args: gi_hidden_visibility_cflags,
  dependencies: cmph_deps,
)

cmph_dep = declare_dependency(
  link_with: cmph,
  include_directories: include_directories('.'),
)

cmph_test = executable('cmph-bdz-test', '../cmph-bdz-test.c',
  dependencies: [
    cmph_dep,
    glib_dep,
  ]
)

test('cmph-bdz-test', cmph_test)
