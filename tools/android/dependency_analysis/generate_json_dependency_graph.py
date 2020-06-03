#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Command-line tool to run jdeps and process its output into a JSON file."""

import argparse
import pathlib
import subprocess

import class_dependency
import package_dependency
import serialization

SRC_PATH = pathlib.Path(__file__).resolve().parents[3]  # src/
JDEPS_PATH = SRC_PATH.joinpath('third_party/jdk/current/bin/jdeps')


def class_is_interesting(name: str):
    """Checks if a jdeps class is a class we are actually interested in."""
    if name.startswith('org.chromium.'):
        return True
    return False


class JavaClassJdepsParser(object):  # pylint: disable=useless-object-inheritance
    """A parser for jdeps class-level dependency output."""
    def __init__(self):  # pylint: disable=missing-function-docstring
        self._graph = class_dependency.JavaClassDependencyGraph()

    @property
    def graph(self):
        """The dependency graph of the jdeps output.

        Initialized as empty and updated using parse_raw_jdeps_output.
        """
        return self._graph

    def parse_raw_jdeps_output(self, jdeps_output: str):
        """Parses the entirety of the jdeps output."""
        for line in jdeps_output.split('\n'):
            self.parse_line(line)

    def parse_line(self, line: str):
        """Parses a line of jdeps output.

        The assumed format of the line starts with 'name_1 -> name_2'.
        """
        parsed = line.split()
        if len(parsed) <= 3:
            return
        if parsed[2] == 'not' and parsed[3] == 'found':
            return
        if parsed[1] != '->':
            return

        dep_from = parsed[0]
        dep_to = parsed[2]
        if not class_is_interesting(dep_from):
            return
        if not class_is_interesting(dep_to):
            return

        key_from, nested_from = class_dependency.split_nested_class_from_key(
            dep_from)
        key_to, nested_to = class_dependency.split_nested_class_from_key(
            dep_to)

        self._graph.add_node_if_new(key_from)
        self._graph.add_node_if_new(key_to)
        if key_from != key_to:  # Skip self-edges (class-nested dependency)
            self._graph.add_edge_if_new(key_from, key_to)
        if nested_from is not None:
            self._graph.add_nested_class_to_key(key_from, nested_from)
        if nested_to is not None:
            self._graph.add_nested_class_to_key(key_from, nested_to)


def run_jdeps(jdeps_path: str, filepath: str):
    """Runs jdeps on the given filepath and returns the output."""
    jdeps_res = subprocess.run([jdeps_path, '-R', '-verbose:class', filepath],
                               capture_output=True,
                               text=True,
                               check=True)
    return jdeps_res.stdout


def main():
    """Runs jdeps and creates a JSON file from the output."""
    arg_parser = argparse.ArgumentParser(
        description='Runs jdeps (dependency analysis tool) on a given JAR and '
        'writes the resulting dependency graph into a JSON file.')
    required_arg_group = arg_parser.add_argument_group('required arguments')
    required_arg_group.add_argument(
        '-t',
        '--target',
        required=True,
        help='Path to the JAR file to run jdeps on.')
    required_arg_group.add_argument(
        '-o',
        '--output',
        required=True,
        help='Path to the file to write JSON output to. Will be created '
        'if it does not yet exist and overwrite existing '
        'content if it does.')
    arg_parser.add_argument('-j',
                            '--jdeps-path',
                            default=JDEPS_PATH,
                            help='Path to the jdeps executable.')
    arguments = arg_parser.parse_args()

    print('Running jdeps and parsing output...')
    raw_jdeps_output = run_jdeps(arguments.jdeps_path, arguments.target)
    jdeps_parser = JavaClassJdepsParser()
    jdeps_parser.parse_raw_jdeps_output(raw_jdeps_output)

    class_graph = jdeps_parser.graph
    print(f'Parsed class-level dependency graph, '
          f'got {class_graph.num_nodes} nodes '
          f'and {class_graph.num_edges} edges.')

    package_graph = package_dependency.JavaPackageDependencyGraph(class_graph)
    print(f'Created package-level dependency graph, '
          f'got {package_graph.num_nodes} nodes '
          f'and {package_graph.num_edges} edges.')

    print(f'Dumping JSON representation to {arguments.output}.')
    serialization.dump_class_and_package_graphs_to_file(
        class_graph, package_graph, arguments.output)


if __name__ == '__main__':
    main()
