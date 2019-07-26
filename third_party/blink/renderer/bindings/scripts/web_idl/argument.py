# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .common import WithIdentifier
from .common import WithExtendedAttributes
from .common import WithCodeGeneratorInfo
from .common import WithOwner
from .idl_type import IdlType
from .values import DefaultValue


class Argument(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo,
               WithOwner):
    class IR(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo):
        def __init__(self,
                     identifier,
                     index,
                     idl_type,
                     default_value=None,
                     extended_attributes=None,
                     code_generator_info=None):
            assert isinstance(index, int)
            assert isinstance(idl_type, IdlType)
            assert (default_value is None
                    or isinstance(default_value, DefaultValue))

            WithIdentifier.__init__(self, identifier)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)

            self.index = index
            self.idl_type = idl_type
            self.default_value = default_value

        def make_copy(self):
            return Argument.IR(
                identifier=self.identifier,
                index=self.index,
                idl_type=self.idl_type,
                default_value=self.default_value,
                extended_attributes=self.extended_attributes.make_copy(),
                code_generator_info=self.code_generator_info.make_copy())

    @property
    def idl_type(self):
        """
        Returns type of this argument.
        @return IdlType
        """
        assert False, 'To be implemented'

    @property
    def is_optional(self):
        """
        Returns True if this argument is optional.
        @return bool
        """
        assert False, 'To be implemented'

    @property
    def is_variadic(self):
        """
        Returns True if this argument is variadic.
        @return bool
        """
        assert False, 'To be implemented'

    @property
    def default_value(self):
        """
        Returns the default value if it is specified. Otherwise, None
        @return DefaultValue
        """
        assert False, 'To be implemented'

    @property
    def index(self):
        """
        Returns its index in an operation's arguments
        @return int
        """
        assert False, 'To be implemented'
