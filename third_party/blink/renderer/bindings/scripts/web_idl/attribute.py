# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .common import WithCodeGeneratorInfo
from .common import WithComponent
from .common import WithDebugInfo
from .common import WithExposure
from .common import WithExtendedAttributes
from .common import WithIdentifier
from .idl_member import IdlMember
from .idl_type import IdlType


class Attribute(IdlMember):
    """https://heycam.github.io/webidl/#idl-attributes"""

    class IR(WithIdentifier, WithExtendedAttributes, WithExposure,
             WithCodeGeneratorInfo, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     idl_type,
                     is_static=False,
                     is_readonly=False,
                     does_inherit_getter=False,
                     extended_attributes=None,
                     exposures=None,
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
            WithExposure.__init__(self, exposures)
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

    @property
    def idl_type(self):
        """
        Returns type of this attribute.
        @return IdlType
        """
        raise exceptions.NotImplementedError()

    @property
    def is_static(self):
        """
        Returns True if this attriute is static.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_readonly(self):
        """
        Returns True if this attribute is read only.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def does_inherit_getter(self):
        """
        Returns True if |self| inherits its getter.
        https://heycam.github.io/webidl/#dfn-inherit-getter
        @return bool
        """
        raise exceptions.NotImplementedError()
