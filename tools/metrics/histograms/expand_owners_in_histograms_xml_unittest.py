# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import expand_owners
import os
import xml.dom.minidom


class ExpandOwnersInHistogramsXmlTest(unittest.TestCase):

  def testExpandOwners(self):
    """Checks that no errors are raised when expanding OWNERS files."""
    path = os.path.join(os.path.dirname(__file__), 'histograms.xml')
    with open(path, 'r') as histograms_file:
      document = xml.dom.minidom.parse(histograms_file)
      histograms = document.getElementsByTagName('histograms')[0]

      num_histograms= len(document.getElementsByTagName('histogram'))

      expand_owners.ExpandHistogramsOWNERS(histograms)

      # Checking the number of histograms before and after the call to
      # ExpandHistogramsOWNERS is a trivial assertion that is done in lieu of
      # not having any assertion at all.
      #
      # The goal is to verify that no errors are raised when calling this
      # function, which is tested in expand_owners_unittest.py.
      self.assertEqual(len(document.getElementsByTagName('histogram')),
                       num_histograms)


if __name__ == '__main__':
    unittest.main()
