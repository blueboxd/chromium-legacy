# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .identifier_ir_map import IdentifierIRMap
from .idl_compiler import IdlCompiler
from .idl_type import IdlTypeFactory
from .ir_builder import load_and_register_idl_definitions
from .reference import RefByIdFactory


def build_database(filepaths, report_error):
    """
    Compiles IDL definitions in |filepaths| and builds a database.

    Args:
        filepaths: Paths to files of AstGroup.
        report_error: A callback that will be invoked when an error occurs due
            to inconsistent/invalid IDL definitions.  This callback takes an
            error message of type str and return value is not used.  It's okay
            to terminate the program in this callback.
    """

    ir_map = IdentifierIRMap()
    ref_to_idl_type_factory = RefByIdFactory()
    ref_to_idl_def_factory = RefByIdFactory()
    idl_type_factory = IdlTypeFactory()
    load_and_register_idl_definitions(
        filepaths=filepaths,
        register_ir=ir_map.register,
        create_ref_to_idl_def=ref_to_idl_def_factory.create,
        create_ref_to_idl_type=ref_to_idl_type_factory.create,
        idl_type_factory=idl_type_factory)
    compiler = IdlCompiler(
        ir_map=ir_map,
        ref_to_idl_def_factory=ref_to_idl_def_factory,
        ref_to_idl_type_factory=ref_to_idl_type_factory,
        idl_type_factory=idl_type_factory,
        report_error=report_error)
    return compiler.build_database()
