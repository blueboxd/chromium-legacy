#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import optparse
import sys

from blinkpy.common.host import Host
from blinkpy.web_tests.port.android import AndroidPort
from blinkpy.w3c.wpt_metadata_builder import WPTMetadataBuilder
from blinkpy.web_tests.models.test_expectations import TestExpectations


def main(args):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--additional-expectations",
        action="append",
        help="Paths to additional expectations files for WPT.")
    parser.add_argument(
        "--android-apk",
        default=None,
        help="Path to Android APK that is being tested")

    known_args, rest_args = parser.parse_known_args(args)
    options = optparse.Values(vars(known_args))
    host = Host()

    if known_args.android_apk:
        port = AndroidPort(host, apk=known_args.android_apk, options=options)
    else:
        port = host.port_factory.get(options=options)

    expectations = TestExpectations(port)
    metadata_builder = WPTMetadataBuilder(expectations, port)
    sys.exit(metadata_builder.run(rest_args))


if __name__ == '__main__':
    main(sys.argv[1:])
