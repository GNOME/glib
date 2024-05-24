#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright © 2025 Marco Trevisan <mail@3v1n0.net>
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

""" Integration tests for gi-inspect-typelib. """

import os
import unittest

import taptestrunner
import testprogramrunner

TYPELIB_NAMESPACE_ENV_VARIABLE = "_G_TEST_GI_INSPECT_TYPELIB_NAMESPACE"
TYPELIB_VERSION_ENV_VARIABLE = "_G_TEST_GI_INSPECT_TYPELIB_VERSION"


class TestGIInspectTypelibBase(testprogramrunner.TestProgramRunner):
    """Integration test base class for checking gi-inspect-typelib behavior"""

    PROGRAM_NAME = "gi-inspect-typelib"
    PROGRAM_TYPE = testprogramrunner.ProgramType.NATIVE

    def runTestProgram(self, *args, **kwargs):
        return super().runTestProgram(args, **kwargs)


class TestGIInspectTypelibCommandLine(TestGIInspectTypelibBase):
    def test_help(self):
        """Test the --help argument."""
        result = self.runTestProgram("--help")
        self.assertIn("Usage:", result.out)
        self.assertIn(
            "gi-inspect-typelib [OPTION…] NAMESPACE - Inspect GI typelib", result.out
        )
        self.assertIn("--typelib-version=VERSION", result.out)
        self.assertIn("--print-shlibs", result.out)
        self.assertIn("--print-typelibs", result.out)

    def test_no_args(self):
        """Test running with no arguments at all."""
        result = self.runTestProgram(should_fail=True)
        self.assertEqual("Please specify exactly one namespace", result.err)

    def test_invalid_typelib(self):
        res = self.runTestProgram(
            "--print-typelibs", "--print-shlibs", "AnInvalidNameSpace", should_fail=True
        )
        self.assertFalse(res.out)
        self.assertIn(
            "Typelib file for namespace 'AnInvalidNameSpace' (any version) not found",
            res.err,
        )


class TestGICompileRepositoryForTypelib(TestGIInspectTypelibBase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()

        cls._typelib_namespace = os.getenv(TYPELIB_NAMESPACE_ENV_VARIABLE)
        cls._typelib_version = os.getenv(TYPELIB_VERSION_ENV_VARIABLE)

    def setUp(self):
        if not self._typelib_namespace:
            self.skipTest(
                f"The test is only valid when {TYPELIB_NAMESPACE_ENV_VARIABLE} is set"
            )

        super().setUp()

    def runTestProgram(self, *args, **kwargs):
        argv = list(args)
        argv.append(self._typelib_namespace)

        print("ARGV ARE", argv)

        if self._typelib_version:
            argv.append(f"--typelib-version={self._typelib_version}")

        return super().runTestProgram(*argv, **kwargs)

    def check_shlib(self, out):
        ext = ".so.0"
        if os.name == "nt":
            ext = "-0.dll"

        if self._typelib_namespace.startswith("GLib"):
            self.assertIn(f"libglib-2.0{ext}", out)
        elif self._typelib_namespace.startswith("Gio"):
            self.assertIn(f"libgio-2.0{ext}", out)
        elif self._typelib_namespace.startswith("GObject"):
            self.assertIn(f"libgobject-2.0{ext}", out)
        elif self._typelib_namespace.startswith("GModule"):
            self.assertIn(f"libgmodule-2.0{ext}", out)

    def check_typelib_deps(self, out):
        if self._typelib_namespace == "GLib":
            self.assertNotIn("GLib-2.0", out)
            self.assertNotIn("GModule-2.0", out)
            self.assertNotIn("GObject-2.0", out)
            self.assertNotIn("Gio-2.0", out)
        elif self._typelib_namespace == "GObject":
            self.assertIn("GLib-2.0", out)
            self.assertNotIn("GModule-2.0", out)
            self.assertNotIn("GObject-2.0", out)
            self.assertNotIn("Gio-2.0", out)
        elif self._typelib_namespace == "GModule":
            self.assertIn("GLib-2.0", out)
            self.assertNotIn("GObject-2.0", out)
            self.assertNotIn("Gio-2.0", out)
        elif self._typelib_namespace.startswith("Gio"):
            self.assertIn("GLib-2.0", out)
            self.assertIn("GObject-2.0", out)
            self.assertIn("GModule-2.0", out)
            if len(self._typelib_namespace) > len("Gio"):
                self.assertIn("Gio-2.0", out)
            else:
                self.assertNotIn("Gio-2.0", out)

    def test_print_typelibs(self):
        res = self.runTestProgram("--print-typelibs")
        self.assertFalse(res.err)
        if self._typelib_namespace == "GLib":
            self.assertFalse(res.out)
        self.check_typelib_deps(res.out)

    def test_print_shlibs(self):
        res = self.runTestProgram("--print-shlibs")
        self.assertFalse(res.err)
        self.check_shlib(res.out)
        self.assertNotIn("GLib-2.0", res.out)

    def test_print_typelibs_and_shlibs(self):
        res = self.runTestProgram("--print-typelibs", "--print-shlibs")
        self.assertFalse(res.err)
        self.check_shlib(res.out)


if __name__ == "__main__":
    unittest.main(testRunner=taptestrunner.TAPTestRunner())
