# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .argument import Argument
from .common import WithCodeGeneratorInfo
from .common import WithComponent
from .common import WithDebugInfo
from .common import WithExposure
from .common import WithExtendedAttributes
from .common import WithIdentifier
from .common import WithOwner
from .idl_member import IdlMember
from .idl_type import IdlType


class Operation(IdlMember):
    """https://heycam.github.io/webidl/#idl-operations
    https://www.w3.org/TR/WebIDL-1/#idl-special-operations"""

    class IR(WithIdentifier, WithExtendedAttributes, WithExposure,
             WithCodeGeneratorInfo, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     arguments,
                     return_type,
                     is_static=False,
                     is_getter=False,
                     is_setter=False,
                     is_deleter=False,
                     extended_attributes=None,
                     exposures=None,
                     code_generator_info=None,
                     component=None,
                     debug_info=None):
            assert isinstance(arguments, (list, tuple)) and all(
                isinstance(arg, Argument.IR) for arg in arguments)
            assert isinstance(return_type, IdlType)
            assert isinstance(is_static, bool)
            assert isinstance(is_getter, bool)
            assert isinstance(is_setter, bool)
            assert isinstance(is_deleter, bool)
            assert int(is_getter) + int(is_setter) + int(is_deleter) <= 1

            WithIdentifier.__init__(self, identifier)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithExposure.__init__(self, exposures)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

            self.arguments = list(arguments)
            self.return_type = return_type
            self.is_static = is_static
            self.is_getter = is_getter
            self.is_setter = is_setter
            self.is_deleter = is_deleter
            self.is_property_handler = is_getter or is_setter or is_deleter

    @property
    def is_static(self):
        """
        Returns True if 'static' is specified.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def return_type(self):
        """
        Returns the type of return value.
        @return IdlType
        """
        raise exceptions.NotImplementedError()

    @property
    def arguments(self):
        """
        Returns a list of arguments.
        @return tuple(Argument)
        """
        raise exceptions.NotImplementedError()

    @property
    def overloaded_index(self):
        """
        Returns the index in the OperationGroup that |self| belongs to.
        @return int
        """
        raise exceptions.NotImplementedError()


class OperationGroup(WithIdentifier, WithCodeGeneratorInfo, WithOwner,
                     WithComponent, WithDebugInfo):
    """
    OperationGroup class has all Operation's with a same identifier, even if the
    operation is not overloaded. Then we can handle overloaded and
    non-overloaded operations seamlessly.
    From the ES bindings' view point, OperationGroup tells something for properties,
    and Operation tells something for actual behaviors.
    """

    def operations(self):
        """
        Returns a list of operations whose identifier is |identifier()|
        @return tuple(Operation)
        """
        raise exceptions.NotImplementedError()
