# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExtendedAttributes
from .composition_parts import WithIdentifier
from .composition_parts import WithOwner


class IdlMember(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo,
                WithOwner, WithComponent, WithDebugInfo):
    """IdlMember provides common APIs for IDL members; attributes, operations,
    constants, dictionary members, etc."""
