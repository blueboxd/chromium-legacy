# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from views_generator import ViewsStyleGenerator
import unittest


class ViewsStyleGeneratorTest(unittest.TestCase):
    def setUp(self):
        self.generator = ViewsStyleGenerator()

    def assertEqualToFile(self, value, filename):
        with open(filename) as f:
            contents = f.read()
            self.assertEqual(
                value, contents,
                '\n>>>>>\n%s<<<<<\n\ndoes not match\n\n>>>>>\n%s<<<<<' %
                (value, contents))

    def testColorTestJSON(self):
        self.generator.AddJSONFileToModel('colors_test.json5')
        self.generator.out_file_path = (
            'tools/style_variable_generator/colors_test_expected.h')
        self.assertEqualToFile(self.generator.Render(),
                               'colors_test_expected.h')


if __name__ == '__main__':
    unittest.main()
