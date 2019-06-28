# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .common import WithCodeGeneratorInfo
from .common import WithComponent
from .common import WithDebugInfo
from .common import WithExtendedAttributes
from .identifier_ir_map import IdentifierIRMap
from .idl_definition import IdlDefinition
from .idl_member import IdlMember


class Dictionary(IdlDefinition):
    """https://heycam.github.io/webidl/#idl-dictionaries"""

    class IR(IdentifierIRMap.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     is_partial,
                     extended_attributes=None,
                     code_generator_info=None,
                     component=None,
                     debug_info=None):
            kind = (IdentifierIRMap.IR.Kind.PARTIAL_DICTIONARY
                    if is_partial else IdentifierIRMap.IR.Kind.DICTIONARY)
            IdentifierIRMap.IR.__init__(self, identifier=identifier, kind=kind)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

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


class DictionaryMember(IdlMember):
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
