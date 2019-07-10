# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .common import WithCodeGeneratorInfo
from .common import WithComponent
from .common import WithDebugInfo
from .common import WithExtendedAttributes
from .common import WithIdentifier
from .identifier_ir_map import IdentifierIRMap
from .idl_member import IdlMember
from .idl_reference_proxy import RefByIdFactory
from .idl_types import IdlType
from .user_defined_type import UserDefinedType
from .values import DefaultValue


class Dictionary(UserDefinedType, WithExtendedAttributes,
                 WithCodeGeneratorInfo, WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-dictionaries"""

    class IR(IdentifierIRMap.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     is_partial,
                     inherited=None,
                     own_members=None,
                     extended_attributes=None,
                     code_generator_info=None,
                     component=None,
                     components=None,
                     debug_info=None):
            assert isinstance(is_partial, bool)
            assert inherited is None or RefByIdFactory.is_reference(inherited)
            assert isinstance(own_members, (list, tuple)) and all(
                isinstance(member, DictionaryMember.IR)
                for member in own_members)

            kind = (IdentifierIRMap.IR.Kind.PARTIAL_DICTIONARY
                    if is_partial else IdentifierIRMap.IR.Kind.DICTIONARY)
            IdentifierIRMap.IR.__init__(self, identifier=identifier, kind=kind)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(
                self, component=component, components=components)
            WithDebugInfo.__init__(self, debug_info)

            self.is_partial = is_partial
            self.inherited = inherited
            self.own_members = own_members

        def make_copy(self):
            return Dictionary.IR(
                identifier=self.identifier,
                is_partial=self.is_partial,
                inherited=self.inherited,
                own_members=map(DictionaryMember.IR.make_copy,
                                self.own_members),
                extended_attributes=self.extended_attributes.make_copy(),
                code_generator_info=self.code_generator_info.make_copy(),
                components=self.components,
                debug_info=self.debug_info.make_copy())

    @property
    def inherited_dictionary(self):
        """
        Returns an Dictionary which this dictionary is inherited from.
        @return Dictionary
        """
        raise exceptions.NotImplementedError()

    @property
    def own_members(self):
        """
        Returns dictionary members which do not include inherited
        Dictionaries' members.
        @return tuple(DictionaryMember)
        """
        raise exceptions.NotImplementedError()

    @property
    def members(self):
        """
        Returns dictionary members including inherited Dictionaries' members.
        @return tuple(DictionaryMember)
        """
        raise exceptions.NotImplementedError()

    # UserDefinedType overrides
    @property
    def is_dictionary(self):
        return True


class DictionaryMember(IdlMember):
    class IR(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     idl_type=None,
                     is_required=False,
                     default_value=None,
                     extended_attributes=None,
                     code_generator_info=None,
                     component=None,
                     components=None,
                     debug_info=None):
            assert isinstance(idl_type, IdlType)
            assert isinstance(is_required, bool)
            assert default_value is None or isinstance(default_value,
                                                       DefaultValue)

            WithIdentifier.__init__(self, identifier)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(
                self, component=component, components=components)
            WithDebugInfo.__init__(self, debug_info)

            self.idl_type = idl_type
            self.is_required = is_required
            self.default_value = default_value

        def make_copy(self):
            return DictionaryMember.IR(
                identifier=self.identifier,
                idl_type=self.idl_type,
                is_required=self.is_required,
                default_value=self.default_value,
                extended_attributes=self.extended_attributes.make_copy(),
                code_generator_info=self.code_generator_info.make_copy(),
                components=self.components,
                debug_info=self.debug_info.make_copy())

    @property
    def idl_type(self):
        """
        Returns type of this member.
        @return IdlType
        """
        raise exceptions.NotImplementedError()

    @property
    def is_required(self):
        """
        Returns if this member is required.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def default_value(self):
        """
        Returns the default value if it is specified. Otherwise, None
        @return DefaultValue?
        """
        raise exceptions.NotImplementedError()
