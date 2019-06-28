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

from blinkpy.common.system.log_utils import configure_logging
from blinkpy.web_tests.models import test_expectations

_log = logging.getLogger(__name__)


class WPTMetadataBuilder(object):
    def __init__(self, expectations):
        """
        Args:
            expectations: a blinkpy.web_tests.models.test_expectations.TestExpectations object
        """
        self.expectations = expectations
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
            _log.warning("Output dir exists, deleting: %s",
                         self.metadata_output_dir)
            import shutil
            shutil.rmtree(self.metadata_output_dir)

        for test_name in self.get_test_names_for_metadata():
            filename, file_contents = self.get_metadata_filename_and_contents(test_name)
            if not filename or not file_contents:
                continue

            # Write the contents to the file name
            if not os.path.exists(os.path.dirname(filename)):
                os.makedirs(os.path.dirname(filename))
            with open(filename, "w") as metadata_file:
                metadata_file.write(file_contents)

    def get_test_names_for_metadata(self):
        """Determines which tests in the expectation file need metadata.

        Returns:
            A list of test names that need metadata.
        """
        return self.expectations.get_tests_with_result_type(
            test_expectations.SKIP)

    def get_metadata_filename_and_contents(self, test_name):
        """Determines the metadata filename and contents for the specified test.

        The metadata filename is derived from the test name but will differ if
        the expectation is for a single test or for a directory of tests. The
        contents of the metadata file will also differ for those two cases.

        Args:
            test_name: A test name from the expectation file.

        Returns:
            A pair of strings, the first is the path to the metadata file and
            the second is the contents to write to that file. Or None if the
            test does not need a metadata file.
        """
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

            metadata_file_contents = "disabled: build_wpt_metadata.py"
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
            _log.debug("Creating a test ini file %s", metadata_filename)

            # The contents of the metadata file is two lines:
            # 1. the test name inside square brackets
            # 2. an indented line with the test status and reason
            metadata_file_contents = ("[%s]\n  disabled: build_wpt_metadata.py" % test_name)
        return metadata_filename, metadata_file_contents
