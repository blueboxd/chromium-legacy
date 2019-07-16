# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .common import WithCodeGeneratorInfo
from .common import WithComponent
from .common import WithDebugInfo
from .common import WithExposure
from .common import WithExtendedAttributes
from .common import WithIdentifier
from .common import WithOwner


class IdlMember(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo,
                WithExposure, WithOwner, WithComponent, WithDebugInfo):
    """IdlMember provides common APIs for IDL members; attributes, operations,
    constants, dictionary members, etc."""
