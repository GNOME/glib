#!/usr/bin/python3
# -*- coding: utf-8 -*-
#
# Copyright © 2018, 2019 Endless Mobile, Inc.
# Copyright © 2023 Philip Withnall
#
# SPDX-License-Identifier: LGPL-2.1-or-later
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

"""Integration tests for the gio utility."""

import subprocess
import sys
import tempfile
import unittest

import taptestrunner
import testprogramrunner


class TestGioTool(testprogramrunner.TestProgramRunner):
    """Integration test for running the gio tool.

    This can be run when installed or uninstalled. When uninstalled, it
    requires G_TEST_BUILDDIR and G_TEST_SRCDIR to be set.

    The idea with this test harness is to test the gio utility, its
    handling of command line arguments, its exit statuses, and its actual
    effects on the file system.
    """

    PROGRAM_NAME = "gio"
    PROGRAM_TYPE = testprogramrunner.ProgramType.NATIVE

    def runGio(self, *args):
        return self.runTestProgram(args, timeout_seconds=6)

    def test_help(self):
        """Test the --help argument and help subcommand."""
        result = self.runGio("--help")
        result2 = self.runGio("help")

        self.assertEqual(result.out, result2.out)
        self.assertEqual(result.err, result2.err)

        self.assertIn("Usage:\n  gio COMMAND", result.out)
        self.assertIn("List the contents of locations", result.out)

    def test_no_args(self):
        """Test running with no arguments at all."""
        with self.assertRaises(subprocess.CalledProcessError):
            self.runGio()

    def test_info_non_default_attributes(self):
        """Test running `gio info --attributes` with a non-default list."""
        with tempfile.NamedTemporaryFile(dir=self.tmpdir.name) as tmpfile:
            result = self.runGio(
                "info", "--attributes=standard::content-type", tmpfile.name
            )
            if sys.platform == "darwin":
                self.assertIn("standard::content-type: public.text", result.out)
            else:
                self.assertIn(
                    "standard::content-type: application/x-zerosize", result.out
                )


if __name__ == "__main__":
    unittest.main(testRunner=taptestrunner.TAPTestRunner())
