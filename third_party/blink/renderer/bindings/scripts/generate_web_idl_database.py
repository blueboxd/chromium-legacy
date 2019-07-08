#!/usr/bin/python
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a data collection of IDL information per component.
This scripts parses IDL files and stores the result ASTs in a pickle file.
The output file may contain information about component, too.
"""

import optparse
import utilities
from web_idl.idl_compiler import IdlCompiler
from web_idl import ir_builder
from web_idl.identifier_ir_map import IdentifierIRMap


def parse_options():
    parser = optparse.OptionParser()
    parser.add_option('--output', type='string',
                      help='pickle file to write down')
    options, args = parser.parse_args()

    if options.output is None:
        parser.error('Must specify a pickle file to output using --output.')

    return options, args


def main():
    options, filepaths = parse_options()
    ir_map = IdentifierIRMap()
    ir_builder.load_and_register_idl_definitions(filepaths, ir_map)
    idl_compiler = IdlCompiler(ir_map)
    idl_database = idl_compiler.build_database()
    utilities.write_pickle_file(options.output, idl_database)


if __name__ == '__main__':
    main()
