# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .database import Database
from .database import DatabaseBody
from .dictionary import Dictionary
from .identifier_ir_map import IdentifierIRMap
from .idl_type import IdlTypeFactory
from .reference import RefByIdFactory
from .typedef import Typedef
from .user_defined_type import UserDefinedType


class IdlCompiler(object):
    """
    Compiles IRs of Web IDL definitions into a database.

    IdlCompiler works very closely with IRs.  IdlCompiler resolves a lot of
    things such as; merge partial definitions, merge mixins into an interface,
    resolve inheritance, etc.  These tasks must be done in order, and it's
    represented with "(compilation) phase".

    IdlCompiler works proceeding one phase to next phase.  A basic strategy is
    like below.

    1. prev_phase = self._ir_map.current_phase, next_phase = prev_phase + 1
    2. for x = an IR in self._ir_map(phase=prev_phase)
    2.1. y = process_and_update(x.copy())
    2.2. self._ir_map(phase=next_phase).add(y)

    Note that an old IR for 'x' remains internally.  See IdentifierIRMap for
    the details.
    """

    def __init__(self, ir_map, ref_to_idl_def_factory, ref_to_idl_type_factory,
                 idl_type_factory, report_error):
        """
        Args:
            ir_map: IdentifierIRMap filled with the initial IRs of IDL
                definitions.
            ref_to_idl_def_factory: RefByIdFactory that created all references
                to UserDefinedType.
            ref_to_idl_type_factory: RefByIdFactory that created all references
                to IdlType.
            idl_type_factory: IdlTypeFactory that created all instances of
                IdlType.
            report_error: A callback that will be invoked when an error occurs
                due to inconsistent/invalid IDL definitions.  This callback
                takes an error message of type str and return value is not used.
                It's okay to terminate the program in this callback.
        """
        assert isinstance(ir_map, IdentifierIRMap)
        assert isinstance(ref_to_idl_def_factory, RefByIdFactory)
        assert isinstance(ref_to_idl_type_factory, RefByIdFactory)
        assert isinstance(idl_type_factory, IdlTypeFactory)
        assert callable(report_error)
        self._ir_map = ir_map
        self._ref_to_idl_def_factory = ref_to_idl_def_factory
        self._ref_to_idl_type_factory = ref_to_idl_type_factory
        self._idl_type_factory = idl_type_factory
        self._report_error = report_error
        self._db = DatabaseBody()
        self._did_run = False  # Run only once.

    def build_database(self):
        assert not self._did_run
        self._did_run = True

        # Merge partial definitions.
        self._merge_partial_interfaces()
        self._merge_partial_dictionaries()

        # Updates on IRs are finished.  Create API objects.
        self._create_public_objects()

        # Resolve references.
        self._resolve_references_to_idl_def()
        self._resolve_references_to_idl_type()

        return Database(self._db)

    def _merge_partial_interfaces(self):
        def merge_partials(old_interfaces, partial_interfaces):
            for identifier, old_interface in old_interfaces.iteritems():
                new_interface = old_interface.make_copy()
                for partial in partial_interfaces.get(identifier, []):
                    new_interface.add_components(partial.components)
                    new_interface.debug_info.add_locations(
                        partial.debug_info.all_locations)
                    new_interface.attributes.extend([
                        attribute.make_copy()
                        for attribute in partial.attributes
                    ])
                    new_interface.constants.extend([
                        constant.make_copy() for constant in partial.constants
                    ])
                    new_interface.operations.extend([
                        operation.make_copy()
                        for operation in partial.operations
                    ])

            self._ir_map.add(new_interface)

        old_interfaces = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.INTERFACE)
        partial_interfaces = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.PARTIAL_INTERFACE)
        old_mixins = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.INTERFACE_MIXIN)
        partial_mixins = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.PARTIAL_INTERFACE_MIXIN)

        self._ir_map.move_to_new_phase()
        merge_partials(old_interfaces, partial_interfaces)
        merge_partials(old_mixins, partial_mixins)

    def _merge_partial_dictionaries(self):
        old_dictionaries = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.DICTIONARY)
        old_partial_dictionaries = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.PARTIAL_DICTIONARY)

        self._ir_map.move_to_new_phase()

        for identifier, old_dictionary in old_dictionaries.iteritems():
            new_dictionary = old_dictionary.make_copy()
            for partial_dictionary in old_partial_dictionaries.get(
                    identifier, []):
                new_dictionary.add_components(partial_dictionary.components)
                new_dictionary.debug_info.add_locations(
                    partial_dictionary.debug_info.all_locations)
                new_dictionary.own_members.extend([
                    member.make_copy()
                    for member in partial_dictionary.own_members
                ])
            self._ir_map.add(new_dictionary)

    def _create_public_objects(self):
        """Creates public representations of compiled objects."""
        dictionary_irs = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.DICTIONARY)
        for ir in dictionary_irs.itervalues():
            self._db.register(DatabaseBody.Kind.DICTIONARY, Dictionary(ir))

        typedef_irs = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.TYPEDEF)
        for ir in typedef_irs.itervalues():
            self._db.register(DatabaseBody.Kind.TYPEDEF, Typedef(ir))

    def _resolve_references_to_idl_def(self):
        def resolve(ref):
            try:
                idl_def = self._db.find_by_identifier(ref.identifier)
            except KeyError:
                self._report_error("{}: Unresolved reference to {}".format(
                    ref.ref_own_debug_info.location, ref.identifier))
                idl_def = UserDefinedType(ref.identifier)  # dummy stub
            ref.set_target_object(idl_def)

        self._ref_to_idl_def_factory.for_each(resolve)

    def _resolve_references_to_idl_type(self):
        def resolve(ref):
            try:
                idl_def = self._db.find_by_identifier(ref.identifier)
            except KeyError:
                self._report_error("{}: Unresolved reference to {}".format(
                    ref.ref_own_debug_info.location, ref.identifier))
                idl_def = UserDefinedType(ref.identifier)  # dummy stub
            if isinstance(idl_def, UserDefinedType):
                idl_type = self._idl_type_factory.definition_type(
                    user_defined_type=idl_def)
            elif isinstance(idl_def, Typedef):
                idl_type = self._idl_type_factory.typedef_type(typedef=idl_def)
            else:
                assert False
            ref.set_target_object(idl_type)

        self._ref_to_idl_type_factory.for_each(resolve)
