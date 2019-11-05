# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .code_node import CodeNode
from .code_node import SymbolDefinitionNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .code_node import UnlikelyExitNode
import web_idl

_format = CodeNode.format_template


def make_v8_to_blink_value(blink_var_name, v8_value_expr, idl_type):
    """
    Returns a SymbolNode whose definition converts a v8::Value to a Blink value.
    """
    assert isinstance(blink_var_name, str)
    assert isinstance(v8_value_expr, str)
    assert isinstance(idl_type, web_idl.IdlType)

    pattern = "NativeValueTraits<{_1}>::NativeValue({_2})"
    _1 = "IDL{}".format(idl_type.type_name)
    _2 = ["${isolate}", v8_value_expr, "${exception_state}"]

    blink_value = _format(pattern, _1=_1, _2=", ".join(_2))
    idl_type_tag = _1

    pattern = "{_1}& ${{{_2}}} = {_3};"
    _1 = "NativeValueTraits<{}>::ImplType".format(idl_type_tag)
    _2 = blink_var_name
    _3 = blink_value
    text = _format(pattern, _1=_1, _2=_2, _3=_3)

    def create_definition(symbol_node):
        return SymbolDefinitionNode(symbol_node, [
            TextNode(text),
            UnlikelyExitNode(
                cond=TextNode("${exception_state}.HadException()"),
                body=SymbolScopeNode([TextNode("return;")])),
        ])

    return SymbolNode(blink_var_name, definition_constructor=create_definition)
