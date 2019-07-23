# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .identifier_ir_map import IdentifierIRMap
from .idl_compiler import IdlCompiler
from .idl_type import IdlTypeFactory
from .ir_builder import load_and_register_idl_definitions
from .reference import RefByIdFactory


def build_database(filepaths):
    """Compiles IDL definitions in |filepaths| and builds a database."""

    ir_map = IdentifierIRMap()
    ref_to_idl_type_factory = RefByIdFactory()
    ref_to_idl_def_factory = RefByIdFactory()
    idl_type_factory = IdlTypeFactory()
    load_and_register_idl_definitions(
        filepaths, ir_map.register, ref_to_idl_type_factory.create,
        ref_to_idl_def_factory.create, idl_type_factory)
    compiler = IdlCompiler(ir_map, ref_to_idl_type_factory,
                           ref_to_idl_def_factory, idl_type_factory)
    return compiler.build_database()
