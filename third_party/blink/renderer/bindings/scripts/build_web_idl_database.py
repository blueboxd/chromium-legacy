# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Builds Web IDL database.

Web IDL database is a Python object that supports a variety of accessors to
IDL definitions such as IDL interface and IDL attribute.
"""

import optparse

import utilities
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

    # Incomplete IDL compiler produces a lot of errors, which break trybots.
    # So, we ignore all the errors for the time being.
    # TODO(bindings-team): Replace |report_error| with sys.exit once IDL
    # compiler completes.
    report_error = lambda message: None

    database = web_idl.build_database(filepaths=filepaths,
                                      report_error=report_error)

    utilities.write_pickle_file(options.output, database)


if __name__ == '__main__':
    main()
