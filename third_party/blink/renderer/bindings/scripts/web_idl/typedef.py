# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithIdentifier
from .identifier_ir_map import IdentifierIRMap
from .make_copy import make_copy


class Typedef(WithIdentifier, WithCodeGeneratorInfo, WithComponent,
              WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-typedefs"""

    class IR(IdentifierIRMap.IR, WithCodeGeneratorInfo, WithComponent,
             WithDebugInfo):
        def __init__(self,
                     identifier,
                     idl_type,
                     code_generator_info=None,
                     component=None,
                     debug_info=None):
            IdentifierIRMap.IR.__init__(
                self,
                identifier=identifier,
                kind=IdentifierIRMap.IR.Kind.TYPEDEF)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

            self.idl_type = idl_type

    def __init__(self, ir):
        assert isinstance(ir, Typedef.IR)

        ir = make_copy(ir)
        WithIdentifier.__init__(self, ir.identifier)
        WithCodeGeneratorInfo.__init__(self, ir.code_generator_info)
        WithComponent.__init__(self, components=ir.components)
        WithDebugInfo.__init__(self, ir.debug_info)

        self._idl_type = ir.idl_type

    @property
    def idl_type(self):
        """Returns the typedef'ed type."""
        return self._idl_type
