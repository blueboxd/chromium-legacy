# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Given the Web IDL database as input, output a list of all high entropy entities.

High entropy entities (methods, attributes, constants) are annotated with
the [HighEntropy] extended attribute.
"""

import optparse

from utilities import write_file
import web_idl


def parse_options():
    parser = optparse.OptionParser(usage="%prog [options]")
    parser.add_option("--web_idl_database", type="string",
                      help="filepath of the input database")
    parser.add_option("--output", type="string",
                      help="filepath of output file")
    options, args = parser.parse_args()

    required_option_names = ("web_idl_database", "output")
    for opt_name in required_option_names:
        if getattr(options, opt_name) is None:
            parser.error("--{} is a required option.".format(opt_name))

    return options, args


def main():
    options, _ = parse_options()

    web_idl_database = web_idl.Database.read_from_file(options.web_idl_database)
    high_entropy_list = []
    for interface in web_idl_database.interfaces:
        for entity in (interface.attributes + interface.operations +
                       interface.constants):
            if "HighEntropy" in entity.extended_attributes:
                high_entropy_list.append('%s.%s' % (interface.identifier,
                                                    entity.identifier))
    write_file('\n'.join(high_entropy_list) + '\n', options.output)


if __name__ == '__main__':
    main()
