# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from css_generator import CSSStyleGenerator
from base_generator import Modes
import unittest


class CSSStyleGeneratorTest(unittest.TestCase):
    def setUp(self):
        self.generator = CSSStyleGenerator()
        self.generator.AddJSONFileToModel('colors_test_palette.json5')
        self.generator.AddJSONFileToModel('colors_test.json5')

    def assertEqualToFile(self, value, filename):
        with open(filename) as f:
            contents = f.read()
            self.assertEqual(
                value, contents,
                '\n>>>>>\n%s<<<<<\n\ndoes not match\n\n>>>>>\n%s<<<<<' %
                (value, contents))

    def testColorTestJSON(self):
        self.assertEqualToFile(self.generator.Render(),
                               'colors_test_expected.css')

    def testColorTestJSONDarkOnly(self):
        self.generator.generate_single_mode = Modes.DARK
        self.assertEqualToFile(self.generator.Render(),
                               'colors_test_dark_only_expected.css')


if __name__ == '__main__':
    unittest.main()
