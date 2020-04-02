# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .ir_map import IRMap


def validate_after_resolve_references(ir_map):
    _validate_literal_constant_type(ir_map)


def _all_constants(ir_map):
    accumulated = []
    irs = ir_map.irs_of_kinds(IRMap.IR.Kind.CALLBACK_INTERFACE,
                              IRMap.IR.Kind.INTERFACE, IRMap.IR.Kind.NAMESPACE)
    for ir in irs:
        accumulated.extend(ir.constants)
    return accumulated


def _all_function_likes(ir_map):
    accumulated = []
    irs = ir_map.irs_of_kinds(IRMap.IR.Kind.CALLBACK_INTERFACE,
                              IRMap.IR.Kind.INTERFACE, IRMap.IR.Kind.NAMESPACE)
    for ir in irs:
        accumulated.extend(ir.constructors)
        accumulated.extend(ir.named_constructors)
        accumulated.extend(ir.operations)
    accumulated.extend(ir_map.irs_of_kinds(IRMap.IR.Kind.CALLBACK_FUNCTION))
    return accumulated


def _validate_literal_constant_type(ir_map):
    for constant in _all_constants(ir_map):
        assert constant.value.is_type_compatible_with(constant.idl_type), (
            "{}: {}: The value is incompatible with the type.".format(
                constant.debug_info.location, constant.identifier))

    for function in _all_function_likes(ir_map):
        for argument in function.arguments:
            if argument.default_value is None:
                continue
            assert argument.default_value.is_type_compatible_with(
                argument.idl_type
            ), ("{}: {}, {}: The default value is incompatible with the type.".
                format(function.debug_info.location, function.identifier,
                       argument.identifier))

    for dictionary in ir_map.irs_of_kind(IRMap.IR.Kind.DICTIONARY):
        for dict_member in dictionary.own_members:
            if dict_member.default_value is None:
                continue
            assert dict_member.default_value.is_type_compatible_with(
                dict_member.idl_type
            ), ("{}: {}, {}: The default value is incompatible with the type.".
                format(dict_member.debug_info.location, dictionary.identifier,
                       dict_member.identifier))
