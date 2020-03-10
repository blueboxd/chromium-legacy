# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import OrderedDict
import json
import os
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.port.factory_mock import MockPortFactory
from blinkpy.w3c.wpt_manifest import BASE_MANIFEST_NAME
from blinkpy.w3c.wpt_metadata_builder import (
    WPTMetadataBuilder,
    HARNESS_ERROR,
    SKIP_TEST,
    SUBTEST_FAIL,
    TEST_FAIL,
    TEST_PASS,
    TEST_PRECONDITION_FAILED,
)


def _make_expectation(port, test_path, test_statuses, test_names=[], trailing_comments=""):
    """Creates an expectation object for a single test or directory.

    Args:
        port: the port to run against
        test_path: the path to set expectations for
        test_status: the statuses of the test
        test_names: a set of tests under the 'test_path'. Should be non-empty
            when the 'test_path' is a directory instead of a single test
        trailing_comments: comments at the end of the expectation line.

    Returns:
        An expectation object with the given test and statuses.
    """
    expectation_dict = OrderedDict()
    expectation_dict["expectations"] = (
        "# results: [ %s ]\n%s [ %s ]%s" % (test_statuses, test_path, test_statuses, trailing_comments))

    # When test_path is a dir, we expect test_names to be provided.
    is_dir = test_path.endswith('/')
    assert is_dir == bool(test_names)
    if not is_dir:
        test_names = [test_path]

    return TestExpectations(port, expectations_dict=expectation_dict)


class WPTMetadataBuilderTest(unittest.TestCase):

    def setUp(self):
        self.num = 2
        self.host = MockHost()
        self.host.port_factory = MockPortFactory(self.host)
        self.port = self.host.port_factory.get()

        # Write a dummy manifest file, describing what tests exist.
        self.host.filesystem.write_text_file(
            self.port.web_tests_dir() + '/external/' + BASE_MANIFEST_NAME,
            json.dumps({
                'items': {
                    'reftest': {
                        'reftest.html': [
                            ['reftest.html', [['reftest-ref.html', '==']], {}],
                        ]
                    },
                    'testharness': {
                        'test.html': [['test.html', {}]],
                        'variant.html': [
                            ['variant.html?foo=bar/abc', {}],
                            ['variant.html?foo=baz', {}],
                        ],
                        'dir/zzzz.html': [['dir/zzzz.html', {}]],
                        'dir/multiglob.https.any.js': [
                            ['dir/multiglob.https.any.window.html', {}],
                            ['dir/multiglob.https.any.worker.html', {}],
                        ],
                    },
                    'manual': {
                        'x-manual.html': [['x-manual.html', {}]],
                    },
                },
            }))

    def test_non_wpt_test(self):
        """A non-WPT test should not get any metadata."""
        test_name = "some/other/test.html"
        expectations = _make_expectation(self.port, test_name, "SKIP")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, SKIP_TEST)
        self.assertIsNone(filename)
        self.assertIsNone(contents)

    def test_wpt_test_without_manifest_entry(self):
        """A WPT test that is not in the manifest should not get a baseline."""
        test_name = "external/wpt/test-not-in-manifest.html"
        expectations = _make_expectation(self.port, test_name, "SKIP")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, SKIP_TEST)
        self.assertIsNone(filename)
        self.assertIsNone(contents)

    def test_wpt_test_not_skipped(self):
        """A WPT test that is not skipped should not get a SKIP metadata."""
        test_name = "external/wpt/test.html"
        expectations = _make_expectation(self.port, test_name, "TIMEOUT")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_names = metadata_builder.get_tests_needing_metadata()
        # The test will appear in the result but won't have a SKIP status
        found = False
        for name_item, status_item in test_names.items():
            if name_item == test_name:
                found = True
                self.assertNotEqual(SKIP_TEST, status_item)
        self.assertTrue(found)

    def test_parse_baseline_all_pass(self):
        """A WPT test with an all-pass baseline doesn't get metadata."""
        # Here we use a test_name that is actually in the test manifest
        test_name = "external/wpt/dir/zzzz.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = self.port.expected_filename(test_name, '.txt')
        self.host.filesystem.write_text_file(
            baseline_filename,
            "This is a test\nPASS some subtest\nPASS another subtest\n")
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertFalse(test_and_status_dict)

    def test_parse_baseline_subtest_fail(self):
        """Test parsing a baseline with a failing subtest."""
        # Here we use a test_name that is actually in the test manifest
        test_name = "external/wpt/dir/zzzz.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = self.port.expected_filename(test_name, '.txt')
        self.host.filesystem.write_text_file(
            baseline_filename,
            "This is a test\nPASS some subtest\nFAIL another subtest\n")
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(SUBTEST_FAIL, test_and_status_dict[test_name])

    def test_parse_baseline_subtest_notrun(self):
        """Test parsing a baseline with a notrun subtest."""
        # Here we use a test_name that is actually in the test manifest
        test_name = "external/wpt/dir/zzzz.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = self.port.expected_filename(test_name, '.txt')
        self.host.filesystem.write_text_file(
            baseline_filename,
            "This is a test\nPASS some subtest\nNOTRUN another subtest\n")
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(SUBTEST_FAIL, test_and_status_dict[test_name])

    def test_parse_baseline_subtest_timeout(self):
        """Test parsing a baseline with a timeout subtest."""
        # Here we use a test_name that is actually in the test manifest
        test_name = "external/wpt/dir/zzzz.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = self.port.expected_filename(test_name, '.txt')
        self.host.filesystem.write_text_file(
            baseline_filename,
            "This is a test\nTIMEOUT some subtest\nPASS another subtest\n")
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(SUBTEST_FAIL, test_and_status_dict[test_name])

    def test_parse_baseline_harness_error(self):
        """Test parsing a baseline with a harness error."""
        # Here we use a test_name that is actually in the test manifest
        test_name = "external/wpt/dir/zzzz.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = self.port.expected_filename(test_name, '.txt')
        self.host.filesystem.write_text_file(
            baseline_filename,
            "This is a test\nHarness Error. some stuff\n")
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(HARNESS_ERROR, test_and_status_dict[test_name])

    def test_parse_baseline_subtest_fail_and_harness_error(self):
        """Test parsing a baseline with a harness error AND a subtest fail."""
        # Here we use a test_name that is actually in the test manifest
        test_name = "external/wpt/dir/zzzz.html"
        # Manually initialize the baseline file and its contents
        baseline_filename = self.port.expected_filename(test_name, '.txt')
        self.host.filesystem.write_text_file(
            baseline_filename,
            "This is a test\nHarness Error. some stuff\nPASS some subtest\nFAIL another subtest\n")
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(SUBTEST_FAIL | HARNESS_ERROR, test_and_status_dict[test_name])

    def test_metadata_for_flaky_test(self):
        """A WPT test that is flaky has multiple statuses in metadata."""
        test_name = "external/wpt/test.html"
        expectations = _make_expectation(self.port, test_name, "PASS FAILURE")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, TEST_PASS | TEST_FAIL)
        self.assertEqual("test.html.ini", filename)
        # The PASS and FAIL expectations fan out to also include OK and ERROR
        # to support reftest/testharness test differences.
        self.assertEqual("[test.html]\n  expected: [PASS, OK, FAIL, ERROR]\n", contents)

    def test_metadata_for_skipped_test(self):
        """A skipped WPT test should get a test-specific metadata file."""
        test_name = "external/wpt/test.html"
        expectations = _make_expectation(self.port, test_name, "SKIP")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, SKIP_TEST)
        self.assertEqual("test.html.ini", filename)
        self.assertEqual("[test.html]\n  disabled: wpt_metadata_builder.py\n", contents)

    def test_metadata_for_skipped_test_with_variants(self):
        """A skipped WPT tests with variants should get a test-specific metadata file."""
        test_name = "external/wpt/variant.html?foo=bar/abc"
        expectations = _make_expectation(self.port, test_name, "SKIP")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, SKIP_TEST)
        # The metadata file name should not include variants
        self.assertEqual("variant.html.ini", filename)
        # ..but the contents of the file should include variants in the test name
        self.assertEqual("[variant.html?foo=bar/abc]\n  disabled: wpt_metadata_builder.py\n", contents)

    def test_metadata_for_skipped_directory(self):
        """A skipped WPT directory should get a dir-wide metadata file."""
        test_dir = "external/wpt/test_dir/"
        test_name = "external/wpt/test_dir/test.html"
        expectations = _make_expectation(self.port, test_dir, "SKIP", test_names=[test_name])
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_dir, SKIP_TEST)
        self.assertEqual(os.path.join("test_dir", "__dir__.ini"), filename)
        self.assertEqual("disabled: wpt_metadata_builder.py\n", contents)

    def test_metadata_for_wpt_test_with_fail_baseline(self):
        """A WPT test with a baseline file containing failures gets metadata."""
        test_name = "external/wpt/dir/zzzz.html"
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, SUBTEST_FAIL)
        self.assertEqual(os.path.join("dir", "zzzz.html.ini"), filename)
        self.assertEqual(
            "[zzzz.html]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n",
            contents)

    def test_metadata_for_wpt_test_with_harness_error_baseline(self):
        """A WPT test with a baseline file containing a harness error gets metadata."""
        test_name = "external/wpt/dir/zzzz.html"
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, HARNESS_ERROR)
        self.assertEqual(os.path.join("dir", "zzzz.html.ini"), filename)
        self.assertEqual("[zzzz.html]\n  expected: [ERROR]\n", contents)

    def test_metadata_for_wpt_test_with_harness_error_and_subtest_fail_baseline(self):
        """A WPT test with a baseline file containing a harness error and subtest failure gets metadata."""
        test_name = "external/wpt/dir/zzzz.html"
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, SUBTEST_FAIL | HARNESS_ERROR)
        self.assertEqual(os.path.join("dir", "zzzz.html.ini"), filename)
        self.assertEqual("[zzzz.html]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n  expected: [ERROR]\n",
                         contents)

    def test_metadata_for_wpt_multiglobal_test_with_baseline(self):
        """A WPT test with a baseline file containing failures gets metadata."""
        test_name = "external/wpt/dir/multiglob.https.any.window.html"
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, SUBTEST_FAIL)
        # The metadata filename matches the test *filename*, not the test name,
        # which in this case is the js file from the manifest.
        self.assertEqual(os.path.join("dir", "multiglob.https.any.js.ini"), filename)
        # The metadata contents contains the *test name*
        self.assertEqual(
            "[multiglob.https.any.window.html]\n  blink_expect_any_subtest_status: True # wpt_metadata_builder.py\n",
            contents)

    def test_precondition_failed(self):
        """A WPT that fails a precondition."""
        test_name = "external/wpt/test.html"
        expectations = TestExpectations(self.port)
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        filename, contents = metadata_builder.get_metadata_filename_and_contents(test_name, TEST_PRECONDITION_FAILED)
        self.assertEqual("test.html.ini", filename)
        # The PASS and FAIL expectations fan out to also include OK and ERROR
        # to support reftest/testharness test differences.
        self.assertEqual("[test.html]\n  expected: [PRECONDITION_FAILED]\n", contents)

    def test_parse_subtest_failure_annotation(self):
        """Check that we parse the wpt_subtest_failure annotation correctly."""
        test_name = "external/wpt/test.html"
        expectations = _make_expectation(self.port, test_name, "PASS", trailing_comments=" # wpt_subtest_failure")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(TEST_PASS | SUBTEST_FAIL, test_and_status_dict[test_name])

    def test_parse_precondition_failure_annotation(self):
        """Check that we parse the wpt_precondition_failed annotation correctly."""
        test_name = "external/wpt/test.html"
        expectations = _make_expectation(self.port, test_name, "PASS", trailing_comments=" # wpt_precondition_failed")
        metadata_builder = WPTMetadataBuilder(expectations, self.port)
        test_and_status_dict = metadata_builder.get_tests_needing_metadata()
        self.assertEqual(1, len(test_and_status_dict))
        self.assertTrue(test_name in test_and_status_dict)
        self.assertEqual(TEST_PASS | TEST_PRECONDITION_FAILED, test_and_status_dict[test_name])

    def test_metadata_filename_from_test_file(self):
        """Check that we get the correct metadata filename in various cases."""
        expectations = TestExpectations(self.port)
        mb = WPTMetadataBuilder(expectations, self.port)
        self.assertEqual("test.html.ini", mb._metadata_filename_from_test_file("test.html"))
        test_file = os.path.join("dir", "multiglob.https.any.js")
        self.assertEqual(test_file + ".ini", mb._metadata_filename_from_test_file(test_file))
        with self.assertRaises(AssertionError):
            mb._metadata_filename_from_test_file("test.html?variant=abc")

    def test_inline_test_name_from_test_name(self):
        """Check that we get the correct inline test name in various cases."""
        expectations = TestExpectations(self.port)
        mb = WPTMetadataBuilder(expectations, self.port)
        self.assertEqual("test.html", mb._metadata_inline_test_name_from_test_name("test.html"))
        self.assertEqual("test.html", mb._metadata_inline_test_name_from_test_name("dir/test.html"))
        self.assertEqual("test.html?variant=abc", mb._metadata_inline_test_name_from_test_name("dir/test.html?variant=abc"))
        self.assertEqual("test.html?variant=abc/def", mb._metadata_inline_test_name_from_test_name("dir/test.html?variant=abc/def"))
        self.assertEqual("test.worker.html", mb._metadata_inline_test_name_from_test_name("test.worker.html"))
        self.assertEqual("test.worker.html?variant=abc",
                         mb._metadata_inline_test_name_from_test_name("dir/test.worker.html?variant=abc"))
