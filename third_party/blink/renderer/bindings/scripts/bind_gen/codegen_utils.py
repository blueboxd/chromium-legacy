# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl

from . import name_style
from .blink_v8_bridge import blink_type_info
from .clang_format import clang_format
from .code_node import CodeNode
from .code_node import EmptyNode
from .code_node import LiteralNode
from .code_node import SequenceNode
from .code_node import render_code_node
from .codegen_accumulator import CodeGenAccumulator
from .path_manager import PathManager


def make_copyright_header():
    return LiteralNode("""\
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.\
""")


def make_forward_declarations(accumulator):
    assert isinstance(accumulator, CodeGenAccumulator)

    class ForwardDeclarations(object):
        def __init__(self, accumulator):
            self._accumulator = accumulator

        def __str__(self):
            return "\n".join([
                "class {};".format(class_name)
                for class_name in sorted(self._accumulator.class_decls)
            ] + [
                "struct {};".format(struct_name)
                for struct_name in sorted(self._accumulator.struct_decls)
            ])

    return LiteralNode(ForwardDeclarations(accumulator))


def make_header_include_directives(accumulator):
    assert isinstance(accumulator, CodeGenAccumulator)

    class HeaderIncludeDirectives(object):
        def __init__(self, accumulator):
            self._accumulator = accumulator

        def __str__(self):
            return "\n".join([
                "#include \"{}\"".format(header)
                for header in sorted(self._accumulator.include_headers)
            ])

    return LiteralNode(HeaderIncludeDirectives(accumulator))


def component_export(component):
    assert isinstance(component, web_idl.Component)

    return name_style.macro(component, "EXPORT")


def component_export_header(component):
    assert isinstance(component, web_idl.Component)

    if component == "core":
        return "third_party/blink/renderer/core/core_export.h"
    elif component == "modules":
        return "third_party/blink/renderer/modules/modules_export.h"
    else:
        assert False


def enclose_with_header_guard(code_node, header_guard):
    assert isinstance(code_node, CodeNode)
    assert isinstance(header_guard, str)

    return SequenceNode([
        LiteralNode("#ifndef {}".format(header_guard)),
        LiteralNode("#define {}".format(header_guard)),
        EmptyNode(),
        code_node,
        EmptyNode(),
        LiteralNode("#endif  // {}".format(header_guard)),
    ])


def enclose_with_namespace(code_node, namespace):
    assert isinstance(code_node, CodeNode)
    assert isinstance(namespace, str)

    return SequenceNode([
        LiteralNode("namespace {} {{".format(namespace)),
        EmptyNode(),
        code_node,
        EmptyNode(),
        LiteralNode("}}  // namespace {}".format(namespace)),
    ])


def traverse_idl_types(idl_definition, callback):
    """
    Traverses in the given |idl_definition| to find all the web_idl.IdlType used
    in the IDL definition.  Invokes |callback| with each web_idl.IdlType.
    """
    assert callable(callback)

    def get(obj, attr):
        try:
            return getattr(obj, attr)
        except:
            return ()

    xs = (get(idl_definition, "attributes") + get(idl_definition, "constants")
          + get(idl_definition, "own_members") + (idl_definition, ))
    for x in xs:
        idl_type = get(x, "idl_type")
        if idl_type:
            callback(idl_type)

    xs = (get(idl_definition, "constructors") + get(idl_definition,
                                                    "operations"))
    for x in xs:
        for argument in x.arguments:
            callback(argument.idl_type)
        if x.return_type is not None:
            callback(x.return_type)

    xs = get(idl_definition, "flattened_member_types")
    for x in xs:
        callback(x)


def collect_include_headers(idl_definition):
    """
    Returns a list of include headers that are required by generated bindings of
    |idl_definition|.
    """
    type_def_objs = set()

    def collect_type_def_obj(idl_type):
        type_def_obj = idl_type.unwrap().type_definition_object
        if type_def_obj is not None:
            type_def_objs.add(type_def_obj)

    traverse_idl_types(idl_definition, collect_type_def_obj)

    header_paths = set(idl_definition.code_generator_info.blink_headers or [])
    for type_def_obj in type_def_objs:
        if isinstance(type_def_obj, web_idl.Enumeration):
            continue
        header_paths.add(PathManager(type_def_obj).api_path(ext="h"))

    return header_paths


def collect_include_headers_of_idl_types(idl_types):
    """
    Returns a set of header paths that are required by |idl_types|.
    """
    header_paths = set()

    def add_header_path(idl_type):
        assert isinstance(idl_type, web_idl.IdlType)

        if idl_type.is_numeric or idl_type.is_boolean or idl_type.is_typedef:
            pass
        elif idl_type.is_string or idl_type.is_enumeration:
            header_paths.add("third_party/blink/renderer/"
                             "platform/wtf/text/wtf_string.h")
        elif idl_type.is_buffer_source_type:
            basename = name_style.file("DOM", idl_type.type_name)
            header_paths.add("third_party/blink/renderer/core/typed_arrays/"
                             "{}.h".format(basename))
            header_paths.add(
                "third_party/blink/renderer/platform/heap/handle.h")
        elif idl_type.is_object or idl_type.is_any:
            header_paths.add("third_party/blink/renderer/"
                             "bindings/core/v8/script_value.h")
        elif idl_type.type_definition_object:
            header_paths.add(
                "third_party/blink/renderer/platform/heap/handle.h")
            type_def_obj = idl_type.type_definition_object
            header_paths.add(PathManager(type_def_obj).api_path(ext="h"))
        elif (idl_type.is_sequence or idl_type.is_frozen_array
              or idl_type.is_variadic or idl_type.is_record):
            header_paths.add("third_party/blink/renderer/"
                             "platform/wtf/vector.h")
            header_paths.add("third_party/blink/renderer/"
                             "platform/heap/heap_allocator.h")
        elif idl_type.is_promise:
            header_paths.add("third_party/blink/renderer/"
                             "bindings/core/v8/script_promise.h")
        elif idl_type.is_union:
            type_def_obj = idl_type.union_definition_object
            header_paths.add(PathManager(type_def_obj).api_path(ext="h"))
        elif idl_type.is_nullable:
            if not blink_type_info(idl_type.inner_type).has_null_value:
                header_paths.add("base/optional.h")
        else:
            assert False, "Unknown type: {}".format(idl_type.syntactic_form)

    for idl_type in idl_types:
        idl_type.apply_to_all_composing_elements(add_header_path)

    return header_paths


def write_code_node_to_file(code_node, filepath):
    """Renders |code_node| and then write the result to |filepath|."""
    assert isinstance(code_node, CodeNode)
    assert isinstance(filepath, str)

    rendered_text = render_code_node(code_node)

    format_result = clang_format(rendered_text, filename=filepath)

    web_idl.file_io.write_to_file_if_changed(filepath, format_result.contents)
