# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .common import WithCodeGeneratorInfo
from .common import WithComponent
from .common import WithDebugInfo
from .common import WithIdentifier
from .identifier_ir_map import IdentifierIRMap


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

        WithIdentifier.__init__(self, ir.identifier)
        WithCodeGeneratorInfo.__init__(self,
                                       ir.code_generator_info.make_copy())
        WithComponent.__init__(self, components=ir.components)
        WithDebugInfo.__init__(self, ir.debug_info.make_copy())

        self._idl_type = ir.idl_type

    @property
    def idl_type(self):
        """Returns the typedef'ed type."""
        return self._idl_type
