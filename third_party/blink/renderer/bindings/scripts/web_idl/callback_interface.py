# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions

from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExtendedAttributes
from .identifier_ir_map import IdentifierIRMap
from .user_defined_type import UserDefinedType


class CallbackInterface(UserDefinedType, WithExtendedAttributes,
                        WithCodeGeneratorInfo, WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-interfaces"""

    class IR(IdentifierIRMap.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     extended_attributes=None,
                     code_generator_info=None,
                     component=None,
                     debug_info=None):
            IdentifierIRMap.IR.__init__(
                self,
                identifier=identifier,
                kind=IdentifierIRMap.IR.Kind.CALLBACK_INTERFACE)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

    def __init__(self, ir):
        assert isinstance(ir, CallbackInterface.IR)

        UserDefinedType.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self,
                                        ir.extended_attributes.make_copy())
        WithCodeGeneratorInfo.__init__(self,
                                       ir.code_generator_info.make_copy())
        WithComponent.__init__(self, components=ir.components)
        WithDebugInfo.__init__(self, ir.debug_info.make_copy())

    @property
    def inherited_callback_interface(self):
        """
        Returns an CallbackInterface which this interface is inherited from.
        @return CallbackInterface?
        """
        raise exceptions.NotImplementedError()

    @property
    def operation_groups(self):
        """
        Returns a list of OperationGroup. Each OperationGroup may have an
        operation or a set of overloaded operations.
        @return tuple(OperationGroup)
        """
        raise exceptions.NotImplementedError()

    @property
    def constants(self):
        """
        Returns a list of constants.
        @return tuple(Constant)
        """
        raise exceptions.NotImplementedError()

    # UserDefinedType overrides
    @property
    def is_callback_interface(self):
        return True
