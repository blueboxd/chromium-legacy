# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .common import WithCodeGeneratorInfo
from .common import WithComponent
from .common import WithDebugInfo
from .common import WithExtendedAttributes
from .common import WithIdentifier
from .common import WithOwner
from .identifier_ir_map import IdentifierIRMap
from .idl_type import IdlType
from .reference import RefById
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
            assert inherited is None or isinstance(inherited, RefById)
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

    def __init__(self, ir):
        assert isinstance(ir, Dictionary.IR)
        assert not ir.is_partial

        UserDefinedType.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self,
                                        ir.extended_attributes.make_copy())
        WithCodeGeneratorInfo.__init__(self,
                                       ir.code_generator_info.make_copy())
        WithComponent.__init__(self, components=ir.components)
        WithDebugInfo.__init__(self, ir.debug_info.make_copy())

        self._inherited = ir.inherited
        self._own_members = tuple([
            DictionaryMember(member_ir, owner=self)
            for member_ir in ir.own_members
        ])

    @property
    def inherited_dictionary(self):
        """Returns the inherited dictionary or None."""
        return self._inherited.target_object if self._inherited else None

    @property
    def own_members(self):
        """
        Returns own dictionary members.  Inherited members are not included.

        Note that members are not sorted alphabetically.  The order should be
        the declaration order.
        """
        return self._own_members

    @property
    def members(self):
        """
        Returns all dictionary members including inherited members, sorted in
        order from least to most derived dictionaries and lexicographical order
        within a dictionary.
        """

        def collect_inherited_members(dictionary):
            if dictionary is None:
                return []
            return (collect_inherited_members(dictionary.inherited_dictionary)
                    + sorted(
                        dictionary.own_members,
                        key=lambda member: member.identifier))

        return tuple(collect_inherited_members(self))

    # UserDefinedType overrides
    @property
    def is_dictionary(self):
        return True


class DictionaryMember(WithIdentifier, WithExtendedAttributes,
                       WithCodeGeneratorInfo, WithOwner, WithComponent,
                       WithDebugInfo):
    class IR(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     idl_type=None,
                     default_value=None,
                     extended_attributes=None,
                     code_generator_info=None,
                     component=None,
                     components=None,
                     debug_info=None):
            assert isinstance(idl_type, IdlType)
            assert default_value is None or isinstance(default_value,
                                                       DefaultValue)
            assert not default_value or idl_type.is_optional

            WithIdentifier.__init__(self, identifier)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(
                self, component=component, components=components)
            WithDebugInfo.__init__(self, debug_info)

            self.idl_type = idl_type
            self.default_value = default_value

        def make_copy(self):
            return DictionaryMember.IR(
                identifier=self.identifier,
                idl_type=self.idl_type,
                default_value=self.default_value,
                extended_attributes=self.extended_attributes.make_copy(),
                code_generator_info=self.code_generator_info.make_copy(),
                components=self.components,
                debug_info=self.debug_info.make_copy())

    def __init__(self, ir, owner):
        assert isinstance(ir, DictionaryMember.IR)
        assert isinstance(owner, Dictionary)

        WithIdentifier.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self,
                                        ir.extended_attributes.make_copy())
        WithCodeGeneratorInfo.__init__(self,
                                       ir.code_generator_info.make_copy())
        WithOwner.__init__(self, owner)
        WithComponent.__init__(self, components=ir.components)
        WithDebugInfo.__init__(self, ir.debug_info.make_copy())

        self._idl_type = ir.idl_type
        self._default_value = ir.default_value

    @property
    def idl_type(self):
        """Returns the type."""
        return self._idl_type

    @property
    def is_required(self):
        """Returns True if this is a required dictionary member."""
        return not self._idl_type.is_optional

    @property
    def default_value(self):
        """Returns the default value or None."""
        raise exceptions.NotImplementedError()
