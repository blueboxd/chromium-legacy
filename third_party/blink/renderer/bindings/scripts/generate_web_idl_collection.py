# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a data collection of IDL information per component.
This scripts parses IDL files and stores the result ASTs in a pickle file.
The output file may contain information about component, too.
"""

import blink_idl_parser
import optparse
import utilities
from web_idl.collector import Collector
from web_idl.collection import Collection


_VALID_COMPONENTS = ("core", "modules")

def parse_options():
    parser = optparse.OptionParser()
    parser.add_option('--idl-list-file', type='string',
                      help='a file path which lists IDL file paths to process')
    parser.add_option('--component', type='choice', choices=_VALID_COMPONENTS,
                      help='specify a component name where IDLs belong')
    parser.add_option('--output', type='string',
                      help='pickle file to write down')
    options, args = parser.parse_args()

    if options.idl_list_file is None:
        parser.error('Must specify a file listing IDL files using --idl-list-file.')
    if options.output is None:
        parser.error('Must specify a pickle file to output using --output.')
    if options.component is None:
        parser.error('Must specify a component using --component.')

    return options, args


def main():
    options, _ = parse_options()
    idl_file_names = utilities.read_idl_files_list_from_file(options.idl_list_file)

    parser = blink_idl_parser.BlinkIDLParser()
    collector = Collector(component=options.component, parser=parser)
    collector.collect_from_idl_files(idl_file_names)
    Collection.write_to_file(collector.get_collection(), options.output)


if __name__ == '__main__':
    main()
