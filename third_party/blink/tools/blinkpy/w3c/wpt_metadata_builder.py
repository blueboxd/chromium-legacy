# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Converts Chromium Test Expectations into WPT Metadata ini files.

This script loads TestExpectations for any WPT test and creates the metadata
files corresponding to the expectation. This script runs as a BUILD action rule.
The output is then bundled into the WPT isolate package to be shipped to bots
running the WPT test suite.
"""

import argparse
import logging
import os
import re

from blinkpy.common.system.log_utils import configure_logging
from blinkpy.web_tests.models import test_expectations

_log = logging.getLogger(__name__)


class WPTMetadataBuilder(object):
    def __init__(self, expectations, port):
        """
        Args:
            expectations: a blinkpy.web_tests.models.test_expectations.TestExpectations object
            port: a blinkpy.web_tests.port.Port object
        """
        self.expectations = expectations
        self.port = port
        self.metadata_output_dir = ""

    def run(self, args=None):
        """Main entry point to parse flags and execute the script."""
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument("--metadata-output-dir",
                            help="The directory to output the metadata files into.")
        parser.add_argument('-v', '--verbose', action='store_true', help='More verbose logging.')
        args = parser.parse_args(args)

        log_level = logging.DEBUG if args.verbose else logging.INFO
        configure_logging(logging_level=log_level, include_time=True)

        self.metadata_output_dir = args.metadata_output_dir
        self._build_metadata_and_write()

        return 0

    def _build_metadata_and_write(self):
        """Build the metadata files and write them to disk."""
        if os.path.exists(self.metadata_output_dir):
            _log.debug("Output dir exists, deleting: %s",
                       self.metadata_output_dir)
            import shutil
            shutil.rmtree(self.metadata_output_dir)

        failing_baseline_tests = self.get_test_names_to_fail()
        _log.info("Found %d tests with failing baselines", len(failing_baseline_tests))
        for test_name in failing_baseline_tests:
            filename, file_contents = self.get_metadata_filename_and_contents(test_name, 'FAIL')
            if not filename or not file_contents:
                continue
            self._write_to_file(filename, file_contents)

        skipped_tests = self.get_test_names_to_skip()
        _log.info("Found %d tests with skip expectations", len(skipped_tests))
        for test_name in skipped_tests:
            if test_name in failing_baseline_tests:
                _log.error("Test %s has a baseline but is also skipped" % test_name)
            filename, file_contents = self.get_metadata_filename_and_contents(test_name, 'SKIP')
            if not filename or not file_contents:
                continue
            self._write_to_file(filename, file_contents)

        # Finally, output a stamp file with the same name as the output
        # directory. The stamp file is empty, it's only used for its mtime.
        # This makes the GN build system happy (see crbug.com/995112).
        with open(self.metadata_output_dir + ".stamp", "w"):
            pass

    def _write_to_file(self, filename, file_contents):
        # Write the contents to the file name
        if not os.path.exists(os.path.dirname(filename)):
            os.makedirs(os.path.dirname(filename))
        with open(filename, "w") as metadata_file:
            metadata_file.write(file_contents)

    def get_test_names_to_skip(self):
        """Determines which tests in the expectation file need metadata.

        Returns:
            A list of test names that need metadata.
        """
        return self.expectations.get_tests_with_result_type(
            test_expectations.SKIP)

    def get_test_names_to_fail(self):
        """Determines which tests should be expected to fail.

        This is currently just tests that have failing baselines defined.

        Returns:
            A list of test names that need metadata.
        """
        all_tests = self.port.tests(paths=['external/wpt'])
        failing_baseline_tests = []
        for test in all_tests:
            test_baseline = self.port.expected_text(test)
            if not test_baseline:
                continue
            if re.search("^FAIL", test_baseline, re.MULTILINE):
                failing_baseline_tests.append(test)
            else:
                # Treat this as an error because we don't want it to happen.
                # Either the non-FAIL statuses need to be handled here, or the
                # baseline is all PASS which should just be deleted.
                _log.error("Test %s has a non-FAIL baseline" % test)
        return failing_baseline_tests

    def get_metadata_filename_and_contents(self, test_name, test_status):
        """Determines the metadata filename and contents for the specified test.

        The metadata filename is derived from the test name but will differ if
        the expectation is for a single test or for a directory of tests. The
        contents of the metadata file will also differ for those two cases.

        Args:
            test_name: A test name from the expectation file.
            test_status: The expected status of this test. Possible values:
                'SKIP' - skip this test (or directory).
                'FAIL' - the test is expected to fail, not applicable to dirs.

        Returns:
            A pair of strings, the first is the path to the metadata file and
            the second is the contents to write to that file. Or None if the
            test does not need a metadata file.
        """
        assert test_status in ('SKIP', 'FAIL')

        # Ignore expectations for non-WPT tests
        if not test_name or not test_name.startswith('external/wpt'):
            return None, None

        # Split the test name by directory. We omit the first 2 entries because
        # they are 'external' and 'wpt' and these don't exist in the WPT's test
        # names.
        test_name_parts = test_name.split("/")[2:]

        # Check if this is a test file or a test directory
        is_test_dir = test_name.endswith("/")
        metadata_filename = None
        metadata_file_contents = None
        if is_test_dir:
            # A test directory gets one metadata file called __dir__.ini and all
            # tests in that dir are skipped.
            metadata_filename = os.path.join(self.metadata_output_dir,
                                             *test_name_parts)
            metadata_filename = os.path.join(metadata_filename, "__dir__.ini")
            _log.debug("Creating a dir-wide ini file %s", metadata_filename)

            metadata_file_contents = self._get_dir_disabled_string()
        else:
            # For individual tests, we create one file per test, with the name
            # of the test in the file as well. This name can contain variants.
            test_name = test_name_parts[-1]

            # If the test name uses variants, we want to omit them from the
            # filename.
            if "?" in test_name:
                # Update test_name_parts so the created metadata file doesn't
                # include any variants
                test_name_parts[-1] = test_name.split("?")[0]

            # Append `.ini` to the test filename to indicate it's the metadata
            # file.
            test_name_parts[-1] += ".ini"
            metadata_filename = os.path.join(self.metadata_output_dir,
                                             *test_name_parts)
            _log.debug("Creating a test ini file %s with status %s",
                       metadata_filename, test_status)

            # The contents of the metadata file is two lines:
            # 1. the test name inside square brackets
            # 2. an indented line with the test status and reason
            if test_status == 'SKIP':
                metadata_file_contents = self._get_test_disabled_string(test_name)
            elif test_status == 'FAIL':
                metadata_file_contents = self._get_test_failed_string(test_name)

        return metadata_filename, metadata_file_contents

    def _get_dir_disabled_string(self):
        return "disabled: wpt_metadata_builder.py"

    def _get_test_disabled_string(self, test_name):
        return "[%s]\n  disabled: wpt_metadata_builder.py" % test_name

    def _get_test_failed_string(self, test_name):
        return "[%s]\n  expected: FAIL # wpt_metadata_builder.py" % test_name
