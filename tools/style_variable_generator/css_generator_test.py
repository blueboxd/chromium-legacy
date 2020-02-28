# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from css_generator import CSSStyleGenerator
import unittest


class CSSStyleGeneratorTest(unittest.TestCase):
    def setUp(self):
        self.generator = CSSStyleGenerator()

    def assertEqualToFile(self, value, filename):
        with open(filename) as f:
            self.assertEqual(value, f.read())

    def testColorTestJSON(self):
        self.generator.AddJSONFileToModel('colors_test.json5')
        self.assertEqualToFile(self.generator.Render(),
                               'colors_test_expected.css')


if __name__ == '__main__':
    unittest.main()
