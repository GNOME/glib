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

GIR_ENV_VARIABLE = "_G_TEST_GI_COMPILE_REPOSITORY_GIR"
GIR_INCLUDE_DIR_ENV_VARIABLE = "_G_TEST_GI_COMPILE_REPOSITORY_INCLUDE_DIR"


class TestGICompileRepositoryBase(testprogramrunner.TestProgramRunner):
    """Integration test base class for checking gi-compile-repository behavior"""

    PROGRAM_NAME = "gi-compile-repository"
    PROGRAM_TYPE = testprogramrunner.ProgramType.NATIVE


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


class TestGICompileRepositoryForGir(TestGICompileRepositoryBase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()

        cls._gir_path = os.getenv(GIR_ENV_VARIABLE)
        cls._include_dir = os.getenv(GIR_INCLUDE_DIR_ENV_VARIABLE)

    def setUp(self):
        if not self._gir_path:
            self.skipTest(f"The test is only valid when {GIR_ENV_VARIABLE} is set")

        super().setUp()

    def runTestProgram(self, *args, **kwargs):
        argv = [self._gir_path]
        argv.extend(*args)

        if self._include_dir:
            argv.extend(["--includedir", self._include_dir])

        return super().runTestProgram(argv, **kwargs)

    def test_write_failure(self):
        typelib_path = "this-is/not/a-good-output/invalid.typelib"
        argv = ["--output", typelib_path]

        result = self.runTestProgram(argv, should_fail=True)

        self.assertEqual(result.info.returncode, 1)
        self.assertIn(f"Failed to open ‘{typelib_path}.tmp’", result.err)

    def test_compile(self):
        typelib_name = os.path.splitext(os.path.basename(self._gir_path))[0]
        typelib_path = os.path.join(self.tmpdir.name, f"{typelib_name}.typelib")
        argv = ["--output", typelib_path]

        result = self.runTestProgram(argv)

        self.assertFalse(result.out)
        self.assertFalse(result.err)
        self.assertTrue(os.path.exists(typelib_path))


if __name__ == "__main__":
    unittest.main(testRunner=taptestrunner.TAPTestRunner())
