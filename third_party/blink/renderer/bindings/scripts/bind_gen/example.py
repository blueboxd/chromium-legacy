# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path

from . import code_node
from .clang_format import clang_format
from .mako_renderer import MakoRenderer


def run_example(web_idl_database, output_dirs):
    renderer = MakoRenderer()

    filename = 'v8_example.cc'
    filepath = os.path.join(output_dirs['core'], filename)

    root_node = code_node.SymbolScopeNode(renderer=renderer)
    root_node.extend([
        code_node.TextNode("${z} = ${x} + ${y};"),
        code_node.TextNode("print ${z};"),
    ])

    root_node.register_code_symbols([
        code_node.SymbolNode('x', "int ${x} = 1;"),
        code_node.SymbolNode('y', "int ${y} = 2;"),
        code_node.SymbolNode('z', "int ${z};"),
    ])

    prev = ''
    current = str(root_node)
    while current != prev:
        prev = current
        current = str(root_node)
    rendered_text = current

    format_result = clang_format(rendered_text, filename=filename)
    with open(filepath, 'w') as output_file:
        output_file.write(format_result.contents)
