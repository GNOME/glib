Title: Testing Framework

# Testing Framework

GLib provides a framework for writing and maintaining unit tests in parallel
to the code they are testing. The API is designed according to established
concepts found in the other test frameworks (JUnit, NUnit, RUnit), which in
turn is based on smalltalk unit testing concepts.

- Test case: Tests (test methods) are grouped together with their fixture
  into test cases.
- Fixture: A test fixture consists of fixture data and setup and teardown
  methods to establish the environment for the test functions. We use fresh
  fixtures, i.e. fixtures are newly set up and torn down around each test
  invocation to avoid dependencies between tests.
- Test suite: Test cases can be grouped into test suites, to allow subsets
  of the available tests to be run. Test suites can be grouped into other
  test suites as well.

The API is designed to handle creation and registration of test suites and
test cases implicitly. A simple call like:

```c
g_test_add_func ("/misc/assertions", test_assertions);
```

creates a test suite called "misc" with a single test case named
"assertions", which consists of running the `test_assertions` function.

In addition to the traditional `g_assert_true()`, the test framework
provides an extended set of assertions for comparisons:
`g_assert_cmpfloat()`, `g_assert_cmpfloat_with_epsilon()`,
`g_assert_cmpint()`, `g_assert_cmpuint()`, `g_assert_cmphex()`,
`g_assert_cmpstr()`, `g_assert_cmpmem()` and `g_assert_cmpvariant()`. The
advantage of these variants over plain `g_assert_true()` is that the
assertion messages can be more elaborate, and include the values of the
compared entities.

Note that `g_assert()` should **not** be used in unit tests, since it is a
no-op when compiling with `G_DISABLE_ASSERT`. Use `g_assert()` in production
code, and `g_assert_true()` in unit tests.

A full example of creating a test suite with two tests using fixtures:

```c
#include <glib.h>
#include <locale.h>

typedef struct {
  MyObject *obj;
  OtherObject *helper;
} MyObjectFixture;

static void
my_object_fixture_set_up (MyObjectFixture *fixture,
                          gconstpointer user_data)
{
  fixture->obj = my_object_new ();
  my_object_set_prop1 (fixture->obj, "some-value");
  my_object_do_some_complex_setup (fixture->obj, user_data);

  fixture->helper = other_object_new ();
}

static void
my_object_fixture_tear_down (MyObjectFixture *fixture,
                             gconstpointer user_data)
{
  g_clear_object (&fixture->helper);
  g_clear_object (&fixture->obj);
}

static void
test_my_object_test1 (MyObjectFixture *fixture,
                      gconstpointer user_data)
{
  g_assert_cmpstr (my_object_get_property (fixture->obj), ==, "initial-value");
}

static void
test_my_object_test2 (MyObjectFixture *fixture,
                      gconstpointer user_data)
{
  my_object_do_some_work_using_helper (fixture->obj, fixture->helper);
  g_assert_cmpstr (my_object_get_property (fixture->obj), ==, "updated-value");
}

int
main (int argc, char *argv[])
{
  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);

  // Define the tests.
  g_test_add ("/my-object/test1", MyObjectFixture, "some-user-data",
              my_object_fixture_set_up, test_my_object_test1,
              my_object_fixture_tear_down);
  g_test_add ("/my-object/test2", MyObjectFixture, "some-user-data",
              my_object_fixture_set_up, test_my_object_test2,
              my_object_fixture_tear_down);

  return g_test_run ();
}
```

### Integrating GTest in your project

#### Using Meson

If you are using the Meson build system, you will typically use the provided
`test()` primitive to call the test binaries, e.g.:

```
test(
  'foo',
  executable('foo', 'foo.c', dependencies: deps),
  env: [
    'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
    'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  ],
  protocol: 'tap',
)

test(
  'bar',
  executable('bar', 'bar.c', dependencies: deps),
  env: [
    'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
    'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  ],
  protocol: 'tap',
)
```

#### Using Autotools

If you are using Autotools, you're strongly encouraged to use the Automake
TAP harness; GLib provides template files for easily integrating with it:

- [`glib-tap.mk`](https://gitlab.gnome.org/GNOME/glib/blob/glib-2-58/glib-tap.mk)
- [`tap-test`](https://gitlab.gnome.org/GNOME/glib/blob/glib-2-58/tap-test)
- [`tap-driver.sh`](https://gitlab.gnome.org/GNOME/glib/blob/glib-2-58/tap-driver.sh)

You can copy these files in your own project's root directory, and then set
up your `Makefile.am` file to reference them, for instance:

```
include $(top_srcdir)/glib-tap.mk

# test binaries
test_programs = \
  foo \
  bar

# data distributed in the tarball
dist_test_data = \
  foo.data.txt \
  bar.data.txt

# data not distributed in the tarball
test_data = \
  blah.data.txt
```

Make sure to distribute the TAP files, using something like the following in
your top-level `Makefile.am`:

```
EXTRA_DIST += \
  tap-driver.sh \
  tap-test
```

`glib-tap.mk` will be distributed implicitly due to being included in a
`Makefile.am`. All three files should be added to version control.

If you don't have access to the Autotools TAP harness, you can use the
gtester and gtester-report tools, and use the
[`glib.mk`](https://gitlab.gnome.org/GNOME/glib/blob/glib-2-58/glib.mk)
Automake template provided by GLib. Note, however, that since GLib 2.62,
gtester and gtester-report have been deprecated in favour of using TAP. The
`--tap` argument to tests is enabled by default as of GLib 2.62.
