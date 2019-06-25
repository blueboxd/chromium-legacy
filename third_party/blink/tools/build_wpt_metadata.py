#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from blinkpy.common.host import Host
from blinkpy.w3c.wpt_metadata_builder import WPTMetadataBuilder
from blinkpy.web_tests.models.test_expectations import TestExpectations


def main(args):
    host = Host()
    port = host.port_factory.get()
    expectations = TestExpectations(port)
    metadata_builder = WPTMetadataBuilder(expectations)
    sys.exit(metadata_builder.run(args))

if __name__ == '__main__':
    main(sys.argv[1:])
