# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path

import web_idl

from . import name_style
from .blink_v8_bridge import blink_class_name
from .blink_v8_bridge import blink_type_info
from .blink_v8_bridge import make_v8_to_blink_value
from .code_node import CodeNode
from .code_node import FunctionDefinitionNode
from .code_node import SequenceNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .codegen_context import CodeGenContext
from .codegen_expr import expr_from_exposure
from .codegen_format import format_template as _format
from .codegen_utils import enclose_with_namespace
from .codegen_utils import make_copyright_header
from .codegen_utils import write_code_node_to_file
from .mako_renderer import MakoRenderer


_DICT_MEMBER_PRESENCE_PREDICATES = {
    "ScriptValue": "{}.IsEmpty()",
    "ScriptPromise": "{}.IsEmpty()",
}


def _blink_member_name(member):
    assert isinstance(member, web_idl.DictionaryMember)

    class BlinkMemberName(object):
        def __init__(self, member):
            blink_name = (member.code_generator_info.property_implemented_as
                          or member.identifier)
            self.get_api = name_style.api_func(blink_name)
            self.set_api = name_style.api_func("set", blink_name)
            self.has_api = name_style.api_func("has", blink_name)
            # C++ data member that shows the presence of the IDL member.
            self.presence_var = name_style.member_var("has", blink_name)
            # C++ data member that holds the value of the IDL member.
            self.value_var = name_style.member_var(blink_name)

    return BlinkMemberName(member)


def _is_member_always_present(member):
    assert isinstance(member, web_idl.DictionaryMember)
    return member.is_required or member.default_value is not None


def _does_use_presence_flag(member):
    assert isinstance(member, web_idl.DictionaryMember)
    return (not _is_member_always_present(member) and blink_type_info(
        member.idl_type).member_t not in _DICT_MEMBER_PRESENCE_PREDICATES)


def _member_presence_expr(member):
    assert isinstance(member, web_idl.DictionaryMember)
    if _is_member_always_present(member):
        return "true"
    if _does_use_presence_flag(member):
        return _blink_member_name(member).presence_var
    blink_type = blink_type_info(member.idl_type).member_t
    assert blink_type in _DICT_MEMBER_PRESENCE_PREDICATES
    _1 = _blink_member_name(member).value_var
    return _format(_DICT_MEMBER_PRESENCE_PREDICATES[blink_type], _1)


def make_dict_member_get_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    member = cg_context.dict_member

    func_name = "{}::{}".format(
        blink_class_name(cg_context.dictionary),
        _blink_member_name(member).get_api)
    func_def = FunctionDefinitionNode(
        name=T(func_name),
        arg_decls=[],
        return_type=T(blink_type_info(member.idl_type).ref_t))

    body = func_def.body
    body.add_template_vars(cg_context.template_bindings())

    _1 = _blink_member_name(member).has_api
    body.append(T(_format("DCHECK({_1}());", _1=_1)))

    _1 = _blink_member_name(member).value_var
    body.append(T(_format("return {_1};", _1=_1)))

    return func_def


def make_dict_member_has_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    member = cg_context.dict_member

    func_name = "{}::{}".format(
        blink_class_name(cg_context.dictionary),
        _blink_member_name(member).has_api)
    func_def = FunctionDefinitionNode(
        name=T(func_name), arg_decls=[], return_type=T("bool"))

    body = func_def.body

    _1 = _member_presence_expr(member)
    body.append(T(_format("return {_1};", _1=_1)))

    return func_def


def make_dict_member_set_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    member = cg_context.dict_member

    func_name = "{}::{}".format(
        blink_class_name(cg_context.dictionary),
        _blink_member_name(member).set_api)
    func_def = FunctionDefinitionNode(
        name=T(func_name),
        arg_decls=[
            T(_format("{} value",
                      blink_type_info(member.idl_type).ref_t))
        ],
        return_type=T("void"))

    body = func_def.body
    body.add_template_vars(cg_context.template_bindings())

    _1 = _blink_member_name(member).value_var
    body.append(T(_format("{_1} = value;", _1=_1)))

    if _does_use_presence_flag(member):
        _1 = _blink_member_name(member).presence_var
        body.append(T(_format("{_1} = true;", _1=_1)))

    return func_def


def make_get_v8_dict_member_names_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary

    func_name = _format("{}::GetV8MemberNames",
                        blink_class_name(cg_context.dictionary))
    func_def = FunctionDefinitionNode(
        name=T(func_name),
        arg_decls=[T("v8::Isolate* isolate")],
        return_type=T("const v8::Eternal<v8::Name>*"),
        comment=T("// static"))

    body = func_def.body

    pattern = ("static const char* kKeyStrings[] = {{{_1}}};")
    _1 = ", ".join(
        _format("\"{}\"", member.identifier) for member in dictionary.members)
    body.extend([
        T(_format(pattern, _1=_1)),
        T("return V8PerIsolateData::From(isolate)"
          "->FindOrCreateEternalNameCache("
          "kKeyStrings, kKeyStrings, base::size(kKeyStrings));")
    ])

    return func_def


def make_fill_with_dict_members_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary

    func_name = _format(
        "{_1}::FillWithMembers", _1=blink_class_name(dictionary))
    func_def = FunctionDefinitionNode(
        name=T(func_name),
        arg_decls=[
            T("v8::Isolate* isolate"),
            T("v8::Local<v8::Object> creation_context"),
            T("v8::Local<v8::Object> v8_dictionary"),
        ],
        return_type=T("bool"))

    body = func_def.body
    body.add_template_vars(cg_context.template_bindings())

    if dictionary.inherited:
        text = """\
if (!BaseClass::FillWithMembers(isolate, creation_context, v8_dictionary)) {
  return false;
}"""
        body.append(T(text))

    body.append(
        T("return FillWithOwnMembers("
          "isolate, creation_context, v8_dictionary)"))

    return func_def


def make_fill_with_own_dict_members_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary
    own_members = dictionary.own_members

    func_name = _format(
        "{_1}::FillWithOwnMembers", _1=blink_class_name(dictionary))
    func_def = FunctionDefinitionNode(
        name=T(func_name),
        arg_decls=[
            T("v8::Isolate* isolate"),
            T("v8::Local<v8::Object> creation_context"),
            T("v8::Local<v8::Object> v8_dictionary")
        ],
        return_type=T("bool"))

    body = func_def.body
    body.add_template_vars(cg_context.template_bindings())

    text = """\
const v8::Eternal<v8::Name>* member_names = GetV8MemberNames(isolate);
v8::Local<v8::Context> current_context = isolate->GetCurrentContext();"""
    body.append(T(text))

    # TODO(peria): Support runtime enabled / origin trial members.
    for key_index, member in enumerate(own_members):
        _1 = _blink_member_name(member).has_api
        _2 = key_index
        _3 = _blink_member_name(member).get_api
        pattern = ("""\
if ({_1}()) {{
  if (!v8_dictionary
           ->CreateDataProperty(
               current_context,
               member_names[{_2}].Get(isolate),
               ToV8({_3}(), creation_context, isolate))
           .ToChecked()) {{
    return false;
  }}
}}\
""")
        node = T(_format(pattern, _1=_1, _2=_2, _3=_3))

        conditional = expr_from_exposure(member.exposure)
        if not conditional.is_always_true:
            node = SequenceNode([
                T(_format("if ({}) {{", conditional.to_text())),
                node,
                T("}"),
            ])

        body.append(node)

    body.append(T("return true;"))

    return func_def


def make_dict_create_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary
    class_name = blink_class_name(dictionary)

    func_def = FunctionDefinitionNode(
        name=T(_format("{_1}::Create", _1=class_name)),
        arg_decls=[
            T("v8::Isolate* isolate"),
            T("v8::Local<v8::Object> v8_dictionary"),
            T("ExceptionState& exception_state"),
        ],
        return_type=T(_format("{_1}*", _1=class_name)),
        comment=T("// static"))

    body = func_def.body
    body.add_template_vars(cg_context.template_bindings())

    pattern = """\
{_1}* dictionary = MakeGarbageCollected<{_1}>();
dictionary->FillMembers(isolate, v8_dictionary, exception_state);
if (exception_state.HadException()) {
  return nullptr;
}
return dictionary;"""
    body.append(T(_format(pattern, _1=class_name)))

    return func_def


def make_fill_dict_members_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary
    class_name = blink_class_name(dictionary)
    own_members = dictionary.own_members
    required_own_members = list(
        member for member in own_members if member.is_required)

    func_name = _format("{_1}::FillMembers", _1=class_name)
    func_def = FunctionDefinitionNode(
        name=T(func_name),
        arg_decls=[
            T("v8::Isolate* isolate"),
            T("v8::Local<v8::Object> v8_dictionary"),
            T("ExceptionState& exception_state"),
        ],
        return_type=T("void"))

    body = func_def.body
    body.add_template_vars(cg_context.template_bindings())

    text = "if (v8_dictionary->IsUndefinedOrNull()) { return; }"
    if len(required_own_members) > 0:
        text = """\
if (v8_dictionary->IsUndefinedOrNull()) {
  exception_state.ThrowError(ExceptionMessages::FailedToConstruct(
      "${dictionary.identifier}",
      "has required members, but null/undefined was passed."));
  return;
}"""
    body.append(T(text))

    # [PermissiveDictionaryConversion]
    if "PermissiveDictionaryConversion" in dictionary.extended_attributes:
        text = """\
if (!v8_dictionary->IsObject()) {
  // [PermissiveDictionaryConversion]
  return;
}"""
    else:
        text = """\
if (!v8_dictionary->IsObject()) {
  exception_state.ThrowTypeError(
      ExceptionMessages::FailedToConstruct(
          "${dictionary.identifier}", "The value is not of type Object"));
  return;
}"""
    body.append(T(text))

    body.append(
        T("FillMembersInternal(isolate, v8_dictionary, exception_state);"))

    return func_def


def make_fill_dict_members_internal_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary
    own_members = dictionary.own_members
    class_name = blink_class_name(dictionary)

    func_name = _format("{_1}::FillMembersInternal", _1=class_name)
    func_def = FunctionDefinitionNode(
        name=T(func_name),
        arg_decls=[
            T("v8::Isolate* isolate"),
            T("v8::Local<v8::Object> v8_dictionary"),
            T("ExceptionState& exception_state"),
        ],
        return_type=T("void"))

    body = func_def.body
    body.add_template_vars(cg_context.template_bindings())
    body.add_template_var("isolate", "isolate")
    body.add_template_var("exception_state", "exception_state")

    if dictionary.inherited:
        text = """\
BaseClass::FillMembersInternal(${isolate}, v8_dictionary, ${exception_state});
if (${exception_state}.HadException()) {
  return;
}
"""
        body.append(T(text))

    body.extend([
        T("const v8::Eternal<v8::Name>* member_names = "
          "GetV8MemberNames(${isolate});"),
        T("v8::TryCatch try_block(${isolate});"),
        T("v8::Local<v8::Context> current_context = "
          "${isolate}->GetCurrentContext();"),
        T("v8::Local<v8::Value> v8_value;"),
    ])

    # TODO(peria): Support origin-trials and runtime enabled features.
    for key_index, member in enumerate(own_members):
        body.append(make_fill_own_dict_member(key_index, member))

    return func_def


def make_fill_own_dict_member(key_index, member):
    assert isinstance(key_index, int)
    assert isinstance(member, web_idl.DictionaryMember)

    T = TextNode

    pattern = """
if (!v8_dictionary->Get(current_context, member_names[{_1}].Get(${isolate}))
         .ToLocal(&v8_memer)) {{
  ${exception_state}.RethrowV8Exception(try_block.Exception());
  return;
}}"""
    get_v8_value_node = T(_format(pattern, _1=key_index))

    api_call_node = SymbolScopeNode()
    api_call_node.register_code_symbol(
        make_v8_to_blink_value("blink_value", "v8_value", member.idl_type))
    _1 = _blink_member_name(member).set_api
    api_call_node.append(T(_format("{_1}(${blink_value});", _1=_1)))

    throw_if_required_member_is_missing_node = None
    if member.is_required:
        pattern = """\
${exception_state}.ThrowTypeError(
    ExceptionMessages::FailedToGet(
        "{_1}", "${{dictionary.identifier}}",
        "Required member is undefined."));
"""
        throw_if_required_member_is_missing_node = T(
            _format(pattern, _1=member.identifier))

    node = SequenceNode([
        get_v8_value_node,
        T("if (!v8_value->IsUndefined()) {"),
        api_call_node,
        T("} else {") if throw_if_required_member_is_missing_node else None,
        throw_if_required_member_is_missing_node,
        T("}"),
    ])

    conditional = expr_from_exposure(member.exposure)
    if not conditional.is_always_true:
        node = SequenceNode([
            T(_format("if ({}) {{", conditional.to_text())),
            node,
            T("}"),
        ])

    return node


def make_dict_trace_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary
    own_members = dictionary.own_members
    class_name = blink_class_name(dictionary)

    func_def = FunctionDefinitionNode(
        name=T(_format("{_1}::Trace", _1=class_name)),
        arg_decls=[
            T("Visitor* visitor"),
        ],
        return_type=T("void"))

    body = func_def.body
    body.add_template_vars(cg_context.template_bindings())

    def trace_member_node(member):
        pattern = "TraceIfNeeded<{_1}>::Trace(visitor, {_2});"
        _1 = blink_type_info(member.idl_type).member_t
        _2 = _blink_member_name(member).value_var
        return T(_format(pattern, _1=_1, _2=_2))

    body.extend(map(trace_member_node, own_members))

    if dictionary.inherited:
        body.append(T("BaseClass::Trace(visitor);"))

    return func_def


def generate_dictionaries(web_idl_database, output_dirs):
    dictionary = web_idl_database.find("MediaDecodingConfiguration")
    filename = "example_dictionary.cc"
    filepath = os.path.join(output_dirs['core'], filename)

    cg_context = CodeGenContext(dictionary=dictionary)

    root_node = SymbolScopeNode(separator_last="\n")
    root_node.set_renderer(MakoRenderer())

    code_node = SequenceNode()

    code_node.extend([
        make_get_v8_dict_member_names_def(cg_context),
        make_dict_create_def(cg_context),
        make_fill_dict_members_def(cg_context),
        make_fill_dict_members_internal_def(cg_context),
        make_fill_with_dict_members_def(cg_context),
        make_fill_with_own_dict_members_def(cg_context),
        make_dict_trace_def(cg_context),
    ])

    for member in dictionary.own_members:
        code_node.extend([
            make_dict_member_get_def(cg_context.make_copy(dict_member=member)),
            make_dict_member_has_def(cg_context.make_copy(dict_member=member)),
            make_dict_member_set_def(cg_context.make_copy(dict_member=member)),
        ])

    root_node.extend([
        make_copyright_header(),
        TextNode(""),
        enclose_with_namespace(code_node, name_style.namespace("blink")),
    ])

    write_code_node_to_file(root_node, filepath)
