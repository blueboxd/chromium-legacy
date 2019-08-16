# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .callback_function import CallbackFunction
from .callback_interface import CallbackInterface
from .composition_parts import Identifier
from .database import Database
from .database import DatabaseBody
from .dictionary import Dictionary
from .enumeration import Enumeration
from .identifier_ir_map import IdentifierIRMap
from .idl_type import IdlTypeFactory
from .interface import Interface
from .reference import RefByIdFactory
from .typedef import Typedef
from .union import Union
from .user_defined_type import StubUserDefinedType
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
        # Merge mixins.
        self._merge_interface_mixins()
        # Process inheritances.
        self._process_interface_inheritances()

        # Updates on IRs are finished.  Create API objects.
        self._create_public_objects()

        # Resolve references.
        self._resolve_references_to_idl_def()
        self._resolve_references_to_idl_type()

        # Build union API objects.
        self._create_public_unions()

        return Database(self._db)

    def _merge_partial_interfaces(self):
        old_interfaces = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.INTERFACE)
        partial_interfaces = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.PARTIAL_INTERFACE)
        old_mixins = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.INTERFACE_MIXIN)
        partial_mixins = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.PARTIAL_INTERFACE_MIXIN)

        self._ir_map.move_to_new_phase()
        self._merge_interfaces(old_interfaces, partial_interfaces)
        self._merge_interfaces(old_mixins, partial_mixins)

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

    def _merge_interface_mixins(self):
        interfaces = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.INTERFACE)
        interface_mixins = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.INTERFACE_MIXIN)

        identifier_to_mixin_map = {
            identifier: [
                interface_mixins[include.mixin_identifier]
                for include in includes
            ]
            for identifier, includes in self._ir_map.find_by_kind(
                IdentifierIRMap.IR.Kind.INCLUDES).iteritems()
        }

        self._ir_map.move_to_new_phase()
        self._merge_interfaces(interfaces, identifier_to_mixin_map)

    def _merge_interfaces(self, old_interfaces, interfaces_to_merge):
        for identifier, old_interface in old_interfaces.iteritems():
            new_interface = old_interface.make_copy()
            for to_merge in interfaces_to_merge.get(identifier, []):
                new_interface.add_components(to_merge.components)
                new_interface.debug_info.add_locations(
                    to_merge.debug_info.all_locations)
                new_interface.attributes.extend([
                    attribute.make_copy() for attribute in to_merge.attributes
                ])
                new_interface.constants.extend(
                    [constant.make_copy() for constant in to_merge.constants])
                new_interface.operations.extend([
                    operation.make_copy() for operation in to_merge.operations
                ])
            self._ir_map.add(new_interface)

    def _process_interface_inheritances(self):
        def is_own_member(member):
            return 'Unfogeable' in member.extended_attributes

        def create_inheritance_stack(obj, table):
            if obj.inherited is None:
                return [obj]
            return [obj] + create_inheritance_stack(
                table[obj.inherited.identifier], table)

        old_interfaces = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.INTERFACE)
        self._ir_map.move_to_new_phase()
        for old_interface in old_interfaces.itervalues():
            new_interface = old_interface.make_copy()
            inheritance_stack = create_inheritance_stack(
                old_interface, old_interfaces)
            for interface in inheritance_stack[1:]:
                new_interface.attributes.extend([
                    attribute.make_copy() for attribute in interface.attributes
                    if is_own_member(attribute)
                ])
                new_interface.operations.extend([
                    operation.make_copy() for operation in interface.operations
                    if is_own_member(operation)
                ])
            self._ir_map.add(new_interface)

    def _create_public_objects(self):
        """Creates public representations of compiled objects."""
        interface_irs = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.INTERFACE)
        for ir in interface_irs.itervalues():
            self._db.register(DatabaseBody.Kind.INTERFACE, Interface(ir))

        dictionary_irs = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.DICTIONARY)
        for ir in dictionary_irs.itervalues():
            self._db.register(DatabaseBody.Kind.DICTIONARY, Dictionary(ir))

        callback_interface_irs = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.CALLBACK_INTERFACE)
        for ir in callback_interface_irs.itervalues():
            self._db.register(DatabaseBody.Kind.CALLBACK_INTERFACE,
                              CallbackInterface(ir))

        callback_function_irs = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.CALLBACK_FUNCTION)
        for ir in callback_function_irs.itervalues():
            self._db.register(DatabaseBody.Kind.CALLBACK_FUNCTION,
                              CallbackFunction(ir))

        enum_irs = self._ir_map.find_by_kind(
            IdentifierIRMap.IR.Kind.ENUMERATION)
        for ir in enum_irs.itervalues():
            self._db.register(DatabaseBody.Kind.ENUMERATION, Enumeration(ir))

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
                idl_def = StubUserDefinedType(ref.identifier)
            ref.set_target_object(idl_def)

        self._ref_to_idl_def_factory.for_each(resolve)

    def _resolve_references_to_idl_type(self):
        def resolve(ref):
            try:
                idl_def = self._db.find_by_identifier(ref.identifier)
            except KeyError:
                self._report_error("{}: Unresolved reference to {}".format(
                    ref.ref_own_debug_info.location, ref.identifier))
                idl_def = StubUserDefinedType(ref.identifier)
            if isinstance(idl_def, UserDefinedType):
                idl_type = self._idl_type_factory.definition_type(
                    user_defined_type=idl_def)
            elif isinstance(idl_def, Typedef):
                idl_type = self._idl_type_factory.typedef_type(typedef=idl_def)
            else:
                assert False
            ref.set_target_object(idl_type)

        self._ref_to_idl_type_factory.for_each(resolve)

    def _create_public_unions(self):
        all_union_types = []  # all instances of UnionType

        def collect_unions(idl_type):
            if idl_type.is_union:
                all_union_types.append(idl_type)

        self._idl_type_factory.for_each(collect_unions)

        def unique_key(union_type):
            """
            Returns an unique (but meaningless) key.  Returns the same key for
            the identical union types.
            """
            key_pieces = sorted([
                idl_type.syntactic_form
                for idl_type in union_type.flattened_member_types
            ])
            if union_type.does_include_nullable_type:
                key_pieces.append('type null')  # something unique
            return '|'.join(key_pieces)

        grouped_unions = {}  # {unique key: list of union types}
        for union_type in all_union_types:
            key = unique_key(union_type)
            grouped_unions.setdefault(key, []).append(union_type)

        grouped_typedefs = {}  # {unique key: list of typedefs to the union}
        all_typedefs = self._db.find_by_kind(DatabaseBody.Kind.TYPEDEF)
        for typedef in all_typedefs.itervalues():
            if not typedef.idl_type.is_union:
                continue
            key = unique_key(typedef.idl_type)
            grouped_typedefs.setdefault(key, []).append(typedef)

        for key, union_types in grouped_unions.iteritems():
            self._db.register(
                DatabaseBody.Kind.UNION,
                Union(
                    Identifier(key),  # dummy identifier
                    union_types=union_types,
                    typedef_backrefs=grouped_typedefs.get(key, [])))
