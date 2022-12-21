#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright © 2022 Emmanuel Fleury <emmanuel.fleury@gmail.com>
# Copyright © 2022 Marco Trevisan <mail@3v1n0.net>
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

""" Integration tests for g_message functions on low-memory. """

import collections
import os
import subprocess
import unittest

import taptestrunner

Result = collections.namedtuple("Result", ("info", "out", "err"))


class TestMessagesLowMemory(unittest.TestCase):
    """Integration test for checking g_message()’s behavior on low memory.

    This can be run when installed or uninstalled. When uninstalled,
    it requires G_TEST_BUILDDIR and G_TEST_SRCDIR to be set.

    The idea with this test harness is to test if g_message and friends
    assert instead of crashing if memory is exhausted, printing the expected
    error message.
    """

    test_binary = "messages-low-memory"

    def setUp(self):
        ext = ""
        if os.name == "nt":
            ext = ".exe"
        if "G_TEST_BUILDDIR" in os.environ:
            self._test_binary = os.path.join(
                os.environ["G_TEST_BUILDDIR"], self.test_binary + ext
            )
        else:
            self._test_binary = os.path.join(
                os.path.dirname(__file__), self.test_binary + ext
            )
        print("messages-low-memory:", self._test_binary)

    def runTestBinary(self, *args):
        print("Running:", *args)

        env = os.environ.copy()
        env["LC_ALL"] = "C.UTF-8"
        print("Environment:", env)

        # We want to ensure consistent line endings...
        info = subprocess.run(
            *args,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=env,
            universal_newlines=True,
        )
        out = info.stdout.strip()
        err = info.stderr.strip()

        result = Result(info, out, err)

        print("Return code:", result.info.returncode)
        print("Output:", result.out)
        print("Error:", result.err)
        return result

    def test_message_memory_allocation_failure(self):
        """Test running g_message() when memory is exhausted."""
        result = self.runTestBinary(self._test_binary)

        if result.info.returncode == 77:
            self.skipTest("Not supported")

        if os.name == "nt":
            self.assertEqual(result.info.returncode, 3)
        else:
            self.assertEqual(result.info.returncode, -6)
        self.assertIn("failed to allocate memory", result.err)


if __name__ == "__main__":
    unittest.main(testRunner=taptestrunner.TAPTestRunner())
