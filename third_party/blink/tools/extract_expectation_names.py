#!/usr/bin/env vpython
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Given an expectations file (e.g. web_tests/WebGPUExpectations), extracts only
# the test name from each expectation (e.g. wpt_internal/webgpu/cts.html?...).

from blinkpy.web_tests.models.test_expectations import TestExpectationLine
import sys


class StubPort(object):
    def is_wpt_test(name):
        return False


filename = sys.argv[1]
with open(filename) as f:
    port = StubPort()
    line_number = 1
    for line in f:
        test_expectation = TestExpectationLine.tokenize_line(filename, line, line_number, port)
        if test_expectation.name:
            print test_expectation.name
        line_number += 1
