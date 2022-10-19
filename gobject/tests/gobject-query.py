#!/usr/bin/python3
# -*- coding: utf-8 -*-
#
# Copyright © 2022 Endless OS Foundation, LLC
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

"""Integration tests for gobject-query utility."""

import collections
import os
import shutil
import subprocess
import unittest

import taptestrunner


Result = collections.namedtuple("Result", ("info", "out", "err"))


class TestGobjectQuery(unittest.TestCase):
    """Integration test for running gobject-query.

    This can be run when installed or uninstalled. When uninstalled, it
    requires G_TEST_BUILDDIR and G_TEST_SRCDIR to be set.

    The idea with this test harness is to test the gobject-query utility, its
    handling of command line arguments, and its exit statuses.
    """

    def setUp(self):
        self.timeout_seconds = 10  # seconds per test
        if "G_TEST_BUILDDIR" in os.environ:
            self.__gobject_query = os.path.join(
                os.environ["G_TEST_BUILDDIR"], "..", "gobject-query"
            )
        else:
            self.__gobject_query = shutil.which("gobject-query")
        print("gobject-query:", self.__gobject_query)

    def runGobjectQuery(self, *args):
        argv = [self.__gobject_query]
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
            text=True,
            encoding="utf-8",
        )
        info.check_returncode()
        out = info.stdout.strip()
        err = info.stderr.strip()

        result = Result(info, out, err)

        print("Output:", result.out)
        return result

    def test_help(self):
        """Test the --help argument."""
        result = self.runGobjectQuery("--help")
        self.assertIn("usage: gobject-query", result.out)

    def test_version(self):
        """Test the --version argument."""
        result = self.runGobjectQuery("--version")
        self.assertIn("2.", result.out)

    def test_froots(self):
        """Test running froots with no other arguments."""
        result = self.runGobjectQuery("froots")

        self.assertEqual("", result.err)
        self.assertIn("├gboolean", result.out)
        self.assertIn("├GObject", result.out)

    def test_tree(self):
        """Test running tree with no other arguments."""
        result = self.runGobjectQuery("tree")

        self.assertEqual("", result.err)
        self.assertIn("GObject", result.out)


if __name__ == "__main__":
    unittest.main(testRunner=taptestrunner.TAPTestRunner())
