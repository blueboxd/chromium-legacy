# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import OrderedDict
import os
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.port.factory_mock import MockPortFactory
from blinkpy.w3c.wpt_metadata_builder import WPTMetadataBuilder


def _make_expectation(port, test_name, test_statuses):
    """Creates an expectation object for a single test.

    Args:
        port: the port to run against
        test_name: the name of the test
        test_status: the statuses of the test

    Returns:
        An expectation object with the given test and statuses.
    """
    expectation_dict = OrderedDict()
    expectation_dict["expectations"] = "Bug(test) %s [ %s ]" % (test_name, test_statuses)
    return TestExpectations(port, tests=[test_name], expectations_dict=expectation_dict)


class WPTMetadataBuilderTest(unittest.TestCase):

    def setUp(self):
        self.num = 2
        self.host = MockHost()
        self.host.port_factory = MockPortFactory(self.host)
        self.port = self.host.port_factory.get()

    def test_skipped_test(self):
        """A skipped WPT test should get a test-specific metadata file."""
        test_name = "external/wpt/test.html"
        expectations = _make_expectation(self.port, test_name, "SKIP")
        metadata_builder = WPTMetadataBuilder(expectations)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name)
        self.assertEqual("test.html.ini", filename)
        self.assertEqual("[test.html]\n  disabled: build_wpt_metadata.py", contents)

    def test_skipped_test_with_variants(self):
        """A skipped WPT tests with variants should get a test-specific metadata file."""
        test_name = "external/wpt/test.html?foo=bar"
        expectations = _make_expectation(self.port, test_name, "SKIP")
        metadata_builder = WPTMetadataBuilder(expectations)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name)
        # The metadata file name should not include variants
        self.assertEqual("test.html.ini", filename)
        # ..but the contents of the file should include variants in the test name
        self.assertEqual("[test.html?foo=bar]\n  disabled: build_wpt_metadata.py", contents)

    def test_skipped_directory(self):
        """A skipped WPT directory should get a dir-wide metadata file."""
        test_name = "external/wpt/test_dir/"
        expectations = _make_expectation(self.port, test_name, "SKIP")
        metadata_builder = WPTMetadataBuilder(expectations)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name)
        self.assertEqual(os.path.join("test_dir", "__dir__.ini"), filename)
        self.assertEqual("disabled: build_wpt_metadata.py", contents)

    def test_non_wpt_test(self):
        """A non-WPT test should not get any metadata."""
        test_name = "some/other/test.html"
        expectations = _make_expectation(self.port, test_name, "SKIP")
        metadata_builder = WPTMetadataBuilder(expectations)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name)
        self.assertIsNone(filename)
        self.assertIsNone(contents)

    def test_wpt_test_not_skipped(self):
        """A WPT test that is not skipped should not get any metadata."""
        test_name = "external/wpt/test.html"
        expectations = _make_expectation(self.port, test_name, "TIMEOUT")
        metadata_builder = WPTMetadataBuilder(expectations)
        test_names = metadata_builder.get_test_names_for_metadata()
        self.assertFalse(test_names)
