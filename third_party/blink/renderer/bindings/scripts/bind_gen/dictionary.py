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
from .code_node import Likeliness
from .code_node import ListNode
from .code_node import SequenceNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .code_node_cxx import CxxFuncDeclNode
from .code_node_cxx import CxxFuncDefNode
from .code_node_cxx import CxxIfElseNode
from .code_node_cxx import CxxLikelyIfNode
from .codegen_accumulator import CodeGenAccumulator
from .codegen_context import CodeGenContext
from .codegen_expr import expr_from_exposure
from .codegen_format import format_template as _format
from .codegen_utils import collect_include_headers
from .codegen_utils import enclose_with_header_guard
from .codegen_utils import enclose_with_namespace
from .codegen_utils import make_copyright_header
from .codegen_utils import make_forward_declarations
from .codegen_utils import make_header_include_directives
from .codegen_utils import write_code_node_to_file
from .mako_renderer import MakoRenderer
from .path_manager import PathManager


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

    func_name = T(
        _format("${class_name}::{}",
                _blink_member_name(member).get_api))
    func_def = CxxFuncDefNode(
        name=func_name,
        arg_decls=[],
        return_type=blink_type_info(member.idl_type).ref_t,
        const=True)
    func_def.add_template_vars(cg_context.template_bindings())

    body = func_def.body

    _1 = _blink_member_name(member).has_api
    body.append(T(_format("DCHECK({_1}());", _1=_1)))

    _1 = _blink_member_name(member).value_var
    body.append(T(_format("return {_1};", _1=_1)))

    return func_def


def make_dict_member_has_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    member = cg_context.dict_member

    func_name = T(
        _format("${class_name}::{}",
                _blink_member_name(member).has_api))
    func_def = CxxFuncDefNode(
        name=func_name, arg_decls=[], return_type="bool", const=True)
    func_def.add_template_vars(cg_context.template_bindings())

    body = func_def.body

    _1 = _member_presence_expr(member)
    body.append(T(_format("return {_1};", _1=_1)))

    return func_def


def make_dict_member_set_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    member = cg_context.dict_member

    func_name = T(
        _format("${class_name}::{}",
                _blink_member_name(member).set_api))
    func_def = CxxFuncDefNode(
        name=func_name,
        arg_decls=[
            _format("{} value",
                    blink_type_info(member.idl_type).ref_t)
        ],
        return_type="void")
    func_def.add_template_vars(cg_context.template_bindings())

    body = func_def.body

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

    func_def = CxxFuncDefNode(
        name=T("${class_name}::GetV8MemberNames"),
        arg_decls=["v8::Isolate* isolate"],
        return_type="const v8::Eternal<v8::Name>*")
    func_def.add_template_vars(cg_context.template_bindings())

    body = func_def.body

    pattern = ("static const char* kKeyStrings[] = {{{_1}}};")
    _1 = ", ".join(
        _format("\"{}\"", member.identifier)
        for member in dictionary.own_members)
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

    func_def = CxxFuncDefNode(
        name=T("${class_name}::FillWithMembers"),
        arg_decls=[
            "v8::Isolate* isolate",
            "v8::Local<v8::Object> creation_context",
            "v8::Local<v8::Object> v8_dictionary",
        ],
        return_type="bool",
        const=True)
    func_def.add_template_vars(cg_context.template_bindings())

    body = func_def.body

    if dictionary.inherited:
        text = """\
if (!BaseClass::FillWithMembers(isolate, creation_context, v8_dictionary)) {
  return false;
}"""
        body.append(T(text))

    body.append(
        T("return FillWithOwnMembers("
          "isolate, creation_context, v8_dictionary);"))

    return func_def


def make_fill_with_own_dict_members_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary
    own_members = dictionary.own_members

    func_def = CxxFuncDefNode(
        name=T("${class_name}::FillWithOwnMembers"),
        arg_decls=[
            "v8::Isolate* isolate",
            "v8::Local<v8::Object> creation_context",
            "v8::Local<v8::Object> v8_dictionary",
        ],
        return_type="bool",
        const=True)
    func_def.add_template_vars(cg_context.template_bindings())

    body = func_def.body

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
            node = CxxLikelyIfNode(cond=conditional, body=node)

        body.append(node)

    body.append(T("return true;"))

    return func_def


def make_dict_create_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary

    func_def = CxxFuncDefNode(
        name=T("${class_name}::Create"),
        arg_decls=[
            "v8::Isolate* isolate",
            "v8::Local<v8::Object> v8_dictionary",
            "ExceptionState& exception_state",
        ],
        return_type=T("${class_name}*"))
    func_def.add_template_vars(cg_context.template_bindings())

    body = func_def.body

    body.append(
        T("""\
${class_name}* dictionary = MakeGarbageCollected<${class_name}>();
dictionary->FillMembers(isolate, v8_dictionary, exception_state);
if (exception_state.HadException()) {
  return nullptr;
}
return dictionary;"""))

    return func_def


def make_fill_dict_members_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary
    own_members = dictionary.own_members
    required_own_members = list(
        member for member in own_members if member.is_required)

    func_def = CxxFuncDefNode(
        name=T("${class_name}::FillMembers"),
        arg_decls=[
            "v8::Isolate* isolate",
            "v8::Local<v8::Object> v8_dictionary",
            "ExceptionState& exception_state",
        ],
        return_type="void")
    func_def.add_template_vars(cg_context.template_bindings())

    body = func_def.body

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

    func_def = CxxFuncDefNode(
        name=T("${class_name}::FillMembersInternal"),
        arg_decls=[
            "v8::Isolate* isolate",
            "v8::Local<v8::Object> v8_dictionary",
            "ExceptionState& exception_state",
        ],
        return_type="void")
    func_def.add_template_vars(cg_context.template_bindings())

    body = func_def.body
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

    if member.is_required:
        exception_pattern = """\
${exception_state}.ThrowTypeError(
    ExceptionMessages::FailedToGet(
        "{}", "${{dictionary.identifier}}",
        "Required member is undefined."));
"""

        check_and_fill_node = CxxIfElseNode(
            cond="!v8_value->IsUndefined()",
            then=api_call_node,
            then_likeliness=Likeliness.LIKELY,
            else_=T(_format(exception_pattern, member.identifier)),
            else_likeliness=Likeliness.UNLIKELY)
    else:
        check_and_fill_node = CxxLikelyIfNode(
            cond="!v8_value->IsUndefined()", body=api_call_node)

    node = SequenceNode([
        get_v8_value_node,
        check_and_fill_node,
    ])

    conditional = expr_from_exposure(member.exposure)
    if not conditional.is_always_true:
        node = CxxLikelyIfNode(cond=conditional, body=node)

    return node


def make_dict_trace_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary
    own_members = dictionary.own_members

    func_def = CxxFuncDefNode(
        name=T("${class_name}::Trace"),
        arg_decls=["Visitor* visitor"],
        return_type="void")
    func_def.add_template_vars(cg_context.template_bindings())

    body = func_def.body

    def trace_member_node(member):
        pattern = "TraceIfNeeded<{_1}>::Trace(visitor, {_2});"
        _1 = blink_type_info(member.idl_type).member_t
        _2 = _blink_member_name(member).value_var
        return T(_format(pattern, _1=_1, _2=_2))

    body.extend(map(trace_member_node, own_members))

    if dictionary.inherited:
        body.append(T("BaseClass::Trace(visitor);"))

    return func_def


def make_dict_class_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary

    alias_base_class_node = None
    if dictionary.inherited:
        alias_base_class_node = T("using BaseClass = ${base_class_name};")

    public_node = ListNode()

    public_node.append(
        T("""\
static ${class_name}* Create();
static ${class_name}* Create(
    v8::Isolate* isolate,
    v8::Local<v8::Object> v8_dictionary,
    ExceptionState& exception_state);
${class_name}() = default;
~${class_name}() = default;

void Trace(Visitor* visitor);
"""))

    # TODO(peria): Consider inlining these accessors.
    for member in dictionary.own_members:
        member_blink_type = blink_type_info(member.idl_type)
        public_node.append(
            CxxFuncDeclNode(
                name=_blink_member_name(member).get_api,
                return_type=member_blink_type.ref_t,
                arg_decls=[],
                const=True))
        public_node.append(
            CxxFuncDeclNode(
                name=_blink_member_name(member).set_api,
                arg_decls=[_format("{} value", member_blink_type.ref_t)],
                return_type="void"))
        public_node.append(
            CxxFuncDeclNode(
                name=_blink_member_name(member).has_api,
                arg_decls=[],
                return_type="bool",
                const=True))

    protected_node = ListNode()

    protected_node.append(
        T("""\
bool FillWithMembers(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    v8::Local<v8::Object> v8_dictionary) const override;
"""))

    private_node = ListNode()

    private_node.append(
        T("""\
static const v8::Eternal<v8::Name>* GetV8MemberNames(v8::Isolate*);

bool FillWithOwnMembers(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    v8::Local<v8::Object> v8_dictionary) const;

void FillMembers(
    v8::Isolate* isolate,
    v8::Local<v8::Object> v8_dictionary,
    ExceptionState& exception_state);
void FillMembersInternal(
    v8::Isolate* isolate,
    v8::Local<v8::Object> v8_dictionary,
    ExceptionState& exception_state);
"""))

    # C++ member variables for values
    # TODO(peria): Set default values.
    for member in dictionary.own_members:
        _1 = blink_type_info(member.idl_type).member_t
        _2 = _blink_member_name(member).value_var
        private_node.append(T(_format("{_1} {_2};", _1=_1, _2=_2)))

    private_node.append(T(""))
    # C++ member variables for precences
    for member in dictionary.own_members:
        if _does_use_presence_flag(member):
            _1 = _blink_member_name(member).presence_var
            private_node.append(T(_format("bool {_1} = false;", _1=_1)))

    node = ListNode()
    node.add_template_vars(cg_context.template_bindings())

    node.extend([
        T("class ${class_name} : public ${base_class_name} {"),
        alias_base_class_node,
        T("public:"),
        public_node,
        T(""),
        T("protected:"),
        protected_node,
        T(""),
        T("private:"),
        private_node,
        T("};"),
    ])

    return node


def generate_dictionary_cc_file(dictionary):
    class_name = blink_class_name(dictionary)
    base_class_name = (blink_class_name(dictionary.inherited)
                       if dictionary.inherited else "bindings::DictionaryBase")
    cg_context = CodeGenContext(
        dictionary=dictionary,
        class_name=class_name,
        base_class_name=base_class_name)

    code_node = ListNode()
    code_node.extend([
        TextNode("// static"),
        make_get_v8_dict_member_names_def(cg_context),
        TextNode("// static"),
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

    root_node = SymbolScopeNode(separator_last="\n")
    root_node.set_accumulator(CodeGenAccumulator())
    root_node.set_renderer(MakoRenderer())

    # TODO(crbug.com/1034398): Use api_path() or impl_path() once we migrate
    # IDL compiler and move generated code of dictionaries.
    h_file_path = PathManager(dictionary).dict_path(ext="h")

    root_node.accumulator.add_include_headers([
        "third_party/blink/renderer/platform/bindings/exception_messages.h",
        "third_party/blink/renderer/platform/bindings/exception_state.h",
        "third_party/blink/renderer/platform/heap/visitor.h",
        "v8/include/v8.h",
    ])

    root_node.extend([
        make_copyright_header(),
        TextNode(""),
        TextNode(_format("#include \"{}\"", h_file_path)),
        TextNode(""),
        make_header_include_directives(root_node.accumulator),
        TextNode(""),
        enclose_with_namespace(code_node, name_style.namespace("blink")),
    ])

    filepath = PathManager.gen_path_to(
        PathManager(dictionary).dict_path(ext="cc"))
    write_code_node_to_file(root_node, filepath)


def generate_dictionary_h_file(dictionary):
    class_name = blink_class_name(dictionary)
    base_class_name = (blink_class_name(dictionary.inherited)
                       if dictionary.inherited else "bindings::DictionaryBase")
    cg_context = CodeGenContext(
        dictionary=dictionary,
        class_name=class_name,
        base_class_name=base_class_name)

    code_node = ListNode()
    code_node.extend([
        make_dict_class_def(cg_context),
    ])

    root_node = SymbolScopeNode(separator_last="\n")
    root_node.set_accumulator(CodeGenAccumulator())
    root_node.set_renderer(MakoRenderer())

    root_node.accumulator.add_include_headers(
        collect_include_headers(dictionary))
    base_class_header = (
        "third_party/blink/renderer/platform/bindings/dictionary_base.h")
    if dictionary.inherited:
        # TODO(crbug.com/1034398): Use api_path() or impl_path() once we
        # migrate IDL compiler and move generated code of dictionaries.
        base_class_header = PathManager(
            dictionary.inherited).dict_path(ext="h")
    root_node.accumulator.add_include_headers([
        base_class_header,
        "v8/include/v8.h",
    ])
    root_node.accumulator.add_class_decls([
        "ExceptionState",
        "Visitor",
    ])

    namespace_node = enclose_with_namespace(
        ListNode([
            make_forward_declarations(root_node.accumulator),
            TextNode(""),
            code_node,
        ]), name_style.namespace("blink"))

    header_guard = name_style.header_guard(
        PathManager(dictionary).dict_path(ext="h"))
    header_guard_node = enclose_with_header_guard(
        ListNode([
            make_header_include_directives(root_node.accumulator),
            TextNode(""),
            namespace_node,
        ]), header_guard)

    root_node.extend([
        make_copyright_header(),
        TextNode(""),
        header_guard_node,
    ])

    filepath = PathManager.gen_path_to(
        PathManager(dictionary).dict_path(ext="h"))
    write_code_node_to_file(root_node, filepath)


def generate_dictionaries(web_idl_database):
    for dictionary in web_idl_database.dictionaries:
        generate_dictionary_cc_file(dictionary)
        generate_dictionary_h_file(dictionary)
