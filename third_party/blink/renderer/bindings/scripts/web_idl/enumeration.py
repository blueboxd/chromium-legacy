# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .common import WithCodeGeneratorInfo
from .common import WithComponent
from .common import WithDebugInfo
from .common import WithExtendedAttributes
from .identifier_ir_map import IdentifierIRMap
from .user_defined_type import UserDefinedType


class Enumeration(UserDefinedType, WithExtendedAttributes,
                  WithCodeGeneratorInfo, WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-enums"""

    class IR(IdentifierIRMap.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     values,
                     extended_attributes=None,
                     code_generator_info=None,
                     component=None,
                     debug_info=None):
            IdentifierIRMap.IR.__init__(
                self,
                identifier=identifier,
                kind=IdentifierIRMap.IR.Kind.ENUMERATION)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

            assert isinstance(values, (list, tuple))
            self.values = tuple(values)

    @property
    def values(self):
        """
        Returns a list of values listed in this enumeration.
        @return tuple(str)
        """
        raise exceptions.NotImplementedError()

    # UserDefinedType overrides
    @property
    def is_enumeration(self):
        return True
