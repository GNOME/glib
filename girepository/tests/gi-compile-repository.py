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

""" Integration tests for gi-compile-repository. """

import os
import unittest

import taptestrunner
import testprogramrunner


class TestGICompileRepositoryBase(testprogramrunner.TestProgramRunner):
    """Integration test for checking gi-compile-repository behavior"""

    PROGRAM_NAME = "gi-compile-repository"
    PROGRAM_TYPE = testprogramrunner.ProgramType.NATIVE

    def test_open_failure(self):
        gir_path = "this-is/not/a-file.gir"
        result = self.runTestProgram(
            [gir_path, "--output", os.path.join(self.tmpdir.name, "invalid.typelib")],
            should_fail=True,
        )

        self.assertEqual(result.info.returncode, 1)
        self.assertIn(f"Error parsing file ‘{gir_path}’", result.err)

    def test_write_failure(self):
        typelib_path = "this-is/not/a-good-output/invalid.typelib"
        glib_gir = os.environ["_G_TEST_GI_COMPILE_REPOSITORY_GLIB_GIR"]
        result = self.runTestProgram(
            [glib_gir, "--output", typelib_path],
            should_fail=True,
        )

        self.assertEqual(result.info.returncode, 1)
        self.assertIn(f"Failed to open ‘{typelib_path}.tmp’", result.err)


if __name__ == "__main__":
    unittest.main(testRunner=taptestrunner.TAPTestRunner())
