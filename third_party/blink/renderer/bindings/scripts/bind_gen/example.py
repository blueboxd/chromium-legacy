# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path

from .clang_format import clang_format
from .code_generation_context import CodeGenerationContext
from .code_node import CodeNode
from .code_node import FunctionDefinitionNode
from .code_node import LiteralNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .mako_renderer import MakoRenderer


def make_common_local_vars(cg_context):
    assert isinstance(cg_context, CodeGenerationContext)

    S = SymbolNode
    T = TextNode

    local_vars = []
    supplemental_bindings = {}

    local_vars.extend([
        S("class_like_name", ("const char* const ${class_like_name} = "
                              "\"${class_like.identifier}\";")),
        S("current_context", ("v8::Local<v8::Context> ${current_context} = "
                              "${isolate}->GetCurrentContext();")),
        S("current_script_state", ("ScriptState* ${current_script_state} = "
                                   "ScriptState::ForCurrentRealm(${info});")),
        S("exception_state",
          ("ExceptionState ${exception_state}("
           "${isolate}, ${exception_state_context_type}, ${class_like_name}, "
           "${property_name});${reject_promise_scope_definition}")),
        S("isolate", "v8::Isolate* ${isolate} = ${info}.GetIsolate();"),
        S("per_context_data", ("V8PerContextData* ${per_context_data} = "
                               "${script_state}->PerContextData();")),
        S("per_isolate_data", ("V8PerIsolateData* ${per_isolate_data} = "
                               "V8PerIsolateData::From(${isolate});")),
        S("property_name",
          "const char* const ${property_name} = \"${property.identifier}\";"),
        S("receiver", "v8::Local<v8::Object> ${receiver} = ${info}.This();"),
        S("receiver_context", ("v8::Local<v8::Context> ${receiver_context} = "
                               "${receiver_script_state}->GetContext();")),
        S("receiver_script_state",
          ("ScriptState* ${receiver_script_state} = "
           "ScriptState::ForRelevantRealm(${info});")),
    ])

    # script_state
    def_text = "ScriptState* ${script_state} = {_1};"
    if cg_context.member_like and not cg_context.member_like.is_static:
        _1 = "${receiver_script_state}"
    else:
        _1 = "${current_script_state}"
    local_vars.append(
        S("script_state", CodeNode.format_template(def_text, _1=_1)))

    # exception_state_context_type
    def_text = (
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
        S("exception_state_context_type",
          CodeNode.format_template(def_text, _1=_1)))

    # reject_promise_scope_definition
    if ((cg_context.attribute_get and cg_context.attribute.idl_type.is_promise)
            or (cg_context.operation
                and cg_context.operation.return_type.is_promise)):
        template_text = ("\n"
                         "ExceptionToRejectPromiseScope reject_promise_scope"
                         "(${info}, ${exception_state});")
    else:
        template_text = ""
    supplemental_bindings["reject_promise_scope_definition"] = T(template_text)

    return local_vars, supplemental_bindings


def make_attribute_get_def(cg_context):
    assert isinstance(cg_context, CodeGenerationContext)

    L = LiteralNode
    T = TextNode

    cg_context = cg_context.make_copy(attribute_get=True)

    local_vars, local_vars_bindings = make_common_local_vars(cg_context)

    func_def = FunctionDefinitionNode(
        name=L("AttributeGetCallback"),
        arg_decls=[L("const v8::FunctionCallbackInfo<v8::Value>& info")],
        return_type=L("void"),
        local_vars=local_vars)

    body = func_def.body
    body.add_template_var("info", "info")
    body.add_template_vars(local_vars_bindings)
    body.add_template_vars(cg_context.template_bindings())

    body.extend([
        T("(void)(${per_context_data}, ${exception_state});"),
    ])

    return func_def


def run_example(web_idl_database, output_dirs):
    renderer = MakoRenderer()

    filename = 'v8_example.cc'
    filepath = os.path.join(output_dirs['core'], filename)

    namespace = list(web_idl_database.namespaces)[0]
    attribute = namespace.attributes[0]

    cg_context = CodeGenerationContext(
        namespace=namespace, attribute=attribute)

    root_node = SymbolScopeNode(renderer=renderer)
    root_node.append(make_attribute_get_def(cg_context))

    prev = ''
    current = str(root_node)
    while current != prev:
        prev = current
        current = str(root_node)
    rendered_text = current

    format_result = clang_format(rendered_text, filename=filename)
    with open(filepath, 'w') as output_file:
        output_file.write(format_result.contents)
