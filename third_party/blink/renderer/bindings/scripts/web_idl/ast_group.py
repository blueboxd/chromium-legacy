# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pickle

from .common import Component


class AstGroup(object):
    """A set of Web IDL ASTs grouped by component."""

    def __init__(self, component=None):
        assert component is None or isinstance(component, Component)
        self._nodes = []
        self._component = component

    def __iter__(self):
        return self._nodes.__iter__()

    @staticmethod
    def read_from_file(filepath):
        with open(filepath, 'r') as pickle_file:
            ast_group = pickle.load(pickle_file)
        assert isinstance(ast_group, AstGroup)
        return ast_group

    def write_to_file(self, filepath):
        with open(filepath, 'w') as pickle_file:
            pickle.dump(self, pickle_file)

    def add_ast_node(self, node):
        assert node.GetClass() == 'File', (
            'Root node of an AST must be a File node, but is %s.' %
            node.GetClass())
        self._nodes.append(node)

    @property
    def component(self):
        return self._component
