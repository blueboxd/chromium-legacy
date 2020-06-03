#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for dependency_analysis.print_dependencies_helper."""

import unittest
import print_dependencies_helper


class TestHelperFunctions(unittest.TestCase):
    """Unit tests for the helper functions in the module."""
    def test_get_valid_keys_matching_input(self):
        """Tests getting all valid keys for the given input."""
        test_keys = ['o.c.test.Test', 'o.c.testing.Test', 'o.c.test.Wrong']
        valid_keys = print_dependencies_helper.get_valid_keys_matching_input(
            test_keys, 'test')
        self.assertEqual(valid_keys,
                         sorted(['o.c.test.Test', 'o.c.testing.Test']))

    def test_get_valid_keys_matching_input_no_match(self):
        """Tests getting all valid keys when there is no matching key."""
        test_keys = ['o.c.test.Test', 'o.c.testing.Test', 'o.c.test.Wrong']
        valid_keys = print_dependencies_helper.get_valid_keys_matching_input(
            test_keys, 'nomatch')
        self.assertEqual(valid_keys, [])
