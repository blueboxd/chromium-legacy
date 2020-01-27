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
from .blink_v8_bridge import v8_bridge_class_name
from .code_node import EmptyNode
from .code_node import ListNode
from .code_node import SequenceNode
from .code_node import SymbolDefinitionNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .code_node_cxx import CxxBlockNode
from .code_node_cxx import CxxBreakableBlockNode
from .code_node_cxx import CxxClassDefNode
from .code_node_cxx import CxxFuncDeclNode
from .code_node_cxx import CxxFuncDefNode
from .code_node_cxx import CxxLikelyIfNode
from .code_node_cxx import CxxMultiBranchesNode
from .code_node_cxx import CxxNamespaceNode
from .code_node_cxx import CxxUnlikelyIfNode
from .codegen_accumulator import CodeGenAccumulator
from .codegen_context import CodeGenContext
from .codegen_expr import CodeGenExpr
from .codegen_expr import expr_from_exposure
from .codegen_expr import expr_or
from .codegen_format import format_template as _format
from .codegen_utils import component_export
from .codegen_utils import component_export_header
from .codegen_utils import enclose_with_header_guard
from .codegen_utils import make_copyright_header
from .codegen_utils import make_forward_declarations
from .codegen_utils import make_header_include_directives
from .codegen_utils import write_code_node_to_file
from .mako_renderer import MakoRenderer
from .path_manager import PathManager


def _is_none_or_str(arg):
    return arg is None or isinstance(arg, str)


def callback_function_name(cg_context, overload_index=None):
    assert isinstance(cg_context, CodeGenContext)

    def _cxx_name(name):
        """
        Returns a property name that the bindings generator can use in
        generated code.

        Note that Web IDL allows '-' (hyphen-minus) and '_' (low line) in
        identifiers but C++ does not allow or recommend them.  This function
        encodes these characters.
        """
        # In Python3, we can use str.maketrans and str.translate.
        #
        # We're optimistic about name conflict.  It's highly unlikely that
        # these replacements will cause a conflict.
        assert "Dec45" not in name
        assert "Dec95" not in name
        name = name.replace("-", "Dec45")
        name = name.replace("_", "Dec95")
        return name

    if cg_context.constant:
        property_name = cg_context.property_.identifier
    else:
        property_name = _cxx_name(cg_context.property_.identifier)

    if cg_context.attribute_get:
        kind = "AttributeGet"
    elif cg_context.attribute_set:
        kind = "AttributeSet"
    elif cg_context.constant:
        kind = "Constant"
    elif cg_context.constructor_group:
        property_name = ""
        kind = "Constructor"
    elif cg_context.exposed_construct:
        kind = "ExposedConstruct"
    elif cg_context.operation_group:
        kind = "Operation"

    if overload_index is None:
        callback_suffix = "Callback"
    else:
        callback_suffix = "Overload{}".format(overload_index + 1)

    if cg_context.for_world == CodeGenContext.MAIN_WORLD:
        world_suffix = "ForMainWorld"
    elif cg_context.for_world == CodeGenContext.NON_MAIN_WORLDS:
        world_suffix = "ForNonMainWorlds"
    elif cg_context.for_world == CodeGenContext.ALL_WORLDS:
        world_suffix = ""

    return name_style.func(property_name, kind, callback_suffix, world_suffix)


def constant_name(cg_context):
    assert isinstance(cg_context, CodeGenContext)
    assert cg_context.constant

    property_name = cg_context.property_.identifier.lower()

    kind = "Constant"

    return name_style.constant(kind, property_name)


def custom_function_name(cg_context, overload_index=None):
    assert isinstance(cg_context, CodeGenContext)

    if cg_context.attribute_get:
        suffix = "AttributeGetterCustom"
    elif cg_context.attribute_set:
        suffix = "AttributeSetterCustom"
    elif cg_context.operation_group:
        suffix = "MethodCustom"
    else:
        assert False

    return name_style.func(cg_context.property_.identifier, suffix)


# ----------------------------------------------------------------------------
# Callback functions
# ----------------------------------------------------------------------------


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
                make_v8_to_blink_value(
                    name,
                    v8_value,
                    argument.idl_type,
                    argument_index=index,
                    default_value=argument.default_value))


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
        if (not cg_context.member_like or
                "CrossOrigin" in cg_context.member_like.extended_attributes):
            text = ("DOMWindow* ${blink_receiver} = "
                    "${class_name}::ToBlinkUnsafe(${v8_receiver});")
        else:
            text = ("LocalDOMWindow* ${blink_receiver} = To<LocalDOMWindow>("
                    "${class_name}::ToBlinkUnsafe(${v8_receiver}));")
    else:
        pattern = ("{_1}* ${blink_receiver} = "
                   "${class_name}::ToBlinkUnsafe(${v8_receiver});")
        _1 = blink_class_name(cg_context.class_like)
        text = _format(pattern, _1=_1)
    local_vars.append(S("blink_receiver", text))

    code_node.register_code_symbols(local_vars)
    code_node.add_template_vars(template_vars)


def _make_reflect_content_attribute_key(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    name = (cg_context.attribute.extended_attributes.value_of("Reflect")
            or cg_context.attribute.identifier.lower())
    if cg_context.attribute_get and name in ("class", "id", "name"):
        return None

    if cg_context.class_like.identifier.startswith("SVG"):
        namespace = "svg_names"
        code_node.accumulate(
            CodeGenAccumulator.require_include_headers(
                ["third_party/blink/renderer/core/svg_names.h"]))
    else:
        namespace = "html_names"
        code_node.accumulate(
            CodeGenAccumulator.require_include_headers(
                ["third_party/blink/renderer/core/html_names.h"]))
    return "{}::{}".format(namespace, name_style.constant(name, "attr"))


def _make_reflect_accessor_func_name(cg_context):
    assert isinstance(cg_context, CodeGenContext)
    assert cg_context.attribute_get or cg_context.attribute_set

    if cg_context.attribute_get:
        name = (cg_context.attribute.extended_attributes.value_of("Reflect")
                or cg_context.attribute.identifier.lower())
        if name in ("class", "id", "name"):
            return name_style.func("get", name, "attribute")

        if "URL" in cg_context.attribute.extended_attributes:
            return "GetURLAttribute"

    FAST_ACCESSORS = {
        "boolean": ("FastHasAttribute", "SetBooleanAttribute"),
        "long": ("GetIntegralAttribute", "SetIntegralAttribute"),
        "unsigned long": ("GetUnsignedIntegralAttribute",
                          "SetUnsignedIntegralAttribute"),
    }
    idl_type = cg_context.attribute.idl_type.unwrap()
    accessors = FAST_ACCESSORS.get(idl_type.keyword_typename)
    if accessors:
        return accessors[0 if cg_context.attribute_get else 1]

    if (idl_type.is_interface
            and idl_type.type_definition_object.does_implement("Element")):
        if cg_context.attribute_get:
            return "GetElementAttribute"
        else:
            return "SetElementAttribute"

    if idl_type.element_type:
        element_type = idl_type.element_type.unwrap()
        if (element_type.is_interface and
                element_type.type_definition_object.does_implement("Element")):
            if cg_context.attribute_get:
                return "GetElementArrayAttribute"
            else:
                return "SetElementArrayAttribute"

    if cg_context.attribute_get:
        return "FastGetAttribute"
    else:
        return "setAttribute"


def _make_reflect_process_keyword_state(cg_context):
    # https://html.spec.whatwg.org/C/#keywords-and-enumerated-attributes

    assert isinstance(cg_context, CodeGenContext)
    assert cg_context.attribute_get or cg_context.attribute_set

    T = TextNode
    F = lambda *args, **kwargs: T(_format(*args, **kwargs))

    if not cg_context.attribute_get:
        return None

    ext_attrs = cg_context.attribute.extended_attributes
    keywords = ext_attrs.values_of("ReflectOnly")
    missing_default = ext_attrs.value_of("ReflectMissing")
    empty_default = ext_attrs.value_of("ReflectEmpty")
    invalid_default = ext_attrs.value_of("ReflectInvalid")

    def constant(keyword):
        if not keyword:
            return "g_empty_atom"
        return "keywords::{}".format(name_style.constant(keyword))

    branches = CxxMultiBranchesNode()
    branches.accumulate(
        CodeGenAccumulator.require_include_headers(
            ["third_party/blink/renderer/core/keywords.h"]))
    nodes = [
        T("// [ReflectOnly]"),
        T("const AtomicString reflect_value(${return_value}.LowerASCII());"),
        branches,
    ]

    if missing_default is not None:
        branches.append(
            cond="reflect_value.IsNull()",
            body=F("${return_value} = {};", constant(missing_default)))
    elif cg_context.return_type.unwrap(nullable=False).is_nullable:
        branches.append(
            cond="reflect_value.IsNull()",
            body=T("// Null string to IDL null."))

    if empty_default is not None:
        branches.append(
            cond="reflect_value.IsEmpty()",
            body=F("${return_value} = {};", constant(empty_default)))

    expr = " || ".join(
        map(lambda keyword: "reflect_value == {}".format(constant(keyword)),
            keywords))
    branches.append(cond=expr, body=T("${return_value} = reflect_value;"))

    if invalid_default is not None:
        branches.append(
            cond=True,
            body=F("${return_value} = {};", constant(invalid_default)))
    else:
        branches.append(
            cond=True, body=F("${return_value} = {};", constant("")))

    return SequenceNode(nodes)


def _make_blink_api_call(code_node, cg_context, num_of_args=None):
    assert isinstance(code_node, SymbolScopeNode)
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

    code_generator_info = cg_context.member_like.code_generator_info
    is_partial = code_generator_info.defined_in_partial
    if (is_partial and
            not (cg_context.constructor or cg_context.member_like.is_static)):
        arguments.append("*${blink_receiver}")

    if "Reflect" in ext_attrs:  # [Reflect]
        key = _make_reflect_content_attribute_key(code_node, cg_context)
        if key:
            arguments.append(key)

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

    func_name = (code_generator_info.property_implemented_as
                 or cg_context.member_like.identifier)
    if cg_context.attribute_set:
        func_name = name_style.api_func("set", func_name)
    if cg_context.constructor:
        func_name = "Create"
    if "Reflect" in ext_attrs:  # [Reflect]
        func_name = _make_reflect_accessor_func_name(cg_context)

    if (cg_context.constructor or cg_context.member_like.is_static
            or is_partial):
        class_like = cg_context.member_like.owner_mixin or cg_context.class_like
        class_name = (code_generator_info.receiver_implemented_as
                      or name_style.class_(class_like.identifier))
        func_designator = "{}::{}".format(class_name, func_name)
    else:
        func_designator = _format("${blink_receiver}->{}", func_name)

    return _format("{_1}({_2})", _1=func_designator, _2=", ".join(arguments))


def bind_return_value(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode
    F = lambda *args, **kwargs: T(_format(*args, **kwargs))

    def create_definition(symbol_node):
        api_calls = []  # Pairs of (num_of_args, api_call_text)
        arguments = (cg_context.function_like.arguments
                     if cg_context.function_like else [])
        for index, arg in enumerate(arguments):
            if arg.is_optional and not arg.default_value:
                api_calls.append((index,
                                  _make_blink_api_call(code_node, cg_context,
                                                       index)))
        api_calls.append((None, _make_blink_api_call(code_node, cg_context)))

        nodes = []
        is_return_type_void = (cg_context.attribute_set
                               or cg_context.return_type.unwrap().is_void)
        if not is_return_type_void:
            return_type = blink_type_info(cg_context.return_type).value_t
        if len(api_calls) == 1:
            _, api_call = api_calls[0]
            if is_return_type_void:
                nodes.append(F("{};", api_call))
            elif cg_context.is_return_by_argument:
                nodes.append(F("{} ${return_value};", return_type))
                nodes.append(F("{};", api_call))
            elif cg_context.is_return_value_mutable:
                nodes.append(
                    F("{} ${return_value} = {};", return_type, api_call))
            else:
                nodes.append(F("const auto& ${return_value} = {};", api_call))
        else:
            branches = SequenceNode()
            for index, api_call in api_calls:
                if is_return_type_void or cg_context.is_return_by_argument:
                    assignment = "{};".format(api_call)
                else:
                    assignment = _format("${return_value} = {};", api_call)
                if index is not None:
                    branches.append(
                        CxxLikelyIfNode(
                            cond=_format("${info}[{}]->IsUndefined()", index),
                            body=[
                                T(assignment),
                                T("break;"),
                            ]))
                else:
                    branches.append(T(assignment))

            if not is_return_type_void:
                nodes.append(F("{} ${return_value};", return_type))
            nodes.append(CxxBreakableBlockNode(branches))

        if cg_context.may_throw_exception:
            nodes.append(
                CxxUnlikelyIfNode(
                    cond="${exception_state}.HadException()",
                    body=T("return;")))

        if "ReflectOnly" in cg_context.member_like.extended_attributes:
            # [ReflectOnly]
            node = _make_reflect_process_keyword_state(cg_context)
            if node:
                nodes.append(EmptyNode())
                nodes.append(node)

        return SymbolDefinitionNode(symbol_node, nodes)

    code_node.register_code_symbol(
        SymbolNode("return_value", definition_constructor=create_definition))


def make_check_argument_length(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode
    F = lambda *args, **kwargs: T(_format(*args, **kwargs))

    if cg_context.attribute_get:
        num_of_required_args = 0
    elif cg_context.attribute_set:
        idl_type = cg_context.attribute.idl_type
        if not (idl_type.does_include_nullable_or_dict
                or idl_type.unwrap().is_any or
                "TreatNonObjectAsNull" in idl_type.unwrap().extended_attributes
                or "PutForwards" in cg_context.attribute.extended_attributes
                or "Replaceable" in cg_context.attribute.extended_attributes):
            # ES undefined in ${info}[0] will cause a TypeError anyway, so omit
            # the check against the number of arguments.
            return None
        num_of_required_args = 1
    elif cg_context.function_like:
        num_of_required_args = (
            cg_context.function_like.num_of_required_arguments)
    else:
        assert False

    if num_of_required_args == 0:
        return None

    return CxxUnlikelyIfNode(
        cond=_format("UNLIKELY(${info}.Length() < {})", num_of_required_args),
        body=[
            F(("${exception_state}.ThrowTypeError("
               "ExceptionMessages::NotEnoughArguments"
               "({}, ${info}.Length()));"), num_of_required_args),
            T("return;"),
        ])


def make_check_constructor_call(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    return SequenceNode([
        CxxUnlikelyIfNode(
            cond="!${info}.IsConstructCall()",
            body=T("${exception_state}.ThrowTypeError("
                   "ExceptionMessages::ConstructorNotCallableAsFunction("
                   "${class_like_name}));\n"
                   "return;")),
        CxxLikelyIfNode(
            cond=("ConstructorMode::Current(${isolate}) == "
                  "ConstructorMode::kWrapExistingObject"),
            body=T("bindings::V8SetReturnValue(${info}, ${v8_receiver});\n"
                   "return;")),
    ])


def make_check_receiver(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    if (cg_context.attribute
            and "LenientThis" in cg_context.attribute.extended_attributes):
        return SequenceNode([
            T("// [LenientThis]"),
            CxxUnlikelyIfNode(
                cond="!${class_name}::HasInstance(${isolate}, ${v8_receiver})",
                body=T("return;")),
        ])

    if cg_context.return_type and cg_context.return_type.unwrap().is_promise:
        return SequenceNode([
            T("// Promise returning function: "
              "Convert a TypeError to a reject promise."),
            CxxUnlikelyIfNode(
                cond="!${class_name}::HasInstance(${isolate}, ${v8_receiver})",
                body=[
                    T("${exception_state}.ThrowTypeError("
                      "\"Illegal invocation\");"),
                    T("return;"),
                ])
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
    body = [
        T(use_counter),
        T("bindings::V8SetReturnValue(${info}, nullptr);\n"
          "return;"),
    ]
    return SequenceNode([
        T("// [CheckSecurity=ReturnValue]"),
        CxxUnlikelyIfNode(cond=cond, body=body),
    ])


def make_cooperative_scheduling_safepoint(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    node = TextNode("scheduler::CooperativeSchedulingManager::Instance()"
                    "->Safepoint();")
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/platform/scheduler/public/"
            "cooperative_scheduling_manager.h"
        ]))
    return node


def make_log_activity(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    target = cg_context.member_like or cg_context.property_
    ext_attrs = target.extended_attributes
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
    elif cg_context.attribute_set:
        _1 = "LogSetter"
        _4 = ", ${info}[0]"
    elif cg_context.operation_group:
        _1 = "LogMethod"
        _4 = ", ${info}"
    body = _format(pattern, _1=_1, _2=_2, _3=_3, _4=_4)

    pattern = ("// [LogActivity], [LogAllWorlds]\n" "if ({_1}) {{ {_2} }}")
    node = TextNode(_format(pattern, _1=cond, _2=body))
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/"
            "platform/bindings/v8_dom_activity_logger.h",
            "third_party/blink/renderer/"
            "platform/bindings/v8_per_context_data.h",
        ]))
    return node


def _make_overload_dispatcher_per_arg_size(cg_context, items):
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
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(items, (list, tuple))
    assert all(
        isinstance(item, web_idl.OverloadGroup.EffectiveOverloadItem)
        for item in items)

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
        func_name = callback_function_name(cg_context,
                                           func_like.overload_index)
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
            node = CxxUnlikelyIfNode(cond=conditional, body=node)
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
    func_like = find(lambda t, u: t.does_include_nullable_or_dict)
    if func_like:
        dispatch_if("{value}->IsNullOrUndefined()")

    # 12.4. if V is a platform object, ...
    def inheritance_length(func_and_type):
        return len(func_and_type[1].type_definition_object.
                   inclusive_inherited_interfaces)

    # Attempt to match from most derived to least derived.
    for func_like, idl_type in sorted(
            find_all_interfaces(), key=inheritance_length, reverse=True):
        v8_bridge_name = v8_bridge_class_name(
            idl_type.unwrap().type_definition_object)
        dispatch_if(
            _format("{}::HasInstance(${isolate}, {value})", v8_bridge_name))

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

        node, can_fail = _make_overload_dispatcher_per_arg_size(
            cg_context, items)

        if arg_size > 0:
            node = CxxLikelyIfNode(
                cond=_format("${info}.Length() >= {}", arg_size),
                body=[node, T("break;") if can_fail else None])
            did_use_break = did_use_break or can_fail

        conditional = expr_or(
            map(lambda item: expr_from_exposure(item.function_like.exposure),
                items))
        if not conditional.is_always_true:
            node = CxxUnlikelyIfNode(cond=conditional, body=node)

        branches.append(node)

    if did_use_break:
        branches = CxxBreakableBlockNode(branches)

    if not did_use_break and arg_size == 0 and conditional.is_always_true:
        return branches

    return SequenceNode([
        branches,
        EmptyNode(),
        T("${exception_state}.ThrowTypeError"
          "(\"Overload resolution failed.\");\n"
          "return;"),
    ])


def make_report_deprecate_as(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    target = cg_context.member_like or cg_context.property_
    name = target.extended_attributes.value_of("DeprecateAs")
    if not name:
        return None

    pattern = ("// [DeprecateAs]\n"
               "Deprecation::CountDeprecation("
               "${execution_context}, WebFeature::k{_1});")
    _1 = name
    node = TextNode(_format(pattern, _1=_1))
    node.accumulate(
        CodeGenAccumulator.require_include_headers(
            ["third_party/blink/renderer/core/frame/deprecation.h"]))
    return node


def make_report_measure_as(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    target = cg_context.member_like or cg_context.property_
    ext_attrs = target.extended_attributes
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
    elif cg_context.exposed_construct:
        suffix = "_ConstructorGetter"
    elif cg_context.operation:
        suffix = "_Method"
    name = target.extended_attributes.value_of("MeasureAs")
    if name:
        name = "k{}".format(name)
    elif cg_context.constructor:
        name = "kV8{}{}".format(cg_context.class_like.identifier, suffix)
    else:
        name = "kV8{}_{}{}".format(
            cg_context.class_like.identifier,
            name_style.raw.upper_camel_case(target.identifier), suffix)

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
        CodeGenAccumulator.require_include_headers(
            ["third_party/blink/renderer/core/frame/dactyloscoper.h"]))

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
if (!${blink_receiver}->""" + pred + """()) {
  v8::Local<v8::Value> v8_value;
  if (v8_private_cached_attribute.GetOrUndefined(${v8_receiver})
          .ToLocal(&v8_value) && !v8_value->IsUndefined()) {
    bindings::V8SetReturnValue(${info}, v8_value);
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
    bindings::V8SetReturnValue(${info}, v8_value);
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

    target = cg_context.member_like or cg_context.property_

    suffix = ""
    if cg_context.attribute_get:
        suffix = "_Getter"
    elif cg_context.attribute_set:
        suffix = "_Setter"
    elif cg_context.exposed_construct:
        suffix = "_ConstructorGetterCallback"

    counter = target.extended_attributes.value_of("RuntimeCallStatsCounter")
    if counter:
        macro_name = "RUNTIME_CALL_TIMER_SCOPE"
        counter_name = "RuntimeCallStats::CounterId::k{}{}".format(
            counter, suffix)
    else:
        macro_name = "RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT"
        counter_name = "\"Blink_{}_{}{}\"".format(
            blink_class_name(cg_context.class_like), target.identifier, suffix)

    return TextNode(
        _format(
            "{macro_name}(${info}.GetIsolate(), {counter_name});",
            macro_name=macro_name,
            counter_name=counter_name))


def make_steps_of_ce_reactions(cg_context):
    assert isinstance(cg_context, CodeGenContext)
    assert cg_context.attribute_set or cg_context.operation

    T = TextNode

    nodes = []

    ext_attrs = cg_context.member_like.extended_attributes
    if "CustomElementCallbacks" in ext_attrs or "Reflect" in ext_attrs:
        if "CustomElementCallbacks" in ext_attrs:
            nodes.append(T("// [CustomElementCallbacks]"))
        elif "Reflect" in ext_attrs:
            nodes.append(T("// [Reflect]"))
        nodes.append(
            T("V0CustomElementProcessingStack::CallbackDeliveryScope "
              "v0_custom_element_scope;"))

    if "CEReactions" in ext_attrs:
        nodes.append(T("// [CEReactions]"))
        nodes.append(T("CEReactionsScope ce_reactions_scope;"))

    if not nodes:
        return None

    nodes = SequenceNode(nodes)
    nodes.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h",
            "third_party/blink/renderer/core/html/custom/"
            "v0_custom_element_processing_stack.h"
        ]))
    return nodes


def make_steps_of_put_forwards(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    return SequenceNode([
        T("// [PutForwards]"),
        T("v8::Local<v8::Value> target;"),
        T("if (!${v8_receiver}->Get(${current_context}, "
          "V8AtomicString(${isolate}, ${property_name}))"
          ".ToLocal(&target)) {\n"
          "  return;\n"
          "}"),
        CxxUnlikelyIfNode(
            cond="!target->IsObject()",
            body=[
                T("${exception_state}.ThrowTypeError("
                  "\"The attribute value is not an object\");"),
                T("return;"),
            ]),
        T("bool did_set;"),
        T("if (!target.As<v8::Object>()->Set(${current_context}, "
          "V8AtomicString(${isolate}, "
          "\"${attribute.extended_attributes.value_of(\"PutForwards\")}\""
          "), ${info}[0]).To(&did_set)) {{\n"
          "  return;\n"
          "}}"),
    ])


def make_steps_of_replaceable(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    return SequenceNode([
        T("// [Replaceable]"),
        T("bool did_create;"),
        T("if (!${v8_receiver}->CreateDataProperty(${current_context}, "
          "V8AtomicString(${isolate}, ${property_name}), "
          "${info}[0]).To(&did_create)) {\n"
          "  return;\n"
          "}"),
    ])


def make_v8_set_return_value(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    if cg_context.attribute_set or cg_context.return_type.unwrap().is_void:
        # Request a SymbolNode |return_value| to define itself without rendering
        # any text.
        return T("<% return_value.request_symbol_definition() %>")

    return_type = cg_context.return_type
    if return_type.is_typedef:
        if return_type.identifier in ("EventHandler",
                                      "OnBeforeUnloadEventHandler",
                                      "OnErrorEventHandler"):
            return T("bindings::V8SetReturnValue(${info}, ${return_value}, "
                     "${isolate}, ${blink_receiver});")

    return_type = return_type.unwrap(typedef=True)
    return_type_body = return_type.unwrap()

    V8_RETURN_VALUE_FAST_TYPES = ("boolean", "byte", "octet", "short",
                                  "unsigned short", "long", "unsigned long")
    if return_type.keyword_typename in V8_RETURN_VALUE_FAST_TYPES:
        return T("bindings::V8SetReturnValue(${info}, ${return_value});")

    if return_type_body.is_string:
        args = ["${info}", "${return_value}", "${isolate}"]
        if return_type.is_nullable:
            args.append("bindings::V8ReturnValue::kNullable")
        else:
            args.append("bindings::V8ReturnValue::kNonNullable")
        return T("bindings::V8SetReturnValue({});".format(", ".join(args)))

    if return_type_body.is_interface:
        args = ["${info}", "${return_value}"]
        if cg_context.for_world == cg_context.MAIN_WORLD:
            args.append("bindings::V8ReturnValue::kMainWorld")
        elif cg_context.constructor or cg_context.member_like.is_static:
            args.append("${creation_context}")
        else:
            args.append("${blink_receiver}")
        return T("bindings::V8SetReturnValue({});".format(", ".join(args)))

    if return_type.is_frozen_array:
        return T("bindings::V8SetReturnValue(${info}, FreezeV8Object(ToV8("
                 "${return_value}, ${creation_context_object}, ${isolate}), "
                 "${isolate}));")

    if return_type.is_promise:
        return T("bindings::V8SetReturnValue"
                 "(${info}, ${return_value}.V8Value());")

    return T("bindings::V8SetReturnValue(${info}, "
             "ToV8(${return_value}, ${creation_context_object}, ${isolate}));")


def _make_empty_callback_def(cg_context, function_name, arg_decls=None):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    if arg_decls is None:
        arg_decls = ["const v8::FunctionCallbackInfo<v8::Value>& info"]

    func_def = CxxFuncDefNode(
        name=function_name, arg_decls=arg_decls, return_type="void")
    func_def.set_base_template_vars(cg_context.template_bindings())

    body = func_def.body
    body.add_template_var("info", "info")

    bind_callback_local_vars(body, cg_context)
    if cg_context.attribute or cg_context.function_like:
        bind_blink_api_arguments(body, cg_context)
        bind_return_value(body, cg_context)

    return func_def


def make_attribute_get_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    func_def = _make_empty_callback_def(cg_context, function_name)
    body = func_def.body

    body.extend([
        make_runtime_call_timer_scope(cg_context),
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        EmptyNode(),
    ])

    if "Getter" in cg_context.property_.extended_attributes.values_of(
            "Custom"):
        text = _format("${class_name}::{}(${info});",
                       custom_function_name(cg_context))
        body.append(TextNode(text))
        return func_def

    body.extend([
        make_check_receiver(cg_context),
        make_return_value_cache_return_early(cg_context),
        EmptyNode(),
        make_check_security_of_return_value(cg_context),
        make_v8_set_return_value(cg_context),
        make_return_value_cache_update_value(cg_context),
    ])

    return func_def


def make_attribute_set_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    ext_attrs = cg_context.attribute.extended_attributes
    if cg_context.attribute.is_readonly and not any(
            ext_attr in ext_attrs
            for ext_attr in ("LenientSetter", "PutForwards", "Replaceable")):
        return None

    func_def = _make_empty_callback_def(cg_context, function_name)
    body = func_def.body

    if "LenientSetter" in ext_attrs:
        body.append(TextNode("// [LenientSetter]"))
        return func_def

    body.extend([
        make_runtime_call_timer_scope(cg_context),
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        EmptyNode(),
    ])

    if "Setter" in cg_context.property_.extended_attributes.values_of(
            "Custom"):
        text = _format("${class_name}::{}(${info}[0], ${info});",
                       custom_function_name(cg_context))
        body.append(TextNode(text))
        return func_def

    body.extend([
        make_check_receiver(cg_context),
        make_check_argument_length(cg_context),
        EmptyNode(),
    ])

    if "PutForwards" in ext_attrs:
        body.append(make_steps_of_put_forwards(cg_context))
        return func_def

    if "Replaceable" in ext_attrs:
        body.append(make_steps_of_replaceable(cg_context))
        return func_def

    body.extend([
        make_steps_of_ce_reactions(cg_context),
        EmptyNode(),
        make_v8_set_return_value(cg_context),
    ])

    return func_def


def make_constant_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    logging_nodes = SequenceNode([
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
    ])
    if not logging_nodes:
        return None

    func_def = _make_empty_callback_def(
        cg_context,
        function_name,
        arg_decls=[
            "v8::Local<v8::Name> property",
            "const v8::FunctionCallbackInfo<v8::Value>& info",
        ])
    body = func_def.body

    v8_set_return_value = _format(
        "bindings::V8SetReturnValue(${info}, ${class_name}::{});",
        constant_name(cg_context))
    body.extend([
        make_runtime_call_timer_scope(cg_context),
        logging_nodes,
        EmptyNode(),
        TextNode(v8_set_return_value),
    ])

    return func_def


def make_constant_constant_def(cg_context, constant_name):
    # IDL constant's C++ constant definition
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(constant_name, str)

    constant_type = blink_type_info(cg_context.constant.idl_type).value_t
    return TextNode("static constexpr {type} {name} = {value};".format(
        type=constant_type,
        name=constant_name,
        value=cg_context.constant.value.literal))


def make_overload_dispatcher_function_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    func_def = _make_empty_callback_def(cg_context, function_name)
    body = func_def.body

    if cg_context.operation_group:
        body.append(make_cooperative_scheduling_safepoint(cg_context))
        body.append(EmptyNode())

    if cg_context.constructor_group:
        body.append(make_check_constructor_call(cg_context))
        body.append(EmptyNode())

    body.append(make_overload_dispatcher(cg_context))

    return func_def


def make_constructor_function_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    T = TextNode

    func_def = _make_empty_callback_def(cg_context, function_name)
    body = func_def.body

    body.extend([
        make_runtime_call_timer_scope(cg_context),
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        EmptyNode(),
        make_check_argument_length(cg_context),
        EmptyNode(),
    ])

    if "HTMLConstructor" in cg_context.constructor.extended_attributes:
        body.append(T("// [HTMLConstructor]"))
        text = _format(
            "V8HTMLConstructor::HtmlConstructor("
            "${info}, *${class_name}::GetWrapperTypeInfo(), "
            "HTMLElementType::{});",
            name_style.constant(cg_context.class_like.identifier))
        body.append(T(text))
    else:
        body.append(
            T("v8::Local<v8::Object> v8_wrapper = "
              "${return_value}->AssociateWithWrapper(${isolate}, "
              "${class_name}::GetWrapperTypeInfo(), ${v8_receiver});"))
        body.append(T("bindings::V8SetReturnValue(${info}, v8_wrapper);"))

    return func_def


def make_constructor_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    constructor_group = cg_context.constructor_group

    if len(constructor_group) == 1:
        return make_constructor_function_def(
            cg_context.make_copy(constructor=constructor_group[0]),
            function_name)

    node = SequenceNode()
    for constructor in constructor_group:
        cgc = cg_context.make_copy(constructor=constructor)
        node.extend([
            make_constructor_function_def(
                cgc, callback_function_name(cgc, constructor.overload_index)),
            EmptyNode(),
        ])
    node.append(
        make_overload_dispatcher_function_def(cg_context, function_name))
    return node


def make_exposed_construct_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    func_def = _make_empty_callback_def(
        cg_context,
        function_name,
        arg_decls=[
            "v8::Local<v8::Name> property",
            "const v8::PropertyCallbackInfo<v8::Value>& info",
        ])
    body = func_def.body

    v8_set_return_value = _format(
        "bindings::V8SetReturnValue"
        "(${info}, {}::GetWrapperTypeInfo(), InterfaceObject);",
        v8_bridge_class_name(cg_context.exposed_construct))
    body.extend([
        make_runtime_call_timer_scope(cg_context),
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        EmptyNode(),
        TextNode(v8_set_return_value),
    ])

    return func_def


def make_operation_function_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    func_def = _make_empty_callback_def(cg_context, function_name)
    body = func_def.body

    body.extend([
        make_runtime_call_timer_scope(cg_context),
        make_report_deprecate_as(cg_context),
        make_report_measure_as(cg_context),
        make_log_activity(cg_context),
        EmptyNode(),
    ])

    if "Custom" in cg_context.property_.extended_attributes:
        text = _format("${class_name}::{}(${info});",
                       custom_function_name(cg_context))
        body.append(TextNode(text))
        return func_def

    body.extend([
        make_check_receiver(cg_context),
        make_check_argument_length(cg_context),
        EmptyNode(),
        make_steps_of_ce_reactions(cg_context),
        EmptyNode(),
        make_check_security_of_return_value(cg_context),
        make_v8_set_return_value(cg_context),
    ])

    return func_def


def make_operation_callback_def(cg_context, function_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)

    operation_group = cg_context.operation_group

    assert (not ("Custom" in operation_group.extended_attributes)
            or len(operation_group) == 1)

    if len(operation_group) == 1:
        return make_operation_function_def(
            cg_context.make_copy(operation=operation_group[0]), function_name)

    node = SequenceNode()
    for operation in operation_group:
        cgc = cg_context.make_copy(operation=operation)
        node.extend([
            make_operation_function_def(
                cgc, callback_function_name(cgc, operation.overload_index)),
            EmptyNode(),
        ])
    node.append(
        make_overload_dispatcher_function_def(cg_context, function_name))
    return node


# ----------------------------------------------------------------------------
# Installer functions
# ----------------------------------------------------------------------------

# FN = function name
FN_INSTALL_INTERFACE_TEMPLATE = name_style.func("InstallInterfaceTemplate")
FN_INSTALL_UNCONDITIONAL_PROPS = name_style.func(
    "InstallUnconditionalProperties")
FN_INSTALL_CONTEXT_INDEPENDENT_PROPS = name_style.func(
    "InstallContextIndependentProperties")
FN_INSTALL_CONTEXT_DEPENDENT_PROPS = name_style.func(
    "InstallContextDependentProperties")

# TP = trampoline name
TP_INSTALL_INTERFACE_TEMPLATE = name_style.member_var(
    "install_interface_template_func")
TP_INSTALL_UNCONDITIONAL_PROPS = name_style.member_var(
    "install_unconditional_props_func")
TP_INSTALL_CONTEXT_INDEPENDENT_PROPS = name_style.member_var(
    "install_context_independent_props_func")
TP_INSTALL_CONTEXT_DEPENDENT_PROPS = name_style.member_var(
    "install_context_dependent_props_func")


def bind_installer_local_vars(code_node, cg_context):
    assert isinstance(code_node, SymbolScopeNode)
    assert isinstance(cg_context, CodeGenContext)

    S = SymbolNode

    local_vars = []

    local_vars.extend([
        S("execution_context", ("ExecutionContext* ${execution_context} = "
                                "ExecutionContext::From(${script_state});")),
        S("instance_template",
          ("v8::Local<v8::ObjectTemplate> ${instance_template} = "
           "${interface_template}->InstanceTemplate();")),
        S("interface_template",
          ("v8::Local<v8::FunctionTemplate> ${interface_template} = "
           "${wrapper_type_info}->DomTemplate(${isolate}, ${world});")),
        S("is_in_secure_context",
          ("const bool ${is_in_secure_context} = "
           "${execution_context}->IsSecureContext();")),
        S("isolate", "v8::Isolate* ${isolate} = ${v8_context}->GetIsolate();"),
        S("prototype_template",
          ("v8::Local<v8::ObjectTemplate> ${prototype_template} = "
           "${interface_template}->PrototypeTemplate();")),
        S("script_state",
          "ScriptState* ${script_state} = ScriptState::From(${v8_context});"),
        S("signature",
          ("v8::Local<v8::Signature> ${signature} = "
           "v8::Signature::New(${isolate}, ${interface_template});")),
        S("wrapper_type_info",
          ("const WrapperTypeInfo* const ${wrapper_type_info} = "
           "${class_name}::GetWrapperTypeInfo();")),
    ])

    pattern = (
        "v8::Local<v8::FunctionTemplate> ${parent_interface_template}{_1};")
    _1 = (" = ${wrapper_type_info}->parent_class->dom_template_function"
          "(${isolate}, ${world})")
    if not cg_context.class_like.inherited:
        _1 = ""
    local_vars.append(S("parent_interface_template", _format(pattern, _1=_1)))

    # Arguments have priority over local vars.
    template_vars = code_node.template_vars
    for symbol_node in local_vars:
        if symbol_node.name not in template_vars:
            code_node.register_code_symbol(symbol_node)


def _make_property_entry_v8_property_attribute(member):
    values = []
    if "NotEnumerable" in member.extended_attributes:
        values.append("v8::DontEnum")
    if "Unforgeable" in member.extended_attributes:
        values.append("v8::DontDelete")
    if not values:
        values.append("v8::None")
    return "static_cast<v8::PropertyAttribute>({})".format(" | ".join(values))


def _make_property_entry_on_which_object(member):
    ON_INSTANCE = "V8DOMConfiguration::kOnInstance"
    ON_PROTOTYPE = "V8DOMConfiguration::kOnPrototype"
    ON_INTERFACE = "V8DOMConfiguration::kOnInterface"
    if isinstance(member, web_idl.Constant):
        return ON_INTERFACE
    if hasattr(member, "is_static") and member.is_static:
        return ON_INTERFACE
    if "Global" in member.owner.extended_attributes:
        return ON_INSTANCE
    if "Unforgeable" in member.extended_attributes:
        return ON_INSTANCE
    return ON_PROTOTYPE


def _make_property_entry_check_receiver(member):
    if ("LenientThis" in member.extended_attributes
            or (isinstance(member, web_idl.Attribute)
                and member.idl_type.unwrap().is_promise)
            or (isinstance(member, web_idl.FunctionLike)
                and member.return_type.unwrap().is_promise)):
        return "V8DOMConfiguration::kDoNotCheckHolder"
    else:
        return "V8DOMConfiguration::kCheckHolder"


def _make_property_entry_has_side_effect(member):
    if member.extended_attributes.value_of("Affects") == "Nothing":
        return "V8DOMConfiguration::kHasNoSideEffect"
    else:
        return "V8DOMConfiguration::kHasSideEffect"


def _make_property_entry_world(world):
    if world == CodeGenContext.MAIN_WORLD:
        return "V8DOMConfiguration::kMainWorld"
    if world == CodeGenContext.NON_MAIN_WORLDS:
        return "V8DOMConfiguration::kNonMainWorlds"
    if world == CodeGenContext.ALL_WORLDS:
        return "V8DOMConfiguration::kAllWorlds"
    assert False


def _make_property_entry_constant_type_and_value_format(member):
    idl_type = member.idl_type.unwrap()
    if (idl_type.keyword_typename == "long long"
            or idl_type.keyword_typename == "unsigned long long"):
        assert False, "64-bit constants are not yet supported."
    if idl_type.keyword_typename == "unsigned long":
        return ("V8DOMConfiguration::kConstantTypeUnsignedLong",
                "static_cast<int>({value})")
    if idl_type.is_integer:
        return ("V8DOMConfiguration::kConstantTypeLong",
                "static_cast<int>({value})")
    if idl_type.is_floating_point_numeric:
        return ("V8DOMConfiguration::kConstantTypeDouble",
                "static_cast<double>({value})")
    assert False, "Unsupported type: {}".format(idl_type.syntactic_form)


def _make_attribute_registration_table(table_name, attribute_entries):
    assert isinstance(table_name, str)
    assert isinstance(attribute_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryAttribute) for entry in attribute_entries)

    T = TextNode

    entry_nodes = []
    for entry in attribute_entries:
        pattern = ("{{"
                   "\"{property_name}\", "
                   "{attribute_get_callback}, "
                   "{attribute_set_callback}, "
                   "V8PrivateProperty::kNoCachedAccessor, "
                   "{v8_property_attribute}, "
                   "{on_which_object}, "
                   "{check_receiver}, "
                   "{has_side_effect}, "
                   "V8DOMConfiguration::kAlwaysCallGetter, "
                   "{world}"
                   "}},")
        text = _format(
            pattern,
            property_name=entry.member.identifier,
            attribute_get_callback=entry.attr_get_callback_name,
            attribute_set_callback=(entry.attr_set_callback_name or "nullptr"),
            v8_property_attribute=_make_property_entry_v8_property_attribute(
                entry.member),
            on_which_object=_make_property_entry_on_which_object(entry.member),
            check_receiver=_make_property_entry_check_receiver(entry.member),
            has_side_effect=_make_property_entry_has_side_effect(entry.member),
            world=_make_property_entry_world(entry.world))
        entry_nodes.append(T(text))

    return ListNode([
        T("static constexpr V8DOMConfiguration::AccessorConfiguration " +
          table_name + "[] = {"),
        ListNode(entry_nodes),
        T("};"),
    ])


def _make_constant_callback_registration_table(table_name, constant_entries):
    assert isinstance(table_name, str)
    assert isinstance(constant_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryConstant)
        and isinstance(entry.const_callback_name, str)
        for entry in constant_entries)

    T = TextNode

    entry_nodes = []
    for entry in constant_entries:
        pattern = ("{{" "\"{property_name}\", " "{constant_callback}" "}},")
        text = _format(
            pattern,
            property_name=entry.member.identifier,
            constant_callback=entry.const_callback_name)
        entry_nodes.append(T(text))

    return ListNode([
        T("static constexpr V8DOMConfiguration::ConstantCallbackConfiguration "
          + table_name + "[] = {"),
        ListNode(entry_nodes),
        T("};"),
    ])


def _make_constant_value_registration_table(table_name, constant_entries):
    assert isinstance(table_name, str)
    assert isinstance(constant_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryConstant)
        and entry.const_callback_name is None for entry in constant_entries)

    T = TextNode

    entry_nodes = []
    for entry in constant_entries:
        pattern = ("{{"
                   "\"{property_name}\", "
                   "{constant_type}, "
                   "{constant_value}"
                   "}},")
        constant_type, constant_value_fmt = (
            _make_property_entry_constant_type_and_value_format(entry.member))
        constant_value = _format(
            constant_value_fmt, value=entry.const_constant_name)
        text = _format(
            pattern,
            property_name=entry.member.identifier,
            constant_type=constant_type,
            constant_value=constant_value)
        entry_nodes.append(T(text))

    return ListNode([
        T("static constexpr V8DOMConfiguration::ConstantConfiguration " +
          table_name + "[] = {"),
        ListNode(entry_nodes),
        T("};"),
    ])


def _make_exposed_construct_registration_table(table_name,
                                               exposed_construct_entries):
    assert isinstance(table_name, str)
    assert isinstance(exposed_construct_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryExposedConstruct)
        for entry in exposed_construct_entries)

    T = TextNode

    entry_nodes = []
    for entry in exposed_construct_entries:
        pattern = ("{{"
                   "\"{property_name}\", "
                   "{exposed_construct_callback}, "
                   "nullptr, "
                   "static_cast<v8::PropertyAttribute>(v8::DontEnum), "
                   "V8DOMConfiguration::kOnInstance, "
                   "V8DOMConfiguration::kDoNotCheckHolder, "
                   "V8DOMConfiguration::kHasNoSideEffect, "
                   "V8DOMConfiguration::kAlwaysCallGetter, "
                   "{world}"
                   "}}, ")
        text = _format(
            pattern,
            property_name=entry.member.identifier,
            exposed_construct_callback=entry.prop_callback_name,
            world=_make_property_entry_world(entry.world))
        entry_nodes.append(T(text))

    return ListNode([
        T("static constexpr V8DOMConfiguration::AttributeConfiguration " +
          table_name + "[] = {"),
        ListNode(entry_nodes),
        T("};"),
    ])


def _make_operation_registration_table(table_name, operation_entries):
    assert isinstance(table_name, str)
    assert isinstance(operation_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryOperationGroup)
        for entry in operation_entries)

    T = TextNode

    entry_nodes = []
    for entry in operation_entries:
        pattern = ("{{"
                   "\"{property_name}\", "
                   "{operation_callback}, "
                   "{function_length}, "
                   "{v8_property_attribute}, "
                   "{on_which_object}, "
                   "{check_receiver}, "
                   "V8DOMConfiguration::kDoNotCheckAccess, "
                   "{has_side_effect}, "
                   "{world}"
                   "}}, ")
        text = _format(
            pattern,
            property_name=entry.member.identifier,
            operation_callback=entry.op_callback_name,
            function_length=entry.op_func_length,
            v8_property_attribute=_make_property_entry_v8_property_attribute(
                entry.member),
            on_which_object=_make_property_entry_on_which_object(entry.member),
            check_receiver=_make_property_entry_check_receiver(entry.member),
            has_side_effect=_make_property_entry_has_side_effect(entry.member),
            world=_make_property_entry_world(entry.world))
        entry_nodes.append(T(text))

    return ListNode([
        T("static constexpr V8DOMConfiguration::MethodConfiguration " +
          table_name + "[] = {"),
        ListNode(entry_nodes),
        T("};"),
    ])


class _PropEntryMember(object):
    def __init__(self, is_context_dependent, exposure_conditional, world,
                 member):
        assert isinstance(is_context_dependent, bool)
        assert isinstance(exposure_conditional, CodeGenExpr)

        self.is_context_dependent = is_context_dependent
        self.exposure_conditional = exposure_conditional
        self.world = world
        self.member = member


class _PropEntryAttribute(_PropEntryMember):
    def __init__(self, is_context_dependent, exposure_conditional, world,
                 attribute, attr_get_callback_name, attr_set_callback_name):
        assert isinstance(attribute, web_idl.Attribute)
        assert isinstance(attr_get_callback_name, str)
        assert _is_none_or_str(attr_set_callback_name)

        _PropEntryMember.__init__(self, is_context_dependent,
                                  exposure_conditional, world, attribute)
        self.attr_get_callback_name = attr_get_callback_name
        self.attr_set_callback_name = attr_set_callback_name


class _PropEntryConstant(_PropEntryMember):
    def __init__(self, is_context_dependent, exposure_conditional, world,
                 constant, const_callback_name, const_constant_name):
        assert isinstance(constant, web_idl.Constant)
        assert _is_none_or_str(const_callback_name)
        assert isinstance(const_constant_name, str)

        _PropEntryMember.__init__(self, is_context_dependent,
                                  exposure_conditional, world, constant)
        self.const_callback_name = const_callback_name
        self.const_constant_name = const_constant_name


class _PropEntryConstructorGroup(_PropEntryMember):
    def __init__(self, is_context_dependent, exposure_conditional, world,
                 constructor_group, ctor_callback_name, ctor_func_length):
        assert isinstance(constructor_group, web_idl.ConstructorGroup)
        assert isinstance(ctor_callback_name, str)
        assert isinstance(ctor_func_length, (int, long))

        _PropEntryMember.__init__(self, is_context_dependent,
                                  exposure_conditional, world,
                                  constructor_group)
        self.ctor_callback_name = ctor_callback_name
        self.ctor_func_length = ctor_func_length


class _PropEntryExposedConstruct(_PropEntryMember):
    def __init__(self, is_context_dependent, exposure_conditional, world,
                 exposed_construct, prop_callback_name):
        assert isinstance(prop_callback_name, str)

        _PropEntryMember.__init__(self, is_context_dependent,
                                  exposure_conditional, world,
                                  exposed_construct)
        self.prop_callback_name = prop_callback_name


class _PropEntryOperationGroup(_PropEntryMember):
    def __init__(self, is_context_dependent, exposure_conditional, world,
                 operation_group, op_callback_name, op_func_length):
        assert isinstance(operation_group, web_idl.OperationGroup)
        assert isinstance(op_callback_name, str)
        assert isinstance(op_func_length, (int, long))

        _PropEntryMember.__init__(self, is_context_dependent,
                                  exposure_conditional, world, operation_group)
        self.op_callback_name = op_callback_name
        self.op_func_length = op_func_length


def _make_property_entries_and_callback_defs(
        cg_context, attribute_entries, constant_entries, constructor_entries,
        exposed_construct_entries, operation_entries):
    """
    Creates intermediate objects to help property installation and also makes
    code nodes of callback functions.

    Args:
        attribute_entries:
        constructor_entries:
        exposed_construct_entries:
        operation_entries:
            Output parameters to store the intermediate objects.
    """
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(attribute_entries, list)
    assert isinstance(constant_entries, list)
    assert isinstance(constructor_entries, list)
    assert isinstance(exposed_construct_entries, list)
    assert isinstance(operation_entries, list)

    interface = cg_context.interface
    global_names = interface.extended_attributes.values_of("Global")

    callback_def_nodes = ListNode()

    def iterate(members, callback):
        for member in members:
            is_context_dependent = member.exposure.is_context_dependent(
                global_names)
            exposure_conditional = expr_from_exposure(member.exposure,
                                                      global_names)

            if "PerWorldBindings" in member.extended_attributes:
                worlds = (CodeGenContext.MAIN_WORLD,
                          CodeGenContext.NON_MAIN_WORLDS)
            else:
                worlds = (CodeGenContext.ALL_WORLDS, )

            for world in worlds:
                callback(member, is_context_dependent, exposure_conditional,
                         world)

    def process_attribute(attribute, is_context_dependent,
                          exposure_conditional, world):
        cgc_attr = cg_context.make_copy(attribute=attribute, for_world=world)
        cgc = cgc_attr.make_copy(attribute_get=True)
        attr_get_callback_name = callback_function_name(cgc)
        attr_get_callback_node = make_attribute_get_callback_def(
            cgc, attr_get_callback_name)
        cgc = cgc_attr.make_copy(attribute_set=True)
        attr_set_callback_name = callback_function_name(cgc)
        attr_set_callback_node = make_attribute_set_callback_def(
            cgc, attr_set_callback_name)
        if attr_set_callback_node is None:
            attr_set_callback_name = None

        callback_def_nodes.extend([
            attr_get_callback_node,
            EmptyNode(),
            attr_set_callback_node,
            EmptyNode(),
        ])

        attribute_entries.append(
            _PropEntryAttribute(
                is_context_dependent=is_context_dependent,
                exposure_conditional=exposure_conditional,
                world=world,
                attribute=attribute,
                attr_get_callback_name=attr_get_callback_name,
                attr_set_callback_name=attr_set_callback_name))

    def process_constant(constant, is_context_dependent, exposure_conditional,
                         world):
        cgc = cg_context.make_copy(constant=constant, for_world=world)
        const_callback_name = callback_function_name(cgc)
        const_callback_node = make_constant_callback_def(
            cgc, const_callback_name)
        if const_callback_node is None:
            const_callback_name = None
        # IDL constant's C++ constant name
        const_constant_name = _format("${class_name}::{}", constant_name(cgc))

        callback_def_nodes.extend([
            const_callback_node,
            EmptyNode(),
        ])

        constant_entries.append(
            _PropEntryConstant(
                is_context_dependent=is_context_dependent,
                exposure_conditional=exposure_conditional,
                world=world,
                constant=constant,
                const_callback_name=const_callback_name,
                const_constant_name=const_constant_name))

    def process_constructor_group(constructor_group, is_context_dependent,
                                  exposure_conditional, world):
        cgc = cg_context.make_copy(
            constructor_group=constructor_group, for_world=world)
        ctor_callback_name = callback_function_name(cgc)
        ctor_callback_node = make_constructor_callback_def(
            cgc, ctor_callback_name)

        callback_def_nodes.extend([
            ctor_callback_node,
            EmptyNode(),
        ])

        constructor_entries.append(
            _PropEntryConstructorGroup(
                is_context_dependent=is_context_dependent,
                exposure_conditional=exposure_conditional,
                world=world,
                constructor_group=constructor_group,
                ctor_callback_name=ctor_callback_name,
                ctor_func_length=(
                    constructor_group.min_num_of_required_arguments)))

    def process_exposed_construct(exposed_construct, is_context_dependent,
                                  exposure_conditional, world):
        cgc = cg_context.make_copy(
            exposed_construct=exposed_construct, for_world=world)
        prop_callback_name = callback_function_name(cgc)
        prop_callback_node = make_exposed_construct_callback_def(
            cgc, prop_callback_name)

        callback_def_nodes.extend([
            prop_callback_node,
            EmptyNode(),
        ])

        exposed_construct_entries.append(
            _PropEntryExposedConstruct(
                is_context_dependent=is_context_dependent,
                exposure_conditional=exposure_conditional,
                world=world,
                exposed_construct=exposed_construct,
                prop_callback_name=prop_callback_name))

    def process_operation_group(operation_group, is_context_dependent,
                                exposure_conditional, world):
        cgc = cg_context.make_copy(
            operation_group=operation_group, for_world=world)
        op_callback_name = callback_function_name(cgc)
        op_callback_node = make_operation_callback_def(cgc, op_callback_name)

        callback_def_nodes.extend([
            op_callback_node,
            EmptyNode(),
        ])

        operation_entries.append(
            _PropEntryOperationGroup(
                is_context_dependent=is_context_dependent,
                exposure_conditional=exposure_conditional,
                world=world,
                operation_group=operation_group,
                op_callback_name=op_callback_name,
                op_func_length=operation_group.min_num_of_required_arguments))

    iterate(interface.attributes, process_attribute)
    iterate(interface.constants, process_constant)
    iterate(interface.constructor_groups, process_constructor_group)
    iterate(interface.exposed_constructs, process_exposed_construct)
    iterate(interface.operation_groups, process_operation_group)

    return callback_def_nodes


def make_install_interface_template(cg_context, function_name, class_name,
                                    trampoline_var_name, constructor_entries,
                                    install_unconditional_func_name,
                                    install_context_independent_func_name):
    """
    Returns:
        A triplet of CodeNode of:
        - function declaration
        - function definition
        - trampoline function definition (from the API class to the
          implementation class), which is supposed to be defined inline
    """
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)
    assert _is_none_or_str(class_name)
    assert _is_none_or_str(trampoline_var_name)
    assert isinstance(constructor_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryConstructorGroup)
        for entry in constructor_entries)
    assert _is_none_or_str(install_unconditional_func_name)
    assert _is_none_or_str(install_context_independent_func_name)

    T = TextNode

    arg_decls = [
        "v8::Isolate* isolate",
        "const DOMWrapperWorld& world",
        "v8::Local<v8::FunctionTemplate> interface_template",
    ]
    return_type = "void"

    if trampoline_var_name is None:
        trampoline_def = None
    else:
        trampoline_def = CxxFuncDefNode(
            name=function_name,
            arg_decls=arg_decls,
            return_type=return_type,
            static=True)
        trampoline_def.body.append(
            TextNode(
                _format("return {}(isolate, world, interface_template);",
                        trampoline_var_name)))

    func_decl = CxxFuncDeclNode(
        name=function_name,
        arg_decls=arg_decls,
        return_type=return_type,
        static=True)

    func_def = CxxFuncDefNode(
        name=function_name,
        class_name=class_name,
        arg_decls=arg_decls,
        return_type=return_type)
    func_def.set_base_template_vars(cg_context.template_bindings())

    body = func_def.body
    body.add_template_vars({
        "isolate": "isolate",
        "world": "world",
        "interface_template": "interface_template",
    })
    bind_installer_local_vars(body, cg_context)

    body.extend([
        T("V8DOMConfiguration::InitializeDOMInterfaceTemplate("
          "${isolate}, ${interface_template}, "
          "${wrapper_type_info}->interface_name, ${parent_interface_template}, "
          "kV8DefaultWrapperInternalFieldCount);"),
        EmptyNode(),
    ])

    for entry in constructor_entries:
        set_callback = _format("${interface_template}->SetCallHandler({});",
                               entry.ctor_callback_name)
        set_length = _format("${interface_template}->SetLength({});",
                             entry.ctor_func_length)
        if entry.world == CodeGenContext.MAIN_WORLD:
            body.append(
                CxxLikelyIfNode(
                    cond="${world}.IsMainWorld()",
                    body=[T(set_callback), T(set_length)]))
        elif entry.world == CodeGenContext.NON_MAIN_WORLDS:
            body.append(
                CxxLikelyIfNode(
                    cond="!${world}.IsMainWorld()",
                    body=[T(set_callback), T(set_length)]))
        elif entry.world == CodeGenContext.ALL_WORLDS:
            body.extend([T(set_callback), T(set_length)])
        else:
            assert False
    body.append(EmptyNode())

    if ("Global" in cg_context.class_like.extended_attributes
            or cg_context.class_like.identifier == "Location"):
        if "Global" in cg_context.class_like.extended_attributes:
            body.append(T("// [Global]"))
        if cg_context.class_like.identifier == "Location":
            body.append(T("// Location exotic object"))
        body.extend([
            T("${instance_template}->SetImmutableProto();"),
            T("${prototype_template}->SetImmutableProto();"),
            EmptyNode(),
        ])

    func_call_pattern = ("{}(${isolate}, ${world}, ${instance_template}, "
                         "${prototype_template}, ${interface_template});")
    if install_unconditional_func_name:
        func_call = _format(func_call_pattern, install_unconditional_func_name)
        body.append(T(func_call))
    if install_context_independent_func_name:
        func_call = _format(func_call_pattern,
                            install_context_independent_func_name)
        body.append(T(func_call))

    return func_decl, func_def, trampoline_def


def make_install_properties(cg_context, function_name, class_name,
                            trampoline_var_name, is_context_dependent,
                            attribute_entries, constant_entries,
                            exposed_construct_entries, operation_entries):
    """
    Returns:
        A triplet of CodeNode of:
        - function declaration
        - function definition
        - trampoline function definition (from the API class to the
          implementation class), which is supposed to be defined inline
    """
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)
    assert _is_none_or_str(class_name)
    assert _is_none_or_str(trampoline_var_name)
    assert isinstance(is_context_dependent, bool)
    assert isinstance(attribute_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryAttribute) for entry in attribute_entries)
    assert isinstance(constant_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryConstant) for entry in constant_entries)
    assert isinstance(exposed_construct_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryExposedConstruct)
        for entry in exposed_construct_entries)
    assert isinstance(operation_entries, (list, tuple))
    assert all(
        isinstance(entry, _PropEntryOperationGroup)
        for entry in operation_entries)

    if not (attribute_entries or constant_entries or exposed_construct_entries
            or operation_entries):
        return None, None, None

    if is_context_dependent:
        arg_decls = [
            "v8::Local<v8::Context> context",
            "const DOMWrapperWorld& world",
            "v8::Local<v8::Object> instance_object",
            "v8::Local<v8::Object> prototype_object",
            "v8::Local<v8::Function> interface_object",
        ]
    else:
        arg_decls = [
            "v8::Isolate* isolate",
            "const DOMWrapperWorld& world",
            "v8::Local<v8::ObjectTemplate> instance_template",
            "v8::Local<v8::ObjectTemplate> prototype_template",
            "v8::Local<v8::FunctionTemplate> interface_template",
        ]
    return_type = "void"

    if trampoline_var_name is None:
        trampoline_def = None
    else:
        trampoline_def = CxxFuncDefNode(
            name=function_name,
            arg_decls=arg_decls,
            return_type=return_type,
            static=True)
        if is_context_dependent:
            args = [
                "context",
                "world",
                "instance_object",
                "prototype_object",
                "interface_object",
            ]
        else:
            args = [
                "isolate",
                "world",
                "instance_template",
                "prototype_template",
                "interface_template",
            ]
        text = _format(
            "return {func}({args});",
            func=trampoline_var_name,
            args=", ".join(args))
        trampoline_def.body.append(TextNode(text))

    func_decl = CxxFuncDeclNode(
        name=function_name,
        arg_decls=arg_decls,
        return_type=return_type,
        static=True)

    func_def = CxxFuncDefNode(
        name=function_name,
        class_name=class_name,
        arg_decls=arg_decls,
        return_type=return_type)
    func_def.set_base_template_vars(cg_context.template_bindings())

    body = func_def.body
    if is_context_dependent:
        body.add_template_vars({
            "v8_context": "context",  # 'context' is reserved by Mako.
            "world": "world",
            "instance_object": "instance_object",
            "prototype_object": "prototype_object",
            "interface_object": "interface_object",
        })
    else:
        body.add_template_vars({
            "isolate": "isolate",
            "world": "world",
            "instance_template": "instance_template",
            "prototype_template": "prototype_template",
            "interface_template": "interface_template",
        })
    bind_installer_local_vars(body, cg_context)

    def group_by_condition(entries):
        unconditional_entries = []
        conditional_to_entries = {}
        for entry in entries:
            assert entry.is_context_dependent == is_context_dependent
            if entry.exposure_conditional.is_always_true:
                unconditional_entries.append(entry)
            else:
                conditional_to_entries.setdefault(entry.exposure_conditional,
                                                  []).append(entry)
        return unconditional_entries, conditional_to_entries

    def install_properties(table_name, target_entries, make_table_func,
                           installer_call_text):
        unconditional_entries, conditional_to_entries = group_by_condition(
            target_entries)
        if unconditional_entries:
            body.append(
                CxxBlockNode([
                    make_table_func(table_name, unconditional_entries),
                    TextNode(installer_call_text),
                ]))
            body.append(EmptyNode())
        for conditional, entries in conditional_to_entries.iteritems():
            body.append(
                CxxUnlikelyIfNode(
                    cond=conditional,
                    body=[
                        make_table_func(table_name, entries),
                        TextNode(installer_call_text),
                    ]))
        body.append(EmptyNode())

    table_name = "kAttributeTable"
    if is_context_dependent:
        installer_call_text = (
            "V8DOMConfiguration::InstallAccessors(${isolate}, ${world}, "
            "${instance_object}, ${prototype_object}, ${interface_object}, "
            "${signature}, kAttributeTable, base::size(kAttributeTable));")
    else:
        installer_call_text = (
            "V8DOMConfiguration::InstallAccessors(${isolate}, ${world}, "
            "${instance_template}, ${prototype_template}, "
            "${interface_template}, ${signature}, "
            "kAttributeTable, base::size(kAttributeTable));")
    install_properties(table_name, attribute_entries,
                       _make_attribute_registration_table, installer_call_text)

    table_name = "kConstantCallbackTable"
    if is_context_dependent:
        installer_call_text = (
            "V8DOMConfiguration::InstallConstants(${isolate}, "
            "${interface_object}, ${prototype_object}, "
            "kConstantCallbackTable, base::size(kConstantCallbackTable));")
    else:
        installer_call_text = (
            "V8DOMConfiguration::InstallConstants(${isolate}, "
            "${interface_template}, ${prototype_template}, "
            "kConstantCallbackTable, base::size(kConstantCallbackTable));")
    constant_callback_entries = filter(lambda entry: entry.const_callback_name,
                                       constant_entries)
    install_properties(table_name, constant_callback_entries,
                       _make_constant_callback_registration_table,
                       installer_call_text)

    table_name = "kConstantValueTable"
    if is_context_dependent:
        installer_call_text = (
            "V8DOMConfiguration::InstallConstants(${isolate}, "
            "${interface_object}, ${prototype_object}, "
            "kConstantValueTable, base::size(kConstantValueTable));")
    else:
        installer_call_text = (
            "V8DOMConfiguration::InstallConstants(${isolate}, "
            "${interface_template}, ${prototype_template}, "
            "kConstantValueTable, base::size(kConstantValueTable));")
    constant_value_entries = filter(
        lambda entry: not entry.const_callback_name, constant_entries)
    install_properties(table_name, constant_value_entries,
                       _make_constant_value_registration_table,
                       installer_call_text)

    table_name = "kExposedConstructTable"
    if is_context_dependent:
        installer_call_text = (
            "V8DOMConfiguration::InstallAttributes(${isolate}, ${world}, "
            "${instance_object}, ${prototype_object}, "
            "kExposedConstructTable, base::size(kExposedConstructTable));")
    else:
        installer_call_text = (
            "V8DOMConfiguration::InstallAttributes(${isolate}, ${world}, "
            "${instance_template}, ${prototype_template}, "
            "kExposedConstructTable, base::size(kExposedConstructTable));")
    install_properties(table_name, exposed_construct_entries,
                       _make_exposed_construct_registration_table,
                       installer_call_text)

    table_name = "kOperationTable"
    if is_context_dependent:
        installer_call_text = (
            "V8DOMConfiguration::InstallMethods(${isolate}, ${world}, "
            "${instance_object}, ${prototype_object}, ${interface_object}, "
            "${signature}, kOperationTable, base::size(kOperationTable));")
    else:
        installer_call_text = (
            "V8DOMConfiguration::InstallMethods(${isolate}, ${world}, "
            "${instance_template}, ${prototype_template}, "
            "${interface_template}, ${signature}, "
            "kOperationTable, base::size(kOperationTable));")
    install_properties(table_name, operation_entries,
                       _make_operation_registration_table, installer_call_text)

    return func_decl, func_def, trampoline_def


def make_cross_component_init(cg_context, function_name, class_name):
    """
    Returns:
        A triplet of CodeNode of:
        - function declaration
        - function definition
        - trampoline member variable definitions
    """
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)
    assert isinstance(class_name, str)

    T = TextNode
    F = lambda *args, **kwargs: T(_format(*args, **kwargs))

    trampoline_var_decls = ListNode([
        F("static InstallInterfaceTemplateFuncType {};",
          TP_INSTALL_INTERFACE_TEMPLATE),
        F("static InstallUnconditionalPropertiesFuncType {};",
          TP_INSTALL_UNCONDITIONAL_PROPS),
        F("static InstallContextIndependentPropertiesFuncType {};",
          TP_INSTALL_CONTEXT_INDEPENDENT_PROPS),
        F("static InstallContextDependentPropertiesFuncType {};",
          TP_INSTALL_CONTEXT_DEPENDENT_PROPS),
    ])

    trampoline_var_defs = ListNode([
        F(("${class_name}::InstallInterfaceTemplateFuncType "
           "${class_name}::{} = nullptr;"), TP_INSTALL_INTERFACE_TEMPLATE),
        F(("${class_name}::InstallUnconditionalPropertiesFuncType "
           "${class_name}::{} = nullptr;"), TP_INSTALL_UNCONDITIONAL_PROPS),
        F(("${class_name}::InstallContextIndependentPropertiesFuncType "
           "${class_name}::{} = nullptr;"),
          TP_INSTALL_CONTEXT_INDEPENDENT_PROPS),
        F(("${class_name}::InstallContextDependentPropertiesFuncType "
           "${class_name}::{} = nullptr;"), TP_INSTALL_CONTEXT_DEPENDENT_PROPS),
    ])
    trampoline_var_defs.set_base_template_vars(cg_context.template_bindings())

    func_decl = CxxFuncDeclNode(
        name=function_name, arg_decls=[], return_type="void", static=True)

    func_def = CxxFuncDefNode(
        name=function_name,
        class_name=class_name,
        arg_decls=[],
        return_type="void")
    func_def.set_base_template_vars(cg_context.template_bindings())

    body = func_def.body
    body.extend([
        F("${class_name}::{} = {};", TP_INSTALL_INTERFACE_TEMPLATE,
          FN_INSTALL_INTERFACE_TEMPLATE),
        F("${class_name}::{} = {};", TP_INSTALL_UNCONDITIONAL_PROPS,
          FN_INSTALL_UNCONDITIONAL_PROPS),
        F("${class_name}::{} = {};", TP_INSTALL_CONTEXT_INDEPENDENT_PROPS,
          FN_INSTALL_CONTEXT_INDEPENDENT_PROPS),
        F("${class_name}::{} = {};", TP_INSTALL_CONTEXT_DEPENDENT_PROPS,
          FN_INSTALL_CONTEXT_DEPENDENT_PROPS),
    ])

    return func_decl, func_def, trampoline_var_decls, trampoline_var_defs


# ----------------------------------------------------------------------------
# WrapperTypeInfo
# ----------------------------------------------------------------------------

# FN = function name
FN_GET_WRAPPER_TYPE_INFO = name_style.func("GetWrapperTypeInfo")

# MN = member name
MN_WRAPPER_TYPE_INFO = name_style.member_var("wrapper_type_info_")


def make_wrapper_type_info(cg_context, function_name, member_var_name,
                           install_context_dependent_func_name):
    assert isinstance(cg_context, CodeGenContext)
    assert isinstance(function_name, str)
    assert isinstance(member_var_name, str)
    assert _is_none_or_str(install_context_dependent_func_name)

    func_def = CxxFuncDefNode(
        name=function_name,
        arg_decls=[],
        return_type="constexpr WrapperTypeInfo*",
        static=True)
    func_def.set_base_template_vars(cg_context.template_bindings())
    func_def.body.append(TextNode("return &{};".format(member_var_name)))

    member_var_def = TextNode(
        "static WrapperTypeInfo {};".format(member_var_name))

    pattern = """\
// Migration adapter
v8::Local<v8::FunctionTemplate> ${class_name}::DomTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world) {{
  return V8DOMConfiguration::DomClassTemplate(
      isolate, world,
      const_cast<WrapperTypeInfo*>(${class_name}::GetWrapperTypeInfo()),
      ${class_name}::InstallInterfaceTemplate);
}}

WrapperTypeInfo ${class_name}::wrapper_type_info_{{
    gin::kEmbedderBlink,
    ${class_name}::DomTemplate,
    {install_context_dependent_func},
    "${{class_like.identifier}}",
    {wrapper_type_info_of_inherited},
    {wrapper_type_prototype},
    {wrapper_class_id},
    {active_script_wrappable_inheritance},
}};"""
    class_like = cg_context.class_like
    install_context_dependent_func = (
        "${class_name}::InstallContextDependentAdapter"
        if install_context_dependent_func_name else "nullptr")
    if class_like.inherited:
        wrapper_type_info_of_inherited = "{}::GetWrapperTypeInfo()".format(
            v8_bridge_class_name(class_like.inherited))
    else:
        wrapper_type_info_of_inherited = "nullptr"
    wrapper_type_prototype = ("WrapperTypeInfo::kWrapperTypeObjectPrototype"
                              if isinstance(class_like, web_idl.Interface) else
                              "WrapperTypeInfo::kWrapperTypeNoPrototype")
    wrapper_class_id = ("WrapperTypeInfo::kNodeClassId"
                        if class_like.does_implement("Node") else
                        "WrapperTypeInfo::kObjectClassId")
    active_script_wrappable_inheritance = (
        "WrapperTypeInfo::kInheritFromActiveScriptWrappable"
        if "ActiveScriptWrappable" in class_like.extended_attributes else
        "WrapperTypeInfo::kNotInheritFromActiveScriptWrappable")
    text = _format(
        pattern,
        install_context_dependent_func=install_context_dependent_func,
        wrapper_type_info_of_inherited=wrapper_type_info_of_inherited,
        wrapper_type_prototype=wrapper_type_prototype,
        wrapper_class_id=wrapper_class_id,
        active_script_wrappable_inheritance=active_script_wrappable_inheritance
    )
    bridge_wrapper_type_info_def = TextNode(text)

    blink_class = blink_class_name(class_like)
    pattern = ("const WrapperTypeInfo& {blink_class}::wrapper_type_info_ = "
               "${class_name}::wrapper_type_info_;")
    blink_wrapper_type_info_def = TextNode(
        _format(pattern, blink_class=blink_class))

    if "ActiveScriptWrappable" in class_like.extended_attributes:
        pattern = """\
// [ActiveScriptWrappable]
static_assert(
    std::is_base_of<ActiveScriptWrappableBase, {blink_class}>::value,
    "{blink_class} does not inherit from ActiveScriptWrappable<> despite "
    "the IDL has [ActiveScriptWrappable] extended attribute.");
static_assert(
    !std::is_same<decltype(&{blink_class}::HasPendingActivity),
                  decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "{blink_class} is not overriding hasPendingActivity() despite "
    "the IDL has [ActiveScriptWrappable] extended attribute.");"""
    else:
        pattern = """\
// non-[ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, {blink_class}>::value,
    "{blink_class} inherits from ActiveScriptWrappable<> without "
    "[ActiveScriptWrappable] extended attribute.");
static_assert(
    std::is_same<decltype(&{blink_class}::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "{blink_class} is overriding hasPendingActivity() without "
    "[ActiveScriptWrappable] extended attribute.");"""
    check_active_script_wrappable = TextNode(
        _format(pattern, blink_class=blink_class))

    wrapper_type_info_def = ListNode([
        bridge_wrapper_type_info_def,
        EmptyNode(),
        blink_wrapper_type_info_def,
        EmptyNode(),
        check_active_script_wrappable,
    ])
    wrapper_type_info_def.set_base_template_vars(
        cg_context.template_bindings())

    return func_def, member_var_def, wrapper_type_info_def


# ----------------------------------------------------------------------------
# Main functions
# ----------------------------------------------------------------------------


def _collect_include_headers(interface):
    headers = set(interface.code_generator_info.blink_headers)

    def collect_from_idl_type(idl_type):
        idl_type = idl_type.unwrap()
        type_def_obj = idl_type.type_definition_object
        if type_def_obj is not None:
            headers.add(PathManager(type_def_obj).api_path(ext="h"))
            return
        union_def_obj = idl_type.union_definition_object
        if union_def_obj is not None:
            headers.add(PathManager(union_def_obj).api_path(ext="h"))
            return

    for attribute in interface.attributes:
        collect_from_idl_type(attribute.idl_type)
    for constructor in interface.constructors:
        for argument in constructor.arguments:
            collect_from_idl_type(argument.idl_type)
    for operation in interface.operations:
        collect_from_idl_type(operation.return_type)
        for argument in operation.arguments:
            collect_from_idl_type(argument.idl_type)

    if interface.inherited:
        headers.add(PathManager(interface.inherited).api_path(ext="h"))

    path_manager = PathManager(interface)
    headers.discard(path_manager.api_path(ext="h"))
    headers.discard(path_manager.impl_path(ext="h"))

    return headers


def generate_interface(interface):
    path_manager = PathManager(interface)
    api_component = path_manager.api_component
    impl_component = path_manager.impl_component
    is_cross_components = path_manager.is_cross_components

    # Class names
    api_class_name = v8_bridge_class_name(interface)
    if is_cross_components:
        impl_class_name = "{}::Impl".format(api_class_name)
    else:
        impl_class_name = api_class_name

    cg_context = CodeGenContext(interface=interface, class_name=api_class_name)

    # Filepaths
    api_header_path = path_manager.api_path(ext="h")
    api_source_path = path_manager.api_path(ext="cc")
    if is_cross_components:
        impl_header_path = path_manager.impl_path(ext="h")
        impl_source_path = path_manager.impl_path(ext="cc")

    # Root nodes
    api_header_node = ListNode(tail="\n")
    api_header_node.set_accumulator(CodeGenAccumulator())
    api_header_node.set_renderer(MakoRenderer())
    api_source_node = ListNode(tail="\n")
    api_source_node.set_accumulator(CodeGenAccumulator())
    api_source_node.set_renderer(MakoRenderer())
    if is_cross_components:
        impl_header_node = ListNode(tail="\n")
        impl_header_node.set_accumulator(CodeGenAccumulator())
        impl_header_node.set_renderer(MakoRenderer())
        impl_source_node = ListNode(tail="\n")
        impl_source_node.set_accumulator(CodeGenAccumulator())
        impl_source_node.set_renderer(MakoRenderer())
    else:
        impl_header_node = api_header_node
        impl_source_node = api_source_node

    # Namespaces
    api_header_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))
    api_source_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))
    if is_cross_components:
        impl_header_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))
        impl_source_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))
    else:
        impl_header_blink_ns = api_header_blink_ns
        impl_source_blink_ns = api_source_blink_ns

    # Class definitions
    api_class_def = CxxClassDefNode(
        cg_context.class_name,
        base_class_names=[
            _format("bindings::V8InterfaceBridge<${class_name}, {}>",
                    blink_class_name(interface)),
        ],
        final=True,
        export=component_export(api_component))
    api_class_def.set_base_template_vars(cg_context.template_bindings())
    api_class_def.bottom_section.append(
        TextNode("friend class {};".format(blink_class_name(interface))))
    if is_cross_components:
        impl_class_def = CxxClassDefNode(
            impl_class_name,
            final=True,
            export=component_export(impl_component))
        impl_class_def.set_base_template_vars(cg_context.template_bindings())
        api_class_def.public_section.extend([
            TextNode("// Cross-component implementation class"),
            TextNode("class Impl;"),
            EmptyNode(),
        ])
    else:
        impl_class_def = api_class_def

    # Constants
    constant_defs = ListNode()
    for constant in interface.constants:
        cgc = cg_context.make_copy(constant=constant)
        constant_defs.append(
            make_constant_constant_def(cgc, constant_name(cgc)))

    # Custom callback implementations
    custom_callback_impl_decls = ListNode()
    for attribute in interface.attributes:
        custom_values = attribute.extended_attributes.values_of("Custom")
        if "Getter" in custom_values:
            func_name = custom_function_name(
                cg_context.make_copy(attribute=attribute, attribute_get=True))
            custom_callback_impl_decls.append(
                CxxFuncDeclNode(
                    name=func_name,
                    arg_decls=["const v8::FunctionCallbackInfo<v8::Value>&"],
                    return_type="void",
                    static=True))
        if "Setter" in custom_values:
            func_name = custom_function_name(
                cg_context.make_copy(attribute=attribute, attribute_set=True))
            custom_callback_impl_decls.append(
                CxxFuncDeclNode(
                    name=func_name,
                    arg_decls=[
                        "v8::Local<v8::Value>",
                        "const v8::FunctionCallbackInfo<v8::Value>&"
                    ],
                    return_type="void",
                    static=True))
    for operation_group in interface.operation_groups:
        if "Custom" in operation_group.extended_attributes:
            func_name = custom_function_name(
                cg_context.make_copy(operation_group=operation_group))
            custom_callback_impl_decls.append(
                CxxFuncDeclNode(
                    name=func_name,
                    arg_decls=["const v8::FunctionCallbackInfo<v8::Value>&"],
                    return_type="void",
                    static=True))

    # Cross-component trampolines
    if is_cross_components:
        # tp_ = trampoline name
        tp_install_interface_template = TP_INSTALL_INTERFACE_TEMPLATE
        tp_install_unconditional_props = TP_INSTALL_UNCONDITIONAL_PROPS
        tp_install_context_independent_props = (
            TP_INSTALL_CONTEXT_INDEPENDENT_PROPS)
        tp_install_context_dependent_props = TP_INSTALL_CONTEXT_DEPENDENT_PROPS
        (cross_component_init_decl, cross_component_init_def,
         trampoline_var_decls, trampoline_var_defs) = make_cross_component_init(
             cg_context, "Init", class_name=impl_class_name)
    else:
        tp_install_interface_template = None
        tp_install_unconditional_props = None
        tp_install_context_independent_props = None
        tp_install_context_dependent_props = None

    # Callback functions
    attribute_entries = []
    constant_entries = []
    constructor_entries = []
    exposed_construct_entries = []
    operation_entries = []
    callback_defs = _make_property_entries_and_callback_defs(
        cg_context,
        attribute_entries=attribute_entries,
        constant_entries=constant_entries,
        constructor_entries=constructor_entries,
        exposed_construct_entries=exposed_construct_entries,
        operation_entries=operation_entries)

    # Installer functions
    is_unconditional = lambda entry: entry.exposure_conditional.is_always_true
    is_context_dependent = lambda entry: entry.is_context_dependent
    is_context_independent = (
        lambda e: not is_context_dependent(e) and not is_unconditional(e))
    (install_unconditional_props_decl, install_unconditional_props_def,
     install_unconditional_props_trampoline) = make_install_properties(
         cg_context,
         FN_INSTALL_UNCONDITIONAL_PROPS,
         class_name=impl_class_name,
         trampoline_var_name=tp_install_unconditional_props,
         is_context_dependent=False,
         attribute_entries=filter(is_unconditional, attribute_entries),
         constant_entries=filter(is_unconditional, constant_entries),
         exposed_construct_entries=filter(is_unconditional,
                                          exposed_construct_entries),
         operation_entries=filter(is_unconditional, operation_entries))
    (install_context_independent_props_decl,
     install_context_independent_props_def,
     install_context_independent_props_trampoline) = make_install_properties(
         cg_context,
         FN_INSTALL_CONTEXT_INDEPENDENT_PROPS,
         class_name=impl_class_name,
         trampoline_var_name=tp_install_context_independent_props,
         is_context_dependent=False,
         attribute_entries=filter(is_context_independent, attribute_entries),
         constant_entries=filter(is_context_independent, constant_entries),
         exposed_construct_entries=filter(is_context_independent,
                                          exposed_construct_entries),
         operation_entries=filter(is_context_independent, operation_entries))
    (install_context_dependent_props_decl, install_context_dependent_props_def,
     install_context_dependent_props_trampoline) = make_install_properties(
         cg_context,
         FN_INSTALL_CONTEXT_DEPENDENT_PROPS,
         class_name=impl_class_name,
         trampoline_var_name=tp_install_context_dependent_props,
         is_context_dependent=True,
         attribute_entries=filter(is_context_dependent, attribute_entries),
         constant_entries=filter(is_context_dependent, constant_entries),
         exposed_construct_entries=filter(is_context_dependent,
                                          exposed_construct_entries),
         operation_entries=filter(is_context_dependent, operation_entries))
    (install_interface_template_decl, install_interface_template_def,
     install_interface_template_trampoline) = make_install_interface_template(
         cg_context,
         FN_INSTALL_INTERFACE_TEMPLATE,
         class_name=impl_class_name,
         trampoline_var_name=tp_install_interface_template,
         constructor_entries=constructor_entries,
         install_unconditional_func_name=(install_unconditional_props_def
                                          and FN_INSTALL_UNCONDITIONAL_PROPS),
         install_context_independent_func_name=(
             install_context_independent_props_def
             and FN_INSTALL_CONTEXT_INDEPENDENT_PROPS))
    installer_function_decls = ListNode([
        install_interface_template_decl,
        install_unconditional_props_decl,
        install_context_independent_props_decl,
        install_context_dependent_props_decl,
    ])
    installer_function_defs = ListNode([
        install_interface_template_def,
        EmptyNode(),
        install_unconditional_props_def,
        EmptyNode(),
        install_context_independent_props_def,
        EmptyNode(),
        install_context_dependent_props_def,
    ])
    installer_function_trampolines = ListNode([
        install_interface_template_trampoline,
        install_unconditional_props_trampoline,
        install_context_independent_props_trampoline,
        install_context_dependent_props_trampoline,
    ])

    # WrapperTypeInfo
    (get_wrapper_type_info_def, wrapper_type_info_var_def,
     wrapper_type_info_init) = make_wrapper_type_info(
         cg_context,
         FN_GET_WRAPPER_TYPE_INFO,
         MN_WRAPPER_TYPE_INFO,
         install_context_dependent_func_name=(
             install_context_dependent_props_def
             and FN_INSTALL_CONTEXT_DEPENDENT_PROPS))

    # Header part (copyright, include directives, and forward declarations)
    api_header_node.extend([
        make_copyright_header(),
        EmptyNode(),
        enclose_with_header_guard(
            ListNode([
                make_header_include_directives(api_header_node.accumulator),
                EmptyNode(),
                api_header_blink_ns,
            ]), name_style.header_guard(api_header_path)),
    ])
    api_header_blink_ns.body.extend([
        make_forward_declarations(api_header_node.accumulator),
        EmptyNode(),
    ])
    api_source_node.extend([
        make_copyright_header(),
        EmptyNode(),
        TextNode("#include \"{}\"".format(api_header_path)),
        EmptyNode(),
        make_header_include_directives(api_source_node.accumulator),
        EmptyNode(),
        api_source_blink_ns,
    ])
    api_source_blink_ns.body.extend([
        make_forward_declarations(api_source_node.accumulator),
        EmptyNode(),
    ])
    if is_cross_components:
        impl_header_node.extend([
            make_copyright_header(),
            EmptyNode(),
            enclose_with_header_guard(
                ListNode([
                    make_header_include_directives(
                        impl_header_node.accumulator),
                    EmptyNode(),
                    impl_header_blink_ns,
                ]), name_style.header_guard(impl_header_path)),
        ])
        impl_header_blink_ns.body.extend([
            make_forward_declarations(impl_header_node.accumulator),
            EmptyNode(),
        ])
        impl_source_node.extend([
            make_copyright_header(),
            EmptyNode(),
            TextNode("#include \"{}\"".format(impl_header_path)),
            EmptyNode(),
            make_header_include_directives(impl_source_node.accumulator),
            EmptyNode(),
            impl_source_blink_ns,
        ])
        impl_source_blink_ns.body.extend([
            make_forward_declarations(impl_source_node.accumulator),
            EmptyNode(),
        ])
    api_header_node.accumulator.add_include_headers([
        component_export_header(api_component),
        "third_party/blink/renderer/platform/bindings/v8_interface_bridge.h",
    ])
    api_header_node.accumulator.add_class_decl(blink_class_name(interface))
    api_source_node.accumulator.add_include_headers([
        interface.code_generator_info.blink_headers[0],
        "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h",
    ])
    if is_cross_components:
        impl_header_node.accumulator.add_include_headers([
            api_header_path,
            component_export_header(impl_component),
        ])
    impl_source_node.accumulator.add_include_headers([
        "third_party/blink/renderer/bindings/core/v8/"
        "native_value_traits_impl.h",
        "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h",
        "third_party/blink/renderer/bindings/core/v8/"
        "v8_set_return_value_for_core.h",
        "third_party/blink/renderer/platform/bindings/exception_messages.h",
        "third_party/blink/renderer/platform/bindings/runtime_call_stats.h",
        "third_party/blink/renderer/platform/bindings/v8_binding.h",
    ])
    impl_source_node.accumulator.add_include_headers(
        _collect_include_headers(interface))

    # Assemble the parts.
    api_header_blink_ns.body.append(api_class_def)
    if is_cross_components:
        impl_header_blink_ns.body.append(impl_class_def)

    api_class_def.public_section.append(get_wrapper_type_info_def)
    api_class_def.public_section.append(EmptyNode())
    api_class_def.public_section.extend([
        TextNode("// Migration adapter"),
        TextNode("static v8::Local<v8::FunctionTemplate> DomTemplate("
                 "v8::Isolate* isolate, "
                 "const DOMWrapperWorld& world);"),
        EmptyNode(),
    ])
    api_class_def.private_section.append(wrapper_type_info_var_def)
    api_class_def.private_section.append(EmptyNode())
    api_source_blink_ns.body.extend([
        wrapper_type_info_init,
        EmptyNode(),
    ])

    if is_cross_components:
        api_class_def.public_section.append(installer_function_trampolines)
        api_class_def.private_section.extend([
            TextNode("// Cross-component trampolines"),
            trampoline_var_decls,
            EmptyNode(),
        ])
        api_source_blink_ns.body.extend([
            TextNode("// Cross-component trampolines"),
            trampoline_var_defs,
            EmptyNode(),
        ])
        impl_class_def.public_section.append(cross_component_init_decl)
        impl_class_def.private_section.append(installer_function_decls)
        impl_source_blink_ns.body.extend([
            cross_component_init_def,
            EmptyNode(),
        ])
    else:
        api_class_def.public_section.append(installer_function_decls)
        api_class_def.public_section.append(EmptyNode())

    if constant_defs:
        api_class_def.public_section.extend([
            TextNode("// Constants"),
            constant_defs,
            EmptyNode(),
        ])

    if custom_callback_impl_decls:
        api_class_def.public_section.extend([
            TextNode("// Custom callback implementations"),
            custom_callback_impl_decls,
            EmptyNode(),
        ])

    impl_source_blink_ns.body.extend([
        CxxNamespaceNode(name="", body=callback_defs),
        EmptyNode(),
        installer_function_defs,
    ])

    # Write down to the files.
    write_code_node_to_file(api_header_node,
                            path_manager.gen_path_to(api_header_path))
    write_code_node_to_file(api_source_node,
                            path_manager.gen_path_to(api_source_path))
    if path_manager.is_cross_components:
        write_code_node_to_file(impl_header_node,
                                path_manager.gen_path_to(impl_header_path))
        write_code_node_to_file(impl_source_node,
                                path_manager.gen_path_to(impl_source_path))


def generate_interfaces(web_idl_database):
    interface = web_idl_database.find("Node")
    generate_interface(interface)
