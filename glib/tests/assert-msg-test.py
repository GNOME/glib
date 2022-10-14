#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright © 2022 Emmanuel Fleury <emmanuel.fleury@gmail.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA  02110-1301  USA

""" Integration tests for g_assert() functions. """

import collections
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

import taptestrunner

Result = collections.namedtuple("Result", ("info", "out", "err"))

GDB_SCRIPT = """
# Work around https://sourceware.org/bugzilla/show_bug.cgi?id=22501
set confirm off
set print elements 0
set auto-load safe-path /
run
print *((char**) &__glib_assert_msg)
quit
"""


class TestAssertMessage(unittest.TestCase):
    """Integration test for throwing message on g_assert().

    This can be run when installed or uninstalled. When uninstalled,
    it requires G_TEST_BUILDDIR and G_TEST_SRCDIR to be set.

    The idea with this test harness is to test if g_assert() prints
    an error message when called, and that it saves this error
    message in a global variable accessible to gdb, so that developers
    and automated tools can more easily debug assertion failures.
    """

    def setUp(self):
        self.__gdb = shutil.which("gdb")
        self.timeout_seconds = 10  # seconds per test

        ext = ""
        if os.name == "nt":
            ext = ".exe"
        if "G_TEST_BUILDDIR" in os.environ:
            self.__assert_msg_test = os.path.join(
                os.environ["G_TEST_BUILDDIR"], "assert-msg-test" + ext
            )
        else:
            self.__assert_msg_test = os.path.join(
                os.path.dirname(__file__), "assert-msg-test" + ext
            )
        print("assert-msg-test:", self.__assert_msg_test)

    def runAssertMessage(self, *args):
        argv = [self.__assert_msg_test]
        argv.extend(args)
        print("Running:", argv)

        env = os.environ.copy()
        env["LC_ALL"] = "C.UTF-8"
        print("Environment:", env)

        # We want to ensure consistent line endings...
        info = subprocess.run(
            argv,
            timeout=self.timeout_seconds,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=env,
            universal_newlines=True,
        )
        out = info.stdout.strip()
        err = info.stderr.strip()

        result = Result(info, out, err)

        print("Output:", result.out)
        print("Error:", result.err)
        return result

    def runGdbAssertMessage(self, *args):
        if self.__gdb is None:
            return Result(None, "", "")

        argv = ["gdb", "--batch"]
        argv.extend(args)
        print("Running:", argv)

        env = os.environ.copy()
        env["LC_ALL"] = "C.UTF-8"
        print("Environment:", env)

        # We want to ensure consistent line endings...
        info = subprocess.run(
            argv,
            timeout=self.timeout_seconds,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=env,
            universal_newlines=True,
        )
        out = info.stdout.strip()
        err = info.stderr.strip()

        result = Result(info, out, err)

        print("Output:", result.out)
        print("Error:", result.err)
        print(result.info)
        return result

    def test_gassert(self):
        """Test running g_assert() and fail the program."""
        result = self.runAssertMessage()

        if os.name == "nt":
            self.assertEqual(result.info.returncode, 3)
        else:
            self.assertEqual(result.info.returncode, -6)
        self.assertIn("assertion failed: (42 < 0)", result.out)

    def test_gdb_gassert(self):
        """Test running g_assert() within gdb and fail the program."""
        if self.__gdb is None:
            self.skipTest("GDB is not installed, skipping this test!")

        with tempfile.NamedTemporaryFile(
            prefix="assert-msg-test-", suffix=".gdb", mode="w", delete=False
        ) as tmp:
            try:
                tmp.write(GDB_SCRIPT)
                tmp.close()
                result = self.runGdbAssertMessage("-x", tmp.name, self.__assert_msg_test)
            finally:
                os.unlink(tmp.name)

            # Some CI environments disable ptrace (as they’re running in a
            # container). If so, skip the test as there’s nothing we can do.
            if (
                result.info.returncode != 0
                and "ptrace: Operation not permitted" in result.err
            ):
                self.skipTest("GDB is not functional due to ptrace being disabled")

            self.assertEqual(result.info.returncode, 0)
            self.assertIn("$1 = 0x", result.out)
            self.assertIn("assertion failed: (42 < 0)", result.out)


if __name__ == "__main__":
    unittest.main(testRunner=taptestrunner.TAPTestRunner())
