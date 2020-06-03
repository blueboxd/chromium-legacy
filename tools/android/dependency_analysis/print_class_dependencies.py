#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Command-line tool for printing class-level dependencies."""

import argparse

import graph
import print_dependencies_helper
import serialization


def print_class_dependencies_for_key(class_graph, key):
    """Prints dependencies for a valid key into the class graph."""
    node = class_graph.get_node_by_key(key)

    print(f'{len(node.inbound)} inbound dependency(ies) for {node.name}:')
    for inbound_dep in graph.sorted_nodes_by_name(node.inbound):
        print(f'\t{inbound_dep.name}')

    print(f'{len(node.outbound)} outbound dependency(ies) for {node.name}:')
    for outbound_dep in graph.sorted_nodes_by_name(node.outbound):
        print(f'\t{outbound_dep.name}')


def main():
    """Prints class-level dependencies for an input class."""
    arg_parser = argparse.ArgumentParser(
        description='Given a JSON dependency graph, output '
        'the class-level dependencies for a given class.')
    required_arg_group = arg_parser.add_argument_group('required arguments')
    required_arg_group.add_argument(
        '-f',
        '--file',
        required=True,
        help='Path to the JSON file containing the dependency graph. '
        'See the README on how to generate this file.')
    required_arg_group.add_argument(
        '-c',
        '--class',
        required=True,
        dest='class_name',
        help='Case-insensitive name of the class to print dependencies for. '
        'Matches names of the form ...input, for example '
        '`apphooks` matches `org.chromium.browser.AppHooks`.')
    arguments = arg_parser.parse_args()

    class_graph = serialization.load_class_graph_from_file(arguments.file)
    class_graph_keys = [node.name for node in class_graph.nodes]
    valid_keys = print_dependencies_helper.get_valid_keys_matching_input(
        class_graph_keys, arguments.class_name)

    if len(valid_keys) == 0:
        print(f'No class found by the name {arguments.class_name}.')
    elif len(valid_keys) > 1:
        print(
            f'Multiple valid keys found for the name {arguments.class_name}, '
            'please disambiguate between one of the following options:')
        for valid_key in valid_keys:
            print(f'\t{valid_key}')
    else:
        print(f'Printing class dependencies for {valid_keys[0]}:')
        print_class_dependencies_for_key(class_graph, valid_keys[0])


if __name__ == '__main__':
    main()
