# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Runs the bindings code generator for the given tasks.
"""

import optparse
import sys

import web_idl
import bind_gen


def parse_options():
    parser = optparse.OptionParser(usage="%prog [options] TASK...")
    parser.add_option(
        "--web_idl_database",
        type="string",
        help="filepath of the input database")
    parser.add_option(
        "--root_src_dir",
        type="string",
        help='root directory of chromium project, i.e. "//"')
    parser.add_option(
        "--root_gen_dir",
        type="string",
        help='root directory of generated code files, i.e. '
        '"//out/Default/gen"')
    parser.add_option(
        "--output_core_reldir",
        type="string",
        help='output directory for "core" component relative to '
        'root_gen_dir')
    parser.add_option(
        "--output_modules_reldir",
        type="string",
        help='output directory for "modules" component relative '
        'to root_gen_dir')
    options, args = parser.parse_args()

    required_option_names = ("web_idl_database", "root_src_dir",
                             "root_gen_dir", "output_core_reldir",
                             "output_modules_reldir")
    for opt_name in required_option_names:
        if getattr(options, opt_name) is None:
            parser.error("--{} is a required option.".format(opt_name))

    if not args:
        parser.error("No argument specified.")

    return options, args


def main():
    options, tasks = parse_options()

    dispatch_table = {
        'dictionary': bind_gen.generate_dictionaries,
        'enumeration': bind_gen.generate_enumerations,
        'interface': bind_gen.generate_interfaces,
        'union': bind_gen.generate_unions,
    }

    for task in tasks:
        if task not in dispatch_table:
            sys.exit("Unknown task: {}".format(task))

    web_idl_database = web_idl.Database.read_from_file(
        options.web_idl_database)
    component_reldirs = {
        web_idl.Component('core'): options.output_core_reldir,
        web_idl.Component('modules'): options.output_modules_reldir,
    }

    bind_gen.init(
        root_src_dir=options.root_src_dir,
        root_gen_dir=options.root_gen_dir,
        component_reldirs=component_reldirs)

    for task in tasks:
        dispatch_table[task](web_idl_database=web_idl_database)


if __name__ == '__main__':
    main()
