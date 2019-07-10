# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pickle
import idl_parser
from .common import Component


class Collection(object):
    """
    Collection class stores ASTs of Web IDL files and meta information
    like component.
    """

    def __init__(self, component=None):
        assert component is None or isinstance(component, Component)
        self._asts = []
        self._component = component

    def add_ast(self, ast):
        assert isinstance(ast, idl_parser.idl_node.IDLNode)
        assert ast.GetClass() == 'File', (
            'Root node of an AST must be a File node., but is %s.' % ast.GetClass())
        self._asts.append(ast)

    @staticmethod
    def load_from_file(filepath):
        with open(filepath, 'r') as pickle_file:
            collection = pickle.load(pickle_file)
        assert isinstance(collection, Collection)
        return collection

    @staticmethod
    def write_to_file(collection, filepath):
        assert isinstance(collection, Collection)
        with open(filepath, 'w') as pickle_file:
            pickle.dump(collection, pickle_file)

    @property
    def asts(self):
        return self._asts

    @property
    def component(self):
        return self._component
