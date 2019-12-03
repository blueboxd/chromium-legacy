# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import os.path

import web_idl

from . import name_style
from .blink_v8_bridge import blink_class_name
from .blink_v8_bridge import blink_type_info
from .blink_v8_bridge import make_v8_to_blink_value
from .blink_v8_bridge import make_v8_to_blink_value_variadic
from .code_node import CodeNode
from .code_node import FunctionDefinitionNode
from .code_node import LiteralNode
from .code_node import SequenceNode
from .code_node import SymbolDefinitionNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .code_node import UnlikelyExitNode
from .codegen_accumulator import CodeGenAccumulator
from .codegen_context import CodeGenContext
from .codegen_expr import expr_from_exposure
from .codegen_expr import expr_or
from .codegen_format import format_template as _format
from .codegen_utils import collect_include_headers
from .codegen_utils import enclose_with_namespace
from .codegen_utils import make_copyright_header
from .codegen_utils import make_header_include_directives
from .codegen_utils import write_code_node_to_file
from .mako_renderer import MakoRenderer


def bind_blink_api_arguments(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    if cg_context.attribute_get:
        return

    if cg_context.attribute_set:
        name = "arg1_value"
        v8_value = "${info}[0]"
        code_node.register_code_symbol(
            make_v8_to_blink_value(name, v8_value,
                                   cg_context.attribute.idl_type))
        return

    for index, argument in enumerate(cg_context.function_like.arguments, 1):
        name = name_style.arg_f("arg{}_{}", index, argument.identifier)
        if argument.is_variadic:
            code_node.register_code_symbol(
                make_v8_to_blink_value_variadic(name, "${info}", index - 1,
                                                argument.idl_type))
        else:
            v8_value = "${{info}}[{}]".format(argument.index)
            code_node.register_code_symbol(
                make_v8_to_blink_value(name, v8_value, argument.idl_type,
                                       argument.default_value))


def bind_callback_local_vars(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    S = SymbolNode
    T = TextNode

    local_vars = []
    template_vars = {}

    local_vars.extend([
        S("class_like_name", ("const char* const ${class_like_name} = "
                              "\"${class_like.identifier}\";")),
        S("current_context", ("v8::Local<v8::Context> ${current_context} = "
                              "${isolate}->GetCurrentContext();")),
        S("current_execution_context",
          ("ExecutionContext* ${current_execution_context} = "
           "ExecutionContext::From(${current_script_state});")),
        S("current_script_state", ("ScriptState* ${current_script_state} = "
                                   "ScriptState::From(${current_context});")),
        S("execution_context", ("ExecutionContext* ${execution_context} = "
                                "ExecutionContext::From(${script_state});")),
        S("isolate", "v8::Isolate* ${isolate} = ${info}.GetIsolate();"),
        S("per_context_data", ("V8PerContextData* ${per_context_data} = "
                               "${script_state}->PerContextData();")),
        S("per_isolate_data", ("V8PerIsolateData* ${per_isolate_data} = "
                               "V8PerIsolateData::From(${isolate});")),
        S("property_name",
          "const char* const ${property_name} = \"${property.identifier}\";"),
        S("v8_receiver",
          "v8::Local<v8::Object> ${v8_receiver} = ${info}.This();"),
        S("receiver_context", ("v8::Local<v8::Context> ${receiver_context} = "
                               "${v8_receiver}->CreationContext();")),
        S("receiver_script_state",
          ("ScriptState* ${receiver_script_state} = "
           "ScriptState::From(${receiver_context});")),
    ])

    is_receiver_context = (cg_context.member_like
                           and not cg_context.member_like.is_static)

    # creation_context
    pattern = "const v8::Local<v8::Context>& ${creation_context} = {_1};"
    _1 = "${receiver_context}" if is_receiver_context else "${current_context}"
    local_vars.append(S("creation_context", _format(pattern, _1=_1)))

    # creation_context_object
    text = ("${v8_receiver}"
            if is_receiver_context else "${current_context}->Global()")
    template_vars["creation_context_object"] = T(text)

    # script_state
    pattern = "ScriptState* ${script_state} = {_1};"
    _1 = ("${receiver_script_state}"
          if is_receiver_context else "${current_script_state}")
    local_vars.append(S("script_state", _format(pattern, _1=_1)))

    # exception_state_context_type
    pattern = (
        "const ExceptionState::ContextType ${exception_state_context_type} = "
        "{_1};")
    if cg_context.attribute_get:
        _1 = "ExceptionState::kGetterContext"
    elif cg_context.attribute_set:
        _1 = "ExceptionState::kSetterContext"
    elif cg_context.constructor:
        _1 = "ExceptionState::kConstructionContext"
    else:
        _1 = "ExceptionState::kExecutionContext"
    local_vars.append(
        S("exception_state_context_type", _format(pattern, _1=_1)))

    # exception_state
    pattern = "ExceptionState ${exception_state}({_1});{_2}"
    _1 = [
        "${isolate}", "${exception_state_context_type}", "${class_like_name}",
        "${property_name}"
    ]
    _2 = ""
    if cg_context.return_type and cg_context.return_type.unwrap().is_promise:
        _2 = ("\n"
              "ExceptionToRejectPromiseScope reject_promise_scope"
              "(${info}, ${exception_state});")
    local_vars.append(
        S("exception_state", _format(pattern, _1=", ".join(_1), _2=_2)))

    # blink_receiver
    if cg_context.class_like.identifier == "Window":
        # TODO(yukishiino): Window interface should be
        # [ImplementedAs=LocalDOMWindow] instead of [ImplementedAs=DOMWindow],
        # and [CrossOrigin] properties should be implemented specifically with
        # DOMWindow class.  Then, we'll have less hacks.
        if "CrossOrigin" in cg_context.member_like.extended_attributes:
            text = ("DOMWindow* ${blink_receiver} = "
                    "${v8_class}::ToBlinkUnsafe(${v8_receiver});")
        else:
            text = ("LocalDOMWindow* ${blink_receiver} = To<LocalDOMWindow>("
                    "${v8_class}::ToBlinkUnsafe(${v8_receiver}));")
    else:
        pattern = ("{_1}* ${blink_receiver} = "
                   "${v8_class}::ToBlinkUnsafe(${v8_receiver});")
        _1 = blink_class_name(cg_context.class_like)
        text = _format(pattern, _1=_1)
    local_vars.append(S("blink_receiver", text))

    code_node.register_code_symbols(local_vars)
    code_node.add_template_vars(template_vars)


def _make_blink_api_call(cg_context, num_of_args=None):
    assert isinstance(cg_context, CodeGenContext)
    assert num_of_args is None or isinstance(num_of_args, (int, long))

    arguments = []
    ext_attrs = cg_context.member_like.extended_attributes

    values = ext_attrs.values_of("CallWith") + (
        ext_attrs.values_of("SetterCallWith") if cg_context.attribute_set else
        ())
    if "Isolate" in values:
        arguments.append("${isolate}")
    if "ScriptState" in values:
        arguments.append("${script_state}")
    if "ExecutionContext" in values:
        arguments.append("${execution_context}")

    if cg_context.attribute_get:
        pass
    elif cg_context.attribute_set:
        arguments.append("${arg1_value}")
    else:
        for index, argument in enumerate(cg_context.function_like.arguments):
            if num_of_args is not None and index == num_of_args:
                break
            name = name_style.arg_f("arg{}_{}", index + 1, argument.identifier)
            arguments.append(_format("${{{}}}", name))

    if cg_context.is_return_by_argument:
        arguments.append("${return_value}")

    if cg_context.may_throw_exception:
        arguments.append("${exception_state}")

    code_generator_info = cg_context.member_like.code_generator_info

    func_name = (code_generator_info.property_implemented_as
                 or name_style.api_func(cg_context.member_like.identifier))
    if cg_context.attribute_set:
        func_name = name_style.api_func("set", func_name)

    is_partial_or_mixin = (code_generator_info.defined_in_partial
                           or code_generator_info.defined_in_mixin)
    if cg_context.member_like.is_static or is_partial_or_mixin:
        class_like = cg_context.member_like.owner_mixin or cg_context.class_like
        class_name = (code_generator_info.receiver_implemented_as
                      or name_style.class_(class_like.identifier))
        func_designator = "{}::{}".format(class_name, func_name)
        if not cg_context.member_like.is_static:
            arguments.insert(0, "*${blink_receiver}")
    else:
        func_designator = _format("${blink_receiver}->{}", func_name)

    return _format("{_1}({_2})", _1=func_designator, _2=", ".join(arguments))


def bind_return_value(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    def create_definition(symbol_node):
        api_calls = []
        arguments = (cg_context.function_like.arguments
                     if cg_context.function_like else [])
        for index, arg in enumerate(arguments):
            if arg.is_optional and not arg.default_value:
                api_calls.append((index, _make_blink_api_call(
                    cg_context, index)))
        api_calls.append((None, _make_blink_api_call(cg_context)))

        nodes = []
        is_return_type_void = cg_context.return_type.unwrap().is_void
        if not is_return_type_void:
            return_type = blink_type_info(cg_context.return_type).value_t
        if len(api_calls) == 1:
            _, api_call = api_calls[0]
            if is_return_type_void:
                nodes.append(T(_format("{};", api_call)))
            elif cg_context.is_return_by_argument:
                nodes.append(T(_format("{} ${return_value};", return_type)))
                nodes.append(T(_format("{};", api_call)))
            else:
                nodes.append(
                    T(_format("const auto& ${return_value} = {};", api_call)))
        else:
            branches = SymbolScopeNode()
            for index, api_call in api_calls:
                if is_return_type_void or cg_context.is_return_by_argument:
                    assignment = api_call
                else:
                    assignment = _format("${return_value} = {}", api_call)
                if index is not None:
                    pattern = ("if (${info}[{index}]->IsUndefined()) {{\n"
                               "  {assignment};\n"
                               "  break;\n"
                               "}}")
                else:
                    pattern = "{assignment};"
                text = _format(pattern, index=index, assignment=assignment)
                branches.append(T(text))

            if not is_return_type_void:
                nodes.append(T(_format("{} ${return_value};", return_type)))
            nodes.append(T("do {  // Dummy loop for use of 'break'"))
            nodes.append(branches)
            nodes.append(T("} while (false);"))

        if cg_context.may_throw_exception:
            nodes.append(
                UnlikelyExitNode(
                    cond=T("${exception_state}.HadException()"),
                    body=SymbolScopeNode([T("return;")])))

        return SymbolDefinitionNode(symbol_node, nodes)

    code_node.register_code_symbol(
        SymbolNode("return_value", definition_constructor=create_definition))


def bind_v8_set_return_value(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    pattern = "{_1}({_2});"
    _1 = "V8SetReturnValue"
    _2 = ["${info}", "${return_value}"]

    return_type = cg_context.return_type.unwrap(nullable=True, typedef=True)
    if return_type.is_void:
        # Render a SymbolNode |return_value| discarding the content text, and
        # let a symbol definition be added.
        pattern = "<% str(return_value) %>"
    elif (cg_context.for_world == cg_context.MAIN_WORLD
          and return_type.is_interface):
        _1 = "V8SetReturnValueForMainWorld"
    elif return_type.is_interface:
        _2.append("${creation_context_object}")

    text = _format(pattern, _1=_1, _2=", ".join(_2))
    code_node.add_template_var("v8_set_return_value", TextNode(text))


_callback_common_binders = (
    bind_blink_api_arguments,
    bind_callback_local_vars,
    bind_return_value,
    bind_v8_set_return_value,
)


def make_check_receiver(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    if (cg_context.attribute
            and "LenientThis" in cg_context.attribute.extended_attributes):
        return SequenceNode([
            T("// [LenientThis]"),
            UnlikelyExitNode(
                cond=T(
                    "!${v8_class}::HasInstance(${v8_receiver}, ${isolate})"),
                body=SymbolScopeNode([T("return;")])),
        ])

    if cg_context.return_type.unwrap().is_promise:
        return SequenceNode([
            T("// Promise returning function: "
              "Convert a TypeError to a reject promise."),
            UnlikelyExitNode(
                cond=T(
                    "!${v8_class}::HasInstance(${v8_receiver}, ${isolate})"),
                body=SymbolScopeNode([
                    T("${exception_state}.ThrowTypeError("
                      "\"Illegal invocation\");"),
                    T("return;")
                ])),
        ])

    return None


def make_check_security_of_return_value(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    check_security = cg_context.member_like.extended_attributes.value_of(
        "CheckSecurity")
    if check_security != "ReturnValue":
        return None

    web_feature = _format(
        "WebFeature::{}",
        name_style.constant("CrossOrigin", cg_context.class_like.identifier,
                            cg_context.member_like.identifier))
    use_counter = _format(
        "UseCounter::Count(${current_execution_context}, {});", web_feature)
    cond = T("!BindingSecurity::ShouldAllowAccessTo("
             "ToLocalDOMWindow(${current_context}), ${return_value}, "
             "BindingSecurity::ErrorReportOption::kDoNotReport)")
    body = SymbolScopeNode([
        T(use_counter),
        T("V8SetReturnValueNull(${info});\n"
          "return;"),
    ])
    return SequenceNode([
        T("// [CheckSecurity=ReturnValue]"),
        UnlikelyExitNode(cond=cond, body=body),
    ])


def make_log_activity(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    ext_attrs = cg_context.member_like.extended_attributes
    if "LogActivity" not in ext_attrs:
        return None
    target = ext_attrs.value_of("LogActivity")
    if target:
        assert target in ("GetterOnly", "SetterOnly")
        if ((target == "GetterOnly" and not cg_context.attribute_get)
                or (target == "SetterOnly" and not cg_context.attribute_set)):
            return None
    if (cg_context.for_world == cg_context.MAIN_WORLD
            and "LogAllWorlds" not in ext_attrs):
        return None

    pattern = "{_1}${per_context_data} && ${per_context_data}->ActivityLogger()"
    _1 = ""
    if (cg_context.attribute and "PerWorldBindings" not in ext_attrs
            and "LogAllWorlds" not in ext_attrs):
        _1 = "${script_state}->World().IsIsolatedWorld() && "
    cond = _format(pattern, _1=_1)

    pattern = "${per_context_data}->ActivityLogger()->{_1}(\"{_2}.{_3}\"{_4});"
    _2 = cg_context.class_like.identifier
    _3 = cg_context.property_.identifier
    if cg_context.attribute_get:
        _1 = "LogGetter"
        _4 = ""
    if cg_context.attribute_set:
        _1 = "LogSetter"
        _4 = ", ${info}[0]"
    if cg_context.operation_group:
        _1 = "LogMethod"
        _4 = ", ${info}"
    body = _format(pattern, _1=_1, _2=_2, _3=_3, _4=_4)

    pattern = ("// [LogActivity], [LogAllWorlds]\n" "if ({_1}) {{ {_2} }}")
    node = TextNode(_format(pattern, _1=cond, _2=body))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/"
            "platform/bindings/v8_dom_activity_logger.h",
        ]))
    return node


def _make_overloaded_function_name(function_like):
    if isinstance(function_like, web_idl.Constructor):
        return name_style.func("constructor", "overload",
                               function_like.overload_index + 1)
    else:
        return name_style.func(function_like.identifier, "op", "overload",
                               function_like.overload_index + 1)


def _make_overload_dispatcher_per_arg_size(items):
    """
    https://heycam.github.io/webidl/#dfn-overload-resolution-algorithm

    Args:
        items: Partial list of an "effective overload set" with the same
            type list size.

    Returns:
        A pair of a resulting CodeNode and a boolean flag that is True if there
        exists a case that overload resolution will fail, i.e. a bailout that
        throws a TypeError is necessary.
    """
    # Variables shared with nested functions
    if len(items) > 1:
        arg_index = web_idl.OverloadGroup.distinguishing_argument_index(items)
    else:
        arg_index = None
    func_like = None
    dispatcher_nodes = SequenceNode()

    # True if there exists a case that overload resolution will fail.
    can_fail = True

    def find_test(item, test):
        # |test| is a callable that takes (t, u) where:
        #   t = the idl_type (in the original form)
        #   u = the unwrapped version of t
        idl_type = item.type_list[arg_index]
        t = idl_type
        u = idl_type.unwrap()
        return test(t, u) or (u.is_union and any(
            [test(m, m.unwrap()) for m in u.flattened_member_types]))

    def find(test):
        for item in items:
            if find_test(item, test):
                return item.function_like
        return None

    def find_all_interfaces():
        result = []  # [(func_like, idl_type), ...]
        for item in items:
            idl_type = item.type_list[arg_index].unwrap()
            if idl_type.is_interface:
                result.append((item.function_like, idl_type))
            if idl_type.is_union:
                for member_type in idl_type.flattened_member_types:
                    if member_type.unwrap().is_interface:
                        result.append((item.function_like,
                                       member_type.unwrap()))
        return result

    def make_node(pattern):
        value = _format("${info}[{}]", arg_index)
        func_name = _make_overloaded_function_name(func_like)
        return TextNode(_format(pattern, value=value, func_name=func_name))

    def dispatch_if(expr):
        if expr is True:
            pattern = "return {func_name}(${info});"
        else:
            pattern = ("if (" + expr + ") {{\n"
                       "  return {func_name}(${info});\n"
                       "}}")
        node = make_node(pattern)
        conditional = expr_from_exposure(func_like.exposure)
        if not conditional.is_always_true:
            node = SymbolScopeNode([
                TextNode("if (" + conditional.to_text() + ") {"),
                node,
                TextNode("}"),
            ])
        dispatcher_nodes.append(node)
        return expr is True and conditional.is_always_true

    if len(items) == 1:
        func_like = items[0].function_like
        can_fail = False
        return make_node("return {func_name}(${info});"), can_fail

    # 12.2. If V is undefined, ...
    func_like = find(lambda t, u: t.is_optional)
    if func_like:
        dispatch_if("{value}->IsUndefined()")

    # 12.3. if V is null or undefined, ...
    func_like = find(
        lambda t, u: t.does_include_nullable_type or u.is_dictionary)
    if func_like:
        dispatch_if("{value}->IsNullOrUndefined()")

    # 12.4. if V is a platform object, ...
    def inheritance_length(func_and_type):
        return len(func_and_type[1].type_definition_object.
                   inclusive_inherited_interfaces)

    # Attempt to match from most derived to least derived.
    for func_like, idl_type in sorted(
            find_all_interfaces(), key=inheritance_length, reverse=True):
        cgc = CodeGenContext(
            interface=idl_type.unwrap().type_definition_object)
        dispatch_if(
            _format("{}::HasInstance(${isolate}, {value})", cgc.v8_class))

    is_typedef_name = lambda t, name: t.is_typedef and t.identifier == name
    func_like_a = find(
        lambda t, u: is_typedef_name(t.unwrap(typedef=False),
                                     "ArrayBufferView"))
    func_like_b = find(
        lambda t, u: is_typedef_name(t.unwrap(typedef=False), "BufferSource"))
    if func_like_a or func_like_b:
        # V8 specific optimization: ArrayBufferView
        if func_like_a:
            func_like = func_like_a
            dispatch_if("{value}->IsArrayBufferView()")
        if func_like_b:
            func_like = func_like_b
            dispatch_if("{value}->IsArrayBufferView() || "
                        "{value}->IsArrayBuffer() || "
                        "{value}->IsSharedArrayBuffer()")
    else:
        # 12.5. if Type(V) is Object, V has an [[ArrayBufferData]] internal
        #   slot, ...
        func_like = find(lambda t, u: u.is_array_buffer)
        if func_like:
            dispatch_if("{value}->IsArrayBuffer() || "
                        "{value}->IsSharedArrayBuffer()")

        # 12.6. if Type(V) is Object, V has a [[DataView]] internal slot, ...
        func_like = find(lambda t, u: u.is_data_view)
        if func_like:
            dispatch_if("{value}->IsDataView()")

        # 12.7. if Type(V) is Object, V has a [[TypedArrayName]] internal slot,
        #   ...
        func_like = find(lambda t, u: u.is_typed_array_type)
        if func_like:
            dispatch_if("{value}->IsTypedArray()")

    # 12.8. if IsCallable(V) is true, ...
    func_like = find(lambda t, u: u.is_callback_function)
    if func_like:
        dispatch_if("{value}->IsFunction()")

    # 12.9. if Type(V) is Object and ... @@iterator ...
    func_like = find(lambda t, u: u.is_sequence or u.is_frozen_array)
    if func_like:
        dispatch_if("{value}->IsArray() || "  # Excessive optimization
                    "bindings::IsEsIterableObject"
                    "(${isolate}, {value}, ${exception_state})")
        dispatcher_nodes.append(
            TextNode("if (${exception_state}.HadException()) {\n"
                     "  return;\n"
                     "}"))

    # 12.10. if Type(V) is Object and ...
    def is_es_object_type(t, u):
        return (u.is_callback_interface or u.is_dictionary or u.is_record
                or u.is_object)

    func_like = find(is_es_object_type)
    if func_like:
        dispatch_if("{value}->IsObject()")

    # 12.11. if Type(V) is Boolean and ...
    func_like = find(lambda t, u: u.is_boolean)
    if func_like:
        dispatch_if("{value}->IsBoolean()")

    # 12.12. if Type(V) is Number and ...
    func_like = find(lambda t, u: u.is_numeric)
    if func_like:
        dispatch_if("{value}->IsNumber()")

    # 12.13. if there is an entry in S that has ... a string type ...
    # 12.14. if there is an entry in S that has ... a numeric type ...
    # 12.15. if there is an entry in S that has ... boolean ...
    # 12.16. if there is an entry in S that has any ...
    func_likes = [
        find(lambda t, u: u.is_string),
        find(lambda t, u: u.is_numeric),
        find(lambda t, u: u.is_boolean),
        find(lambda t, u: u.is_any),
    ]
    for func_like in func_likes:
        if func_like:
            if dispatch_if(True):
                can_fail = False
                break

    return dispatcher_nodes, can_fail


def make_overload_dispatcher(cg_context):
    # https://heycam.github.io/webidl/#dfn-overload-resolution-algorithm

    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    overload_group = cg_context.property_
    items = overload_group.effective_overload_set()
    args_size = lambda item: len(item.type_list)
    items_grouped_by_arg_size = itertools.groupby(
        sorted(items, key=args_size, reverse=True), key=args_size)

    branches = SequenceNode()
    did_use_break = False
    for arg_size, items in items_grouped_by_arg_size:
        items = list(items)

        node, can_fail = _make_overload_dispatcher_per_arg_size(items)

        if arg_size > 0:
            node = SymbolScopeNode([
                T("if (${info}.Length() >= " + str(arg_size) + ") {"),
                node,
                T("break;") if can_fail else None,
                T("}"),
            ])
            did_use_break = did_use_break or can_fail

        terms = map(
            lambda item: expr_from_exposure(item.function_like.exposure),
            items)
        conditional = expr_or(terms)
        if not conditional.is_always_true:
            node = SymbolScopeNode([
                T("if (" + conditional.to_text() + ") {"),
                node,
                T("}"),
            ])

        branches.append(node)

    if did_use_break:
        branches = SymbolScopeNode([
            T("do {  // Dummy loop for use of 'break'"),
            branches,
            T("} while (false);"),
        ])
    # Make the entire branches an indivisible chunk so that SymbolDefinitionNode
    # will not be inserted in-between.
    branches = LiteralNode(branches)

    if not did_use_break and arg_size == 0 and conditional.is_always_true:
        return branches

    return SequenceNode([
        branches,
        T(""),
        T("${exception_state}.ThrowTypeError"
          "(\"Overload resolution failed.\");\n"
          "return;"),
    ])


def make_report_deprecate_as(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    name = cg_context.member_like.extended_attributes.value_of("DeprecateAs")
    if not name:
        return None

    pattern = ("// [DeprecateAs]\n"
               "Deprecation::CountDeprecation("
               "${execution_context}, WebFeature::k{_1});")
    _1 = name
    node = TextNode(_format(pattern, _1=_1))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/frame/deprecation.h",
        ]))
    return node


def make_report_measure_as(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    ext_attrs = cg_context.member_like.extended_attributes
    if not ("Measure" in ext_attrs or "MeasureAs" in ext_attrs):
        assert "HighEntropy" not in ext_attrs, "{}: {}".format(
            cg_context.idl_location_and_name,
            "[HighEntropy] must be specified with either [Measure] or "
            "[MeasureAs].")
        return None

    suffix = ""
    if cg_context.attribute_get:
        suffix = "_AttributeGetter"
    elif cg_context.attribute_set:
        suffix = "_AttributeSetter"
    elif cg_context.constructor:
        suffix = "_Constructor"
    elif cg_context.operation:
        suffix = "_Method"
    name = cg_context.member_like.extended_attributes.value_of("MeasureAs")
    if name:
        name = "k{}".format(name)
    elif cg_context.constructor:
        name = "kV8{}{}".format(cg_context.class_like.identifier, suffix)
    else:
        name = "kV8{}_{}{}".format(
            cg_context.class_like.identifier,
            name_style.raw.upper_camel_case(cg_context.member_like.identifier),
            suffix)

    node = SequenceNode()

    pattern = ("// [Measure], [MeasureAs]\n"
               "UseCounter::Count(${execution_context}, WebFeature::{_1});")
    _1 = name
    node.append(TextNode(_format(pattern, _1=_1)))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/frame/web_feature.h",
            "third_party/blink/renderer/platform/instrumentation/use_counter.h",
        ]))

    if "HighEntropy" not in ext_attrs or cg_context.attribute_set:
        return node

    pattern = (
        "// [HighEntropy]\n"
        "Dactyloscoper::Record(${execution_context}, WebFeature::{_1});")
    _1 = name
    node.append(TextNode(_format(pattern, _1=_1)))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/frame/dactyloscoper.h",
        ]))

    return node


def make_return_value_cache_return_early(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    pred = cg_context.member_like.extended_attributes.value_of(
        "CachedAttribute")
    if pred:
        return TextNode("""\
// [CachedAttribute]
static const V8PrivateProperty::SymbolKey kPrivatePropertyCachedAttribute;
auto v8_private_cached_attribute =
    V8PrivateProperty::GetSymbol(${isolate}, kPrivatePropertyCachedAttribute);
if (!impl->""" + pred + """()) {
  v8::Local<v8::Value> v8_value;
  if (v8_private_cached_attribute.GetOrUndefined(${v8_receiver})
          .ToLocal(&v8_value) && !v8_value->IsUndefined()) {
    V8SetReturnValue(${info}, v8_value);
    return;
  }
}""")

    if "SaveSameObject" in cg_context.member_like.extended_attributes:
        return TextNode("""\
// [SaveSameObject]
static const V8PrivateProperty::SymbolKey kPrivatePropertySaveSameObject;
auto v8_private_save_same_object =
    V8PrivateProperty::GetSymbol(${isolate}, kPrivatePropertySaveSameObject);
{
  v8::Local<v8::Value> v8_value;
  if (v8_private_save_same_object.GetOrUndefined(${v8_receiver})
          .ToLocal(&v8_value) && !v8_value->IsUndefined()) {
    V8SetReturnValue(${info}, v8_value);
    return;
  }
}""")


def make_return_value_cache_update_value(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    if "CachedAttribute" in cg_context.member_like.extended_attributes:
        return TextNode("// [CachedAttribute]\n"
                        "v8_private_cached_attribute.Set"
                        "(${v8_receiver}, ${info}.GetReturnValue().Get());")

    if "SaveSameObject" in cg_context.member_like.extended_attributes:
        return TextNode("// [SaveSameObject]\n"
                        "v8_private_save_same_object.Set"
                        "(${v8_receiver}, ${info}.GetReturnValue().Get());")


def make_runtime_call_timer_scope(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    pattern = "RUNTIME_CALL_TIMER_SCOPE{_1}(${isolate}, {_2});"
    _1 = "_DISABLED_BY_DEFAULT"
    suffix = ""
    if cg_context.attribute_get:
        suffix = "_Getter"
    elif cg_context.attribute_set:
        suffix = "_Setter"
    counter = cg_context.member_like.extended_attributes.value_of(
        "RuntimeCallStatsCounter")
    if counter:
        _2 = "k{}{}".format(counter, suffix)
    else:
        _2 = "\"Blink_{}_{}{}\"".format(
            blink_class_name(cg_context.class_like),
            cg_context.member_like.identifier, suffix)
    node = TextNode(_format(pattern, _1=_1, _2=_2))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/platform/bindings/runtime_call_stats.h",
        ]))
    return node


def make_attribute_get_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    T = TextNode

    cg_context = cg_context.make_copy(attribute_get=True)

    func_def = FunctionDefinitionNode(
        name=T(function_name),
        arg_decls=[T("const v8::FunctionCallbackInfo<v8::Value>& info")],
        return_type=T("void"))

    body = func_def.body
    body.add_template_var("info", "info")
    body.add_template_vars(cg_context.template_bindings())

    for bind in _callback_common_binders:
        bind(body, cg_context)

    body.extend([
        make_runtime_call_timer_scope(cg_context),
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        T(""),
        make_check_receiver(cg_context),
        make_return_value_cache_return_early(cg_context),
        T(""),
        make_check_security_of_return_value(cg_context),
        T("${v8_set_return_value}"),
        make_return_value_cache_update_value(cg_context),
    ])

    return func_def


def make_attribute_set_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    return None


def make_operation_function_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    T = TextNode

    func_def = FunctionDefinitionNode(
        name=T(function_name),
        arg_decls=[T("const v8::FunctionCallbackInfo<v8::Value>& info")],
        return_type=T("void"))

    body = func_def.body
    body.add_template_var("info", "info")
    body.add_template_vars(cg_context.template_bindings())

    for bind in _callback_common_binders:
        bind(body, cg_context)

    body.extend([
        make_runtime_call_timer_scope(cg_context),
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        T(""),
        make_check_receiver(cg_context),
        T(""),
        T("${v8_set_return_value}"),
    ])

    return func_def


def make_overload_dispatcher_function_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    T = TextNode

    func_def = FunctionDefinitionNode(
        name=T(function_name),
        arg_decls=[T("const v8::FunctionCallbackInfo<v8::Value>& info")],
        return_type=T("void"))

    body = func_def.body
    body.add_template_var("info", "info")
    body.add_template_vars(cg_context.template_bindings())

    bind_callback_local_vars(body, cg_context)

    body.append(make_overload_dispatcher(cg_context))

    return func_def


def make_operation_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    operation_group = cg_context.constructor_group or cg_context.operation_group

    if len(operation_group) == 1:
        return make_operation_function_def(
            cg_context.make_copy(operation=operation_group[0]), function_name)

    node = SequenceNode()
    for operation in operation_group:
        node.append(
            make_operation_function_def(
                cg_context.make_copy(operation=operation),
                _make_overloaded_function_name(operation)))
    node.append(
        make_overload_dispatcher_function_def(cg_context, function_name))
    return node


def bind_template_installer_local_vars(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    S = SymbolNode

    local_vars = []

    local_vars.extend([
        S("instance_template",
          ("v8::Local<v8::ObjectTemplate> ${instance_template} = "
           "${interface_template}->InstanceTemplate();")),
        S("prototype_template",
          ("v8::Local<v8::ObjectTemplate> ${prototype_template} = "
           "${interface_template}->PrototypeTemplate();")),
        S("signature",
          ("v8::Local<v8::Signature> ${signature} = "
           "v8::Signature::New(${isolate}, ${interface_template});")),
        S("wrapper_type_info",
          ("const WrapperTypeInfo* const ${wrapper_type_info} = "
           "${v8_class}::GetWrapperTypeInfo();")),
    ])

    pattern = (
        "v8::Local<v8::FunctionTemplate> ${parent_interface_template}{_1};")
    _1 = (" = ${wrapper_type_info}->parent_class->dom_template_function"
          "(${isolate}, ${world})")
    if not cg_context.class_like.inherited:
        _1 = ""
    local_vars.append(S("parent_interface_template", _format(pattern, _1=_1)))

    code_node.register_code_symbols(local_vars)


def make_install_interface_template_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    func_def = FunctionDefinitionNode(
        name=T("InstallInterfaceTemplate"),
        arg_decls=[
            T("v8::Isolate* isolate"),
            T("const DOMWrapperWorld& world"),
            T("v8::Local<v8::FunctionTemplate> interface_template"),
        ],
        return_type=T("void"))

    body = func_def.body
    body.add_template_var("isolate", "isolate")
    body.add_template_var("world", "world")
    body.add_template_var("interface_template", "interface_template")
    body.add_template_vars(cg_context.template_bindings())

    binders = [
        bind_template_installer_local_vars,
    ]
    for bind in binders:
        bind(body, cg_context)

    body.extend([
        T("V8DOMConfiguration::InitializeDOMInterfaceTemplate("
          "${isolate}, ${interface_template}, "
          "${wrapper_type_info}->interface_name, ${parent_interface_template}, "
          "kV8DefaultWrapperInternalFieldCount);"),
    ])

    if cg_context.class_like.constructor_groups:
        body.extend([
            T("${interface_template}->SetCallHandler(ConstructorCallback);"),
            T("${interface_template}->SetLength("
              "${class_like.constructor_groups[0]"
              ".min_num_of_required_arguments});"),
        ])

    return func_def


def generate_interfaces(web_idl_database, output_dirs):
    filename = "v8_example_interface.cc"
    filepath = os.path.join(output_dirs['core'], filename)

    interface = web_idl_database.find("TestNamespace")

    cg_context = CodeGenContext(interface=interface)

    root_node = SymbolScopeNode(separator_last="\n")
    root_node.set_accumulator(CodeGenAccumulator())
    root_node.set_renderer(MakoRenderer())

    root_node.accumulator.add_include_headers(
        collect_include_headers(interface))

    code_node = SequenceNode()

    for attribute in interface.attributes:
        func_name = name_style.func(attribute.identifier,
                                    "AttributeGetCallback")
        code_node.append(
            make_attribute_get_callback_def(
                cg_context.make_copy(attribute=attribute), func_name))
        func_name = name_style.func(attribute.identifier,
                                    "AttributeSetCallback")
        code_node.append(
            make_attribute_set_callback_def(
                cg_context.make_copy(attribute=attribute), func_name))

    for constructor_group in interface.constructor_groups:
        func_name = name_style.func("ConstructorCallback")
        code_node.append(
            make_operation_callback_def(
                cg_context.make_copy(constructor_group=constructor_group),
                func_name))

    for operation_group in interface.operation_groups:
        func_name = name_style.func(operation_group.identifier,
                                    "OperationCallback")
        code_node.append(
            make_operation_callback_def(
                cg_context.make_copy(operation_group=operation_group),
                func_name))

    code_node.append(make_install_interface_template_def(cg_context))

    root_node.extend([
        make_copyright_header(),
        TextNode(""),
        make_header_include_directives(root_node.accumulator),
        TextNode(""),
        enclose_with_namespace(code_node, name_style.namespace("blink")),
    ])

    write_code_node_to_file(root_node, filepath)
