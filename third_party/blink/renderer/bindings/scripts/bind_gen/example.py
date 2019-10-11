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

    root_node = code_node.SequenceNode(renderer=renderer)
    root_node.extend([
        code_node.SimpleNode(template_text="${z} = ${x} + ${y};"),
        code_node.SimpleNode(template_text="print ${z};"),
    ])

    def make_symbol(name, template_text):
        def constructor(symbol_node):
            return code_node.SymbolDefinitionNode(
                symbol_node, template_text=template_text)

        return code_node.SymbolNode(
            name=name, definition_node_constructor=constructor)

    root_node.add_template_vars({
        'x': make_symbol('x', "int ${x} = 1;"),
        'y': make_symbol('y', "int ${y} = 2;"),
        'z': make_symbol('z', "int ${z};"),
    })

    prev = ''
    current = str(root_node)
    while current != prev:
        prev = current
        current = str(root_node)
    rendered_text = current

    format_result = clang_format(rendered_text, filename=filename)
    with open(filepath, 'w') as output_file:
        output_file.write(format_result.contents)
