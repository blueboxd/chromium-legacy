# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


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

    def __init__(self, ir_map):
        self._ir_map = ir_map

    def build_database(self):
        self._merge_partials()
        self._merge_mixins()
        self._resolve_inheritances()
        self._resolve_exposures()
        self._define_unions()
        self._generate_database()

    def _generate_database(self):
        """
        Returns an IDL database based on this compiler.
        """
        pass

    def _merge_partials(self):
        """
        Merges partial definitions with corresponding non-partial definitions.
        """
        self._ir_map.move_to_new_phase()
        # TODO(peria): Implement this. http:///crbug.com/839389

    def _merge_mixins(self):
        """
        Merges mixins with interfaces that connected with includes statements.
        """
        self._ir_map.move_to_new_phase()
        # TODO(peria): Implement this. http:///crbug.com/839389

    def _resolve_inheritances(self):
        """
        Resolves inheritances and [Unforgeable]
        """
        self._ir_map.move_to_new_phase()
        # TODO(peria): Implement this. http:///crbug.com/839389

    def _resolve_exposures(self):
        """
        Links [Exposed] interfaces/namespaces with [Global] interfaces
        """
        self._ir_map.move_to_new_phase()
        # TODO(peria): Implement this. http:///crbug.com/839389

    def _define_unions(self):
        """
        Create a definition of union, that is unique in union types that have
        same flattened-like members.
        """
        self._ir_map.move_to_new_phase()
        # TODO(peria): Implement this. http:///crbug.com/839389
