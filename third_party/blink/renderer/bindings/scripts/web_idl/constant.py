# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .common import WithCodeGeneratorInfo
from .common import WithComponent
from .common import WithDebugInfo
from .common import WithExtendedAttributes
from .common import WithExposure
from .common import WithIdentifier
from .idl_member import IdlMember
from .idl_type import IdlType
from .values import ConstantValue


class Constant(IdlMember):
    """https://heycam.github.io/webidl/#idl-constants"""

    class IR(WithIdentifier, WithExtendedAttributes, WithExposure,
             WithCodeGeneratorInfo, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     value,
                     idl_type,
                     extended_attributes=None,
                     exposures=None,
                     code_generator_info=None,
                     component=None,
                     debug_info=None):
            assert isinstance(value, ConstantValue)
            assert isinstance(idl_type, IdlType)

            WithIdentifier.__init__(self, identifier)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithExposure.__init__(self, exposures)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

            self.value = value
            self.idl_type = idl_type

    @property
    def idl_type(self):
        """
        Returns the type of this constant.
        @return IdlType
        """
        assert False, 'To be implemented'

    @property
    def value(self):
        """
        Returns the constant value.
        @return ConstantValue
        """
        assert False, 'To be implemented'
