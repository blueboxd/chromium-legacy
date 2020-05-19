# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implementation of the graph module for a [Java class] dependency graph."""

import re
from typing import Tuple

import graph

# Matches w/o parens: (some.package.name).(class)$($optional$nested$class)
JAVA_CLASS_FULL_NAME_REGEX = re.compile(
    r"^(?P<package>.*)\.(?P<class_name>.*?)(\$(?P<nested>.*))?$")


def java_class_params_to_key(package: str, class_name: str):
    """Returns the unique key created from a package and class name."""
    return f"{package}.{class_name}"


def split_nested_class_from_key(key: str) -> Tuple[str, str]:
    """Splits a jdeps class name into its key and nested class, if any."""
    re_match = JAVA_CLASS_FULL_NAME_REGEX.match(key)
    package = re_match.group("package")
    class_name = re_match.group("class_name")
    nested = re_match.group("nested")
    return java_class_params_to_key(package, class_name), nested


class JavaClass(graph.Node):
    """A representation of a Java class.

    Some classes may have nested classes (eg. explicitly, or
    implicitly through lambdas). We treat these nested classes as part of
    the outer class, storing only their names as metadata.
    """
    def __init__(self, package: str, class_name: str):
        """Initializes a new Java class structure.

        The package and class_name are used to create a unique key per class.

        Args:
            package: The package the class belongs to.
            class_name: The name of the class. For nested classes, this is
                the name of the class that contains them.
        """
        super().__init__(java_class_params_to_key(package, class_name))

        self._package = package
        self._class_name = class_name

        self._nested_classes = set()

    def add_nested_class(self, nested: str):  # pylint: disable=missing-function-docstring
        self._nested_classes.add(nested)


class JavaClassDependencyGraph(graph.Graph):
    """A graph representation of the dependencies between Java classes.

    A directed edge A -> B indicates that A depends on B.
    """
    def create_node_from_key(self, key: str):
        """See comment above the regex definition."""
        re_match = JAVA_CLASS_FULL_NAME_REGEX.match(key)
        package = re_match.group("package")
        class_name = re_match.group("class_name")
        return JavaClass(package, class_name)

    def add_nested_class_to_key(self, key: str, nested: str):  # pylint: disable=missing-function-docstring
        self.get_node_by_key(key).add_nested_class(nested)
