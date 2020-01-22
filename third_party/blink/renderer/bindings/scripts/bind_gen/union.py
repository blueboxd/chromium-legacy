# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path

import web_idl

from . import name_style
from .code_node import ListNode
from .code_node import TextNode
from .code_node_cxx import CxxClassDefNode
from .code_node_cxx import CxxFuncDeclNode
from .code_node_cxx import CxxNamespaceNode
from .codegen_accumulator import CodeGenAccumulator
from .codegen_context import CodeGenContext
from .codegen_utils import component_export
from .codegen_utils import enclose_with_header_guard
from .codegen_utils import enclose_with_namespace
from .codegen_utils import make_copyright_header
from .codegen_utils import make_forward_declarations
from .codegen_utils import make_header_include_directives
from .codegen_utils import write_code_node_to_file
from .mako_renderer import MakoRenderer
from .path_manager import PathManager


def make_union_constructor_defs(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    class_name = cg_context.class_name

    return ListNode([
        CxxFuncDeclNode(
            name=class_name, arg_decls=[], return_type="", default=True),
        CxxFuncDeclNode(
            name=class_name,
            arg_decls=["const ${class_name}&"],
            return_type="",
            default=True),
        CxxFuncDeclNode(
            name=class_name,
            arg_decls=["${class_name}&&"],
            return_type="",
            default=True),
        CxxFuncDeclNode(
            name="~${class_name}",
            arg_decls=[],
            return_type="",
            override=True,
            default=True),
    ])


def make_union_class_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    union = cg_context.union
    component = union.components[0]

    class_def = CxxClassDefNode(
        cg_context.class_name, export=component_export(component))
    class_def.set_base_template_vars(cg_context.template_bindings())

    public_section = class_def.public_section
    public_section.extend([
        make_union_constructor_defs(cg_context),
        T(""),
        CxxFuncDeclNode(
            name="Trace", arg_decls=["Visitor*"], return_type="void"),
    ])

    return class_def


def generate_union(union):
    path_manager = PathManager(union)
    class_name = union.identifier
    cg_context = CodeGenContext(union=union, class_name=class_name)

    filename = "union_example.h"
    header_path = path_manager.api_path(filename=filename)

    header_node = ListNode(tail="\n")
    header_node.set_accumulator(CodeGenAccumulator())
    header_node.set_renderer(MakoRenderer())

    header_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))

    class_def = make_union_class_def(cg_context)

    header_node.extend([
        make_copyright_header(),
        TextNode(""),
        enclose_with_header_guard(
            ListNode([
                make_header_include_directives(header_node.accumulator),
                TextNode(""),
                header_blink_ns,
            ]), name_style.header_guard(header_path)),
    ])
    header_blink_ns.body.extend([
        make_forward_declarations(header_node.accumulator),
        TextNode(""),
    ])

    header_blink_ns.body.append(class_def)

    # Write down to the files.
    write_code_node_to_file(header_node, path_manager.gen_path_to(header_path))


def generate_unions(web_idl_database):
    union = list(web_idl_database.union_types)[0]
    generate_union(union)
