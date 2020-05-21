#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for dependency_analysis.process_jdeps."""

import unittest
import process_jdeps


class TestHelperFunctions(unittest.TestCase):
    """Unit tests for module-level helper functions."""
    def test_class_is_interesting(self):
        """Tests that the helper identifies a valid Chromium class name."""
        self.assertTrue(
            process_jdeps.class_is_interesting(
                'org.chromium.chrome.browser.Foo'))

    def test_class_is_interesting_longer(self):
        """Tests that the helper identifies a valid Chromium class name."""
        self.assertTrue(
            process_jdeps.class_is_interesting(
                'org.chromium.chrome.browser.foo.Bar'))

    def test_class_is_interesting_negative(self):
        """Tests that the helper ignores a non-Chromium class name."""
        self.assertFalse(
            process_jdeps.class_is_interesting(
                'org.notchromium.chrome.browser.Foo'))

    def test_class_is_interesting_not_interesting(self):
        """Tests that the helper ignores a builtin class name."""
        self.assertFalse(
            process_jdeps.class_is_interesting('java.lang.Object'))


class TestJavaClassJdepsParser(unittest.TestCase):
    """Unit tests for
        dependency_analysis.class_dependency.JavaClassJdepsParser.
    """
    def setUp(self):
        """Sets up a new JavaClassJdepsParser."""
        self.parser = process_jdeps.JavaClassJdepsParser()

    def test_parse_line(self):
        """Tests that new nodes + edges are added after a successful parse."""
        self.parser.parse_line(
            'org.chromium.a -> org.chromium.b org.chromium.c')
        self.assertEqual(self.parser.graph.num_nodes, 2)
        self.assertEqual(self.parser.graph.num_edges, 1)

    def test_parse_line_not_interesting(self):
        """Tests that nothing is changed if there is an uninteresting class."""
        self.parser.parse_line('org.chromium.a -> b c')
        self.assertEqual(self.parser.graph.num_nodes, 0)
        self.assertEqual(self.parser.graph.num_edges, 0)

    def test_parse_line_too_short(self):
        """Tests that nothing is changed if the line is too short."""
        self.parser.parse_line('org.chromium.a -> b')
        self.assertEqual(self.parser.graph.num_nodes, 0)
        self.assertEqual(self.parser.graph.num_edges, 0)

    def test_parse_line_not_found(self):
        """Tests that nothing is changed if the line contains `not found`
            as the second class.
        """
        self.parser.parse_line('org.chromium.a -> not found')
        self.assertEqual(self.parser.graph.num_nodes, 0)
        self.assertEqual(self.parser.graph.num_edges, 0)

    def test_parse_line_empty_string(self):
        """Tests that nothing is changed if the line is empty."""
        self.parser.parse_line('')
        self.assertEqual(self.parser.graph.num_nodes, 0)
        self.assertEqual(self.parser.graph.num_edges, 0)

    def test_parse_line_bad_input(self):
        """Tests that nothing is changed if the line is nonsensical"""
        self.parser.parse_line('bad_input')
        self.assertEqual(self.parser.graph.num_nodes, 0)
        self.assertEqual(self.parser.graph.num_edges, 0)


if __name__ == '__main__':
    unittest.main()
