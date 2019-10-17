# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from .code_node import LiteralNode
from .code_node import SequenceNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .mako_renderer import MakoRenderer


class CodeNodeTest(unittest.TestCase):
    def render(self, node):
        prev = ""
        current = str(node)
        while current != prev:
            prev = current
            current = str(node)
        return current

    def assertRenderResult(self, node, expected):
        def simplify(s):
            return " ".join(s.split())

        actual = simplify(self.render(node))
        expected = simplify(expected)

        self.assertEqual(actual, expected)

    def test_literal_node(self):
        """
        Tests that, in LiteralNode, the special characters of template (%, ${},
        etc) are not processed.
        """
        renderer = MakoRenderer()
        root = LiteralNode("<% x = 42 %>${x}", renderer=renderer)
        self.assertRenderResult(root, "<% x = 42 %>${x}")

    def test_empty_literal_node(self):
        renderer = MakoRenderer()
        root = LiteralNode("", renderer=renderer)
        self.assertRenderResult(root, "")

    def test_text_node(self):
        """Tests that the template language works in TextNode."""
        renderer = MakoRenderer()
        root = TextNode("<% x = 42 %>${x}", renderer=renderer)
        self.assertRenderResult(root, "42")

    def test_empty_text_node(self):
        renderer = MakoRenderer()
        root = TextNode("", renderer=renderer)
        self.assertRenderResult(root, "")

    def test_list_operations_of_sequence_node(self):
        """
        Tests that list operations (insert, append, and extend) of SequenceNode
        work just same as Python built-in list.
        """
        renderer = MakoRenderer()
        root = SequenceNode(renderer=renderer)
        root.extend([
            LiteralNode("2"),
            LiteralNode("4"),
        ])
        root.insert(1, LiteralNode("3"))
        root.insert(0, LiteralNode("1"))
        root.insert(100, LiteralNode("5"))
        root.append(LiteralNode("6"))
        self.assertRenderResult(root, "1 2 3 4 5 6")

    def test_nested_sequence(self):
        """Tests nested SequenceNodes."""
        renderer = MakoRenderer()
        root = SequenceNode(renderer=renderer)
        nested = SequenceNode()
        nested.extend([
            LiteralNode("2"),
            LiteralNode("3"),
            LiteralNode("4"),
        ])
        root.extend([
            LiteralNode("1"),
            nested,
            LiteralNode("5"),
        ])
        self.assertRenderResult(root, "1 2 3 4 5")

    def test_symbol_definition_chains(self):
        """
        Tests that use of SymbolNode inserts necessary SymbolDefinitionNode
        appropriately.
        """
        renderer = MakoRenderer()
        root = SymbolScopeNode(renderer=renderer)

        root.register_code_symbols([
            SymbolNode("var1", "int ${var1} = ${var2} + ${var3};"),
            SymbolNode("var2", "int ${var2} = ${var5};"),
            SymbolNode("var3", "int ${var3} = ${var4};"),
            SymbolNode("var4", "int ${var4} = 1;"),
            SymbolNode("var5", "int ${var5} = 2;"),
        ])

        root.append(TextNode("(void)${var1};"))

        self.assertRenderResult(
            root, """
int var5 = 2;
int var4 = 1;
int var3 = var4;
int var2 = var5;
int var1 = var2 + var3;
(void)var1;
        """)
