# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from .code_node import SequenceNode
from .code_node import SimpleNode
from .code_node import SymbolDefinitionNode
from .code_node import SymbolNode
from .mako_renderer import MakoRenderer


class CodeNodeTest(unittest.TestCase):
    def render(self, node):
        prev = ''
        current = str(node)
        while current != prev:
            prev = current
            current = str(node)
        return current

    def assertRenderResult(self, node, expected):
        def simplify(s):
            return ' '.join(s.split())

        actual = simplify(self.render(node))
        expected = simplify(expected)

        self.assertEqual(actual, expected)

    def test_list_operations_of_sequence_node(self):
        renderer = MakoRenderer()
        root = SequenceNode(renderer=renderer)
        root.extend([
            SimpleNode(template_text="2"),
            SimpleNode(template_text="4"),
        ])
        root.insert(1, SimpleNode(template_text="3"))
        root.insert(0, SimpleNode(template_text="1"))
        root.insert(100, SimpleNode(template_text="5"))
        root.append(SimpleNode(template_text="6"))
        self.assertRenderResult(root, "1 2 3 4 5 6")

    def test_nested_sequence(self):
        renderer = MakoRenderer()
        root = SequenceNode(renderer=renderer)
        nested = SequenceNode()
        nested.extend([
            SimpleNode(template_text="2"),
            SimpleNode(template_text="3"),
            SimpleNode(template_text="4"),
        ])
        root.extend([
            SimpleNode(template_text="1"),
            nested,
            SimpleNode(template_text="5"),
        ])
        self.assertRenderResult(root, "1 2 3 4 5")

    def test_symbol_definition_chains(self):
        renderer = MakoRenderer()
        root = SequenceNode(renderer=renderer)

        def make_symbol(name, template_text):
            def constructor(symbol_node):
                return SymbolDefinitionNode(
                    symbol_node, template_text=template_text)

            return SymbolNode(
                name=name, definition_node_constructor=constructor)

        root.add_template_vars({
            'var1':
            make_symbol('var1', "int var1 = ${var2} + ${var3};"),
            'var2':
            make_symbol('var2', "int var2 = ${var5};"),
            'var3':
            make_symbol('var3', "int var3 = ${var4};"),
            'var4':
            make_symbol('var4', "int var4 = 1;"),
            'var5':
            make_symbol('var5', "int var5 = 2;"),
        })

        root.append(SimpleNode(template_text="(void)${var1};"))

        self.assertRenderResult(
            root, """
int var5 = 2;
int var4 = 1;
int var3 = var4;
int var2 = var5;
int var1 = var2 + var3;
(void)var1;
        """)
