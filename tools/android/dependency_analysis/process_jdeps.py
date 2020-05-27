# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Python wrapper for running jdeps and parsing its output"""

import argparse
import pathlib
import subprocess

import class_dependency
import package_dependency

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


def run_jdeps(filepath: str):
    """Runs jdeps on the given filepath and returns the output."""
    jdeps_res = subprocess.run([JDEPS_PATH, '-R', '-verbose:class', filepath],
                               capture_output=True,
                               text=True,
                               check=True)
    return jdeps_res.stdout


def main():
    """Parses args, runs jdeps, and creates a graph from the output.

    Currently only works for class-level dependencies.
    The goal is to have more functions available for interfacing with the
    constructed graph, but currently we just print its number of nodes/edges.
    """
    arg_parser = argparse.ArgumentParser(
        description='Run jdeps and process output')
    arg_parser.add_argument('filepath', help='Path of the JAR to run jdeps on')
    arguments = arg_parser.parse_args()

    raw_jdeps_output = run_jdeps(arguments.filepath)
    jdeps_parser = JavaClassJdepsParser()
    jdeps_parser.parse_raw_jdeps_output(raw_jdeps_output)

    class_graph = jdeps_parser.graph

    print(f"Parsed class-level dependency graph, "
          f"got {class_graph.num_nodes} nodes "
          f"and {class_graph.num_edges} edges.")

    package_graph = package_dependency.JavaPackageDependencyGraph(class_graph)

    print(f"Created package-level dependency graph, "
          f"got {package_graph.num_nodes} nodes "
          f"and {package_graph.num_edges} edges.")


if __name__ == '__main__':
    main()
