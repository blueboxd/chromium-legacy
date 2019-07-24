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

    def __init__(self, ir_map, ref_to_idl_type_factory, ref_to_idl_def_factory,
                 idl_type_factory):
        """
        Args:
            ir_map: IdentifierIRMap filled with the initial IRs of IDL
                definitions.
            ref_to_idl_type_factory: RefByIdFactory that created all references
                to IdlType.
            ref_to_idl_def_factory: RefByIdFactory that created all references
                to UserDefinedType.
            idl_type_factory: IdlTypeFactory that created all instances of
                IdlType.
        """
        assert isinstance(ir_map, IdentifierIRMap)
        assert isinstance(ref_to_idl_type_factory, RefByIdFactory)
        assert isinstance(ref_to_idl_def_factory, RefByIdFactory)
        assert isinstance(idl_type_factory, IdlTypeFactory)
        self._ir_map = ir_map
        self._ref_to_idl_type_factory = ref_to_idl_type_factory
        self._ref_to_idl_def_factory = ref_to_idl_def_factory
        self._idl_type_factory = idl_type_factory
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
        self._resolve_references_to_idl_type()
        self._resolve_references_to_idl_def()

        return Database(self._db)

    def _merge_partial_interfaces(self):
        pass

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

    def _resolve_references_to_idl_type(self):
        def resolve(ref):
            # Resolve to stubs for the time being.
            ref.set_target_object(False)

        self._ref_to_idl_type_factory.for_each(resolve)

    def _resolve_references_to_idl_def(self):
        def resolve(ref):
            try:
                ref.set_target_object(
                    self._db.find_by_identifier(ref.identifier))
            except KeyError:
                # Resolve to stubs for the time being.
                ref.set_target_object(False)

        self._ref_to_idl_def_factory.for_each(resolve)
