#!/usr/bin/env vpython
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Given an expectations file (e.g. web_tests/WebGPUExpectations), extracts only
# the test name from each expectation (e.g. wpt_internal/webgpu/cts.html?...).

from blinkpy.common import path_finder

path_finder.add_typ_dir_to_sys_path()

from typ.expectations_parser import TaggedTestListParser
import sys


class StubPort(object):
    def is_wpt_test(name):
        return False


filename = sys.argv[1]
with open(filename) as f:
    port = StubPort()
    parser = TaggedTestListParser(f.read())
    for test_expectation in parser.expectations:
        if test_expectation.test:
            print test_expectation.test
