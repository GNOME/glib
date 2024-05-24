#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright © 2025 Marco Trevisan <mail@3v1n0.net>
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

""" Integration tests for gi-compile-repository. """

import os
import unittest

import taptestrunner
import testprogramrunner


class TestGICompileRepositoryBase(testprogramrunner.TestProgramRunner):
    """Integration test base class for checking gi-compile-repository behavior"""

    PROGRAM_NAME = "gi-compile-repository"
    PROGRAM_TYPE = testprogramrunner.ProgramType.NATIVE

    @classmethod
    def setUpClass(cls):
        super().setUpClass()

        if "G_TEST_BUILDDIR" in os.environ:
            cls._gir_path = os.path.join(
                os.environ["G_TEST_BUILDDIR"], "..", "introspection"
            )
        else:
            cls._gir_path = os.path.join(
                os.path.dirname(os.path.realpath(__file__)),
                "..",
                "..",
                "..",
                "share",
                "gir-1.0",
            )
        print(f"gir path set to {cls._gir_path}")


class TestGICompileRepository(TestGICompileRepositoryBase):
    """Integration test for checking gi-compile-repository behavior"""

    def test_open_failure(self):
        gir_path = "this-is/not/a-file.gir"
        result = self.runTestProgram(
            [gir_path, "--output", os.path.join(self.tmpdir.name, "invalid.typelib")],
            should_fail=True,
        )

        self.assertEqual(result.info.returncode, 1)
        self.assertIn(f"Error parsing file ‘{gir_path}’", result.err)


class TestGICompileRepositoryForGLib(TestGICompileRepositoryBase):
    GIR_NAME = "GLib-2.0"

    def runTestProgram(self, *args, **kwargs):
        gir_file = os.path.join(self._gir_path, f"{self.GIR_NAME}.gir")
        self.assertTrue(os.path.exists(gir_file))
        argv = [gir_file]
        argv.extend(*args)

        if self.GIR_NAME != "GLib-2.0":
            argv.extend(["--includedir", self._gir_path])

        return super().runTestProgram(argv, **kwargs)

    def test_write_failure(self):
        typelib_path = "this-is/not/a-good-output/invalid.typelib"
        result = self.runTestProgram(
            ["--output", typelib_path],
            should_fail=True,
        )

        self.assertEqual(result.info.returncode, 1)
        self.assertIn(f"Failed to open ‘{typelib_path}.tmp’", result.err)

    def test_compile(self):
        typelib_name = os.path.splitext(self.GIR_NAME)[0]
        typelib_path = os.path.join(self.tmpdir.name, f"{typelib_name}.typelib")
        argv = ["--output", typelib_path]

        result = self.runTestProgram(argv)

        self.assertFalse(result.out)
        self.assertFalse(result.err)
        self.assertTrue(os.path.exists(typelib_path))


class TestGICompileRepositoryForGObject(TestGICompileRepositoryForGLib):
    GIR_NAME = "GObject-2.0"


class TestGICompileRepositoryForGio(TestGICompileRepositoryForGLib):
    GIR_NAME = "Gio-2.0"


if __name__ == "__main__":
    unittest.main(testRunner=taptestrunner.TAPTestRunner())
