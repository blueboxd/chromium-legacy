# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This module provides C++ language specific implementations of
code_node.CodeNode.
"""

from .code_node import CodeNode
from .code_node import CompositeNode
from .code_node import Likeliness
from .code_node import ListNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .codegen_expr import CodeGenExpr


class CxxBlockNode(CompositeNode):
    def __init__(self, body):
        template_format = ("{{\n" "  {body}\n" "}}")

        CompositeNode.__init__(
            self,
            template_format,
            body=_to_symbol_scope_node(body, Likeliness.ALWAYS))


class CxxIfNode(CompositeNode):
    def __init__(self, cond, body, likeliness):
        template_format = ("if ({cond}) {{\n" "  {body}\n" "}}")

        CompositeNode.__init__(
            self,
            template_format,
            cond=_to_conditional_node(cond),
            body=_to_symbol_scope_node(body, likeliness))


class CxxIfElseNode(CompositeNode):
    def __init__(self, cond, then, then_likeliness, else_, else_likeliness):
        template_format = ("if ({cond}) {{\n"
                           "  {then}\n"
                           "}} else {{\n"
                           "  {else_}\n"
                           "}}")

        CompositeNode.__init__(
            self,
            template_format,
            cond,
            then=_to_symbol_scope_node(then, then_likeliness),
            else_=_to_symbol_scope_node(else_, else_likeliness))


class CxxLikelyIfNode(CxxIfNode):
    def __init__(self, cond, body):
        CxxIfNode.__init__(self, cond, body, Likeliness.LIKELY)


class CxxUnlikelyIfNode(CxxIfNode):
    def __init__(self, cond, body):
        CxxIfNode.__init__(self, cond, body, Likeliness.UNLIKELY)


class CxxBreakableBlockNode(CompositeNode):
    def __init__(self, body, likeliness=Likeliness.LIKELY):
        template_format = ("do {{  // Dummy loop for use of 'break'.\n"
                           "  {body}\n"
                           "}} while (false);")

        CompositeNode.__init__(
            self,
            template_format,
            body=_to_symbol_scope_node(body, likeliness))


class CxxFuncDeclNode(CompositeNode):
    def __init__(self,
                 name,
                 arg_decls,
                 return_type,
                 const=False,
                 override=False,
                 default=False,
                 delete=False):
        """
        Args:
            name: Function name node, which may include nested-name-specifier
                (i.e. 'namespace_name::' and/or 'class_name::').
            arg_decls: List of argument declarations.
            return_type: Return type.
            const: True makes this a const function.
            override: True makes this an overriding function.
            default: True makes this have the default implementation.
            delete: True makes this function be deleted.
        """
        assert isinstance(const, bool)
        assert isinstance(override, bool)
        assert isinstance(default, bool)
        assert isinstance(delete, bool)
        assert not (default and delete)

        template_format = ("{return_type} {name}({arg_decls})"
                           "{const}"
                           "{override}"
                           "{default_or_delete}"
                           ";")

        const = " const" if const else ""
        override = " override" if override else ""
        if default:
            default_or_delete = " = default"
        elif delete:
            default_or_delete = " = delete"
        else:
            default_or_delete = ""

        CompositeNode.__init__(
            self,
            template_format,
            name=name,
            arg_decls=ListNode(
                _to_node_list(arg_decls, TextNode), separator=", "),
            return_type=return_type,
            const=const,
            override=override,
            default_or_delete=default_or_delete)


class CxxFuncDefNode(CompositeNode):
    def __init__(self,
                 name,
                 arg_decls,
                 return_type,
                 const=False,
                 override=False,
                 member_initializer_list=None):
        """
        Args:
            name: Function name node, which may include nested-name-specifier
                (i.e. 'namespace_name::' and/or 'class_name::').
            arg_decls: List of argument declarations.
            return_type: Return type.
            const: True makes this a const function.
            override: True makes this an overriding function.
            member_initializer_list: List of member initializers.
        """
        assert isinstance(const, bool)
        assert isinstance(override, bool)

        template_format = ("{return_type} {name}({arg_decls})"
                           "{const}"
                           "{override}"
                           "{member_initializer_list} {{\n"
                           "  {body}\n"
                           "}}")

        const = " const" if const else ""
        override = " override" if override else ""

        if member_initializer_list is None:
            member_initializer_list = ""
        else:
            initializers = ListNode(
                _to_node_list(member_initializer_list, TextNode),
                separator=", ")
            member_initializer_list = ListNode([TextNode(" : "), initializers],
                                               separator="")

        self._body_node = SymbolScopeNode()

        CompositeNode.__init__(
            self,
            template_format,
            name=name,
            arg_decls=ListNode(
                _to_node_list(arg_decls, TextNode), separator=", "),
            return_type=return_type,
            const=const,
            override=override,
            member_initializer_list=member_initializer_list,
            body=self._body_node)

    @property
    def body(self):
        return self._body_node


def _to_conditional_node(cond):
    if isinstance(cond, CodeNode):
        return cond
    elif isinstance(cond, CodeGenExpr):
        return TextNode(cond.to_text())
    elif isinstance(cond, str):
        return TextNode(cond)
    else:
        assert False


def _to_node_list(iterable, constructor):
    return map(lambda x: x if isinstance(x, CodeNode) else constructor(x),
               iterable)


def _to_symbol_scope_node(node, likeliness):
    if isinstance(node, SymbolScopeNode):
        pass
    elif isinstance(node, CodeNode):
        node = SymbolScopeNode([node])
    elif isinstance(node, (list, tuple)):
        node = SymbolScopeNode(node)
    else:
        assert False
    node.set_likeliness(likeliness)
    return node
