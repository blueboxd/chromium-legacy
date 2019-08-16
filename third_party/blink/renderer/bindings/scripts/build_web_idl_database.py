# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Builds Web IDL database.

Web IDL database is a Python object that supports a variety of accessors to
IDL definitions such as IDL interface and IDL attribute.
"""

import optparse
import sys

import web_idl


def parse_options():
    parser = optparse.OptionParser()
    parser.add_option('--output', type='string',
                      help='filepath of the resulting database')
    options, args = parser.parse_args()

    if options.output is None:
        parser.error('Specify a filepath of the database with --output.')

    return options, args


def main():
    options, filepaths = parse_options()

    was_error_reported = [False]

    def report_error(message):
        was_error_reported[0] = True
        sys.stderr.writelines([message, '\n'])

    database = web_idl.build_database(filepaths=filepaths,
                                      report_error=report_error)

    if was_error_reported[0]:
        sys.exit('Aborted due to error.')

    database.write_to_file(options.output)


if __name__ == '__main__':
    main()
