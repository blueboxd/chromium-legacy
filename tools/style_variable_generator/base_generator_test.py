# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from base_generator import BaseGenerator
import unittest


class BaseGeneratorTest(unittest.TestCase):
    def setUp(self):
        self.generator = BaseGenerator()

    def testMissingColor(self):
        # google_grey_900 is missing.
        self.generator.AddJSONToModel('''
{
  colors: {
    cros_default_text_color: {
      light: "$google_grey_900",
      dark: "rgb(255, 255, 255)",
    },
  },
}
        ''')
        self.assertRaises(ValueError, self.generator.Validate)

        # Add google_grey_900.
        self.generator.AddJSONToModel('''
{
  colors: {
    google_grey_900: "rgb(255, 255, 255)",
  }
}
        ''')
        self.generator.Validate()

    def testMissingDefaultModeColor(self):
        # google_grey_900 is missing in the default mode (light).
        self.generator.AddJSONToModel('''
{
  colors: {
    google_grey_900: { dark: "rgb(255, 255, 255)", },
  }
}
        ''')
        self.assertRaises(ValueError, self.generator.Validate)

    def testDuplicateKeys(self):
        self.generator.AddJSONToModel('''
{
  colors: {
    google_grey_900: { light: "rgb(255, 255, 255)", },
  }
}
        ''')
        self.generator.Validate()

        # Add google_grey_900's dark mode as if in a separate file. This counts
        # as a redefinition/conflict and causes an error.
        self.assertRaises(
            ValueError, self.generator.AddJSONToModel, '''
{
  colors: {
    google_grey_900: { dark: "rgb(255, 255, 255)", }
  }
}
        ''')


if __name__ == '__main__':
    unittest.main()
