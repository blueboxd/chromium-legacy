# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from .code_node import LikelyExitNode
from .code_node import LiteralNode
from .code_node import SequenceNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .code_node import UnlikelyExitNode
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
int var2 = var5;
int var4 = 1;
int var3 = var4;
int var1 = var2 + var3;
(void)var1;
        """)

    def test_symbol_definition_with_exit_branches(self):
        renderer = MakoRenderer()
        root = SymbolScopeNode(renderer=renderer)

        root.register_code_symbols([
            SymbolNode("var1", "int ${var1} = 1;"),
            SymbolNode("var2", "int ${var2} = 2;"),
            SymbolNode("var3", "int ${var3} = 3;"),
            SymbolNode("var4", "int ${var4} = 4;"),
            SymbolNode("var5", "int ${var5} = 5;"),
            SymbolNode("var6", "int ${var6} = 6;"),
        ])

        root.extend([
            TextNode("${var1};"),
            UnlikelyExitNode(
                cond_node=TextNode("${var2}"),
                body_node=SymbolScopeNode([
                    TextNode("${var3};"),
                    TextNode("return ${var4};"),
                ])),
            LikelyExitNode(
                cond_node=TextNode("${var5}"),
                body_node=SymbolScopeNode([
                    TextNode("return ${var6};"),
                ])),
            TextNode("${var3};"),
        ])

        self.assertRenderResult(
            root, """
int var1 = 1;
var1;
int var2 = 2;
int var3 = 3;
if (var2) {
  var3;
  int var4 = 4;
  return var4;
}
int var5 = 5;
if (var5) {
  int var6 = 6;
  return var6;
}
var3;
""")

    def test_symbol_definition_with_nested_exit_branches(self):
        renderer = MakoRenderer()
        root = SymbolScopeNode(renderer=renderer)

        root.register_code_symbols([
            SymbolNode("var1", "int ${var1} = 1;"),
            SymbolNode("var2", "int ${var2} = 2;"),
            SymbolNode("var3", "int ${var3} = 3;"),
            SymbolNode("var4", "int ${var4} = 4;"),
            SymbolNode("var5", "int ${var5} = 5;"),
            SymbolNode("var6", "int ${var6} = 6;"),
        ])

        root.extend([
            UnlikelyExitNode(
                cond_node=LiteralNode("false"),
                body_node=SymbolScopeNode([
                    UnlikelyExitNode(
                        cond_node=LiteralNode("false"),
                        body_node=SymbolScopeNode([
                            TextNode("return ${var1};"),
                        ])),
                    LiteralNode("return;"),
                ])),
            LikelyExitNode(
                cond_node=LiteralNode("true"),
                body_node=SymbolScopeNode([
                    LikelyExitNode(
                        cond_node=LiteralNode("true"),
                        body_node=SymbolScopeNode([
                            TextNode("return ${var2};"),
                        ])),
                    LiteralNode("return;"),
                ])),
        ])

        self.assertRenderResult(
            root, """
if (false) {
  if (false) {
    int var1 = 1;
    return var1;
  }
  return;
}
if (true) {
  if (true) {
    int var2 = 2;
    return var2;
  }
  return;
}
""")
