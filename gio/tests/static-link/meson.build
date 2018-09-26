project('test-static-link', 'c')

# Test we can static link on installed gio library. This is ran by the CI after
# installing the build into a separate prefix.
# See .gitlab-ci/fedora-test-installed.sh
app = executable('test-static-link', 'app.c',
  dependencies : dependency('gio-2.0', static : true)
)
test('test-static-link', app)
