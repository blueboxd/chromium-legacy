# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from core.results_processor import util


class UtilTests(unittest.TestCase):
  def testApplyInParallel(self):
    work_list = [1, 2, 3]
    fun = lambda x: x * x
    result = set(util.ApplyInParallel(fun, work_list))
    self.assertEqual(result, set([1, 4, 9]))
