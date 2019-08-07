# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExtendedAttributes
from .composition_parts import WithIdentifier
from .composition_parts import WithOwner
from .idl_type import IdlType
from .values import ConstantValue


class Constant(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo,
               WithOwner, WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-constants"""

    class IR(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     idl_type,
                     value,
                     extended_attributes=None,
                     code_generator_info=None,
                     component=None,
                     components=None,
                     debug_info=None):
            assert isinstance(idl_type, IdlType)
            assert isinstance(value, ConstantValue)

            WithIdentifier.__init__(self, identifier)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(
                self, component=component, components=components)
            WithDebugInfo.__init__(self, debug_info)

            self.idl_type = idl_type
            self.value = value

        def make_copy(self):
            return Constant.IR(
                identifier=self.identifier,
                idl_type=self.idl_type,
                value=self.value,
                extended_attributes=self.extended_attributes.make_copy(),
                code_generator_info=self.code_generator_info.make_copy(),
                components=self.components,
                debug_info=self.debug_info.make_copy())

    def __init__(self, ir, owner):
        assert isinstance(ir, Constant.IR)

        WithIdentifier.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self,
                                        ir.extended_attributes.make_copy())
        WithCodeGeneratorInfo.__init__(self,
                                       ir.code_generator_info.make_copy())
        WithOwner.__init__(self, owner)
        WithComponent.__init__(self, components=ir.components)
        WithDebugInfo.__init__(self, ir.debug_info.make_copy())

        self._idl_type = ir.idl_type
        self._value = ir.value

    @property
    def idl_type(self):
        """Returns the type"""
        return self._idl_type

    @property
    def value(self):
        """Returns the value."""
        return self._value
