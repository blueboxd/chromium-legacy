# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .common import WithCodeGeneratorInfo
from .common import WithComponent
from .common import WithDebugInfo
from .identifier_ir_map import IdentifierIRMap


class Includes(WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#include"""

    class IR(IdentifierIRMap.IR, WithCodeGeneratorInfo, WithComponent,
             WithDebugInfo):
        def __init__(self,
                     interface_identifier,
                     mixin_identifier,
                     code_generator_info=None,
                     component=None,
                     debug_info=None):
            # Includes statements are treated similarly to partial
            # definitions, and it's convenient for IdlCompiler that
            # 'includes' are grouped by interface's identifier, i.e.
            # a group of mixins are merged into the interface.
            # So, we take the interface's identifier as this IR's
            # identifier.
            IdentifierIRMap.IR.__init__(
                self,
                identifier=interface_identifier,
                kind=IdentifierIRMap.IR.Kind.INCLUDES)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

            self.interafce_identifier = interface_identifier
            self.mixin_identifier = mixin_identifier

    @property
    def interface(self):
        """
        Returns the interface that includes the mixin.
        @return Interface
        """
        raise exceptions.NotImplementedError()

    @property
    def mixin(self):
        """
        Returns the interface mixin that is included by the other.
        @return Interface
        """
        raise exceptions.NotImplementedError()
