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


class Attribute(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo,
                WithOwner, WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-attributes"""

    class IR(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     idl_type,
                     is_static=False,
                     is_readonly=False,
                     does_inherit_getter=False,
                     extended_attributes=None,
                     code_generator_info=None,
                     component=None,
                     components=None,
                     debug_info=None):
            assert isinstance(idl_type, IdlType)
            assert isinstance(is_static, bool)
            assert isinstance(is_readonly, bool)
            assert isinstance(does_inherit_getter, bool)

            WithIdentifier.__init__(self, identifier)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(
                self, component=component, components=components)
            WithDebugInfo.__init__(self, debug_info)

            self.idl_type = idl_type
            self.is_static = is_static
            self.is_readonly = is_readonly
            self.does_inherit_getter = does_inherit_getter

        def make_copy(self):
            return Attribute.IR(
                identifier=self.identifier,
                idl_type=self.idl_type,
                is_static=self.is_static,
                is_readonly=self.is_readonly,
                does_inherit_getter=self.does_inherit_getter,
                extended_attributes=self.extended_attributes.make_copy(),
                code_generator_info=self.code_generator_info.make_copy(),
                components=self.components,
                debug_info=self.debug_info.make_copy())

    def __init__(self, ir, owner):
        assert isinstance(ir, Attribute.IR)

        WithIdentifier.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self,
                                        ir.extended_attributes.make_copy())
        WithCodeGeneratorInfo.__init__(self,
                                       ir.code_generator_info.make_copy())
        WithOwner.__init__(self, owner)
        WithComponent.__init__(self, components=ir.components)
        WithDebugInfo.__init__(self, ir.debug_info.make_copy())

        self._idl_type = ir.idl_type
        self._is_static = ir.is_static
        self._is_readonly = ir.is_readonly
        self._does_inherit_getter = ir.does_inherit_getter

    @property
    def idl_type(self):
        """Returns the type."""
        return self._idl_type

    @property
    def is_static(self):
        """Returns True if this attriute is static."""
        return self._is_static

    @property
    def is_readonly(self):
        """Returns True if this attribute is read only."""
        return self._is_readonly

    @property
    def does_inherit_getter(self):
        """
        Returns True if this attribute inherits its getter.
        https://heycam.github.io/webidl/#dfn-inherit-getter
        """
        return self._does_inherit_getter
