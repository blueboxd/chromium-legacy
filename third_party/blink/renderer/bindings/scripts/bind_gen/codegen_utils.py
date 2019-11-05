# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .clang_format import clang_format
from .code_node import CodeNode
from .code_node import LiteralNode
from .code_node import SymbolScopeNode


def make_copyright_header():
    return LiteralNode("""\
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.\
""")


def enclose_with_header_guard(code_node, header_guard):
    assert isinstance(code_node, CodeNode)
    assert isinstance(header_guard, str)

    return SymbolScopeNode([
        LiteralNode("#ifndef {}".format(header_guard)),
        LiteralNode("#define {}".format(header_guard)),
        LiteralNode(""),
        code_node,
        LiteralNode(""),
        LiteralNode("#endif  // {}".format(header_guard)),
    ])


def enclose_with_namespace(code_node, namespace):
    assert isinstance(code_node, CodeNode)
    assert isinstance(namespace, str)

    return SymbolScopeNode([
        LiteralNode("namespace {} {{".format(namespace)),
        LiteralNode(""),
        code_node,
        LiteralNode(""),
        LiteralNode("}}  // namespace {}".format(namespace)),
    ])


def render_code_node(code_node):
    """
    Renders |code_node| and turns it into text letting |code_node| apply all
    necessary changes (side effects).  Returns the resulting text.
    """
    prev = "_"
    current = ""
    while current != prev:
        prev = current
        current = str(code_node)
    return current


def write_code_node_to_file(code_node, filepath):
    """Renders |code_node| and then write the result to |filepath|."""
    assert isinstance(code_node, CodeNode)
    assert isinstance(filepath, str)

    rendered_text = render_code_node(code_node)

    format_result = clang_format(rendered_text, filename=filepath)

    with open(filepath, "w") as output_file:
        output_file.write(format_result.contents)
