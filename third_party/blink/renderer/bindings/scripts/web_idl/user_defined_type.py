# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .common import WithIdentifier


class UserDefinedType(WithIdentifier):
    """
    UserDefinedType is a common base class of spec-author-defined types.

    Spec-author-defined types are top-level IDL definitions given an unique
    name.
    """

    def __init__(self, identifier):
        WithIdentifier.__init__(self, identifier)

    @property
    def is_interface(self):
        """
        Returns True if |self| represents an Interface.
        @return bool
        """
        return False

    @property
    def is_dictionary(self):
        """
        Returns True if |self| represents a Dictionary.
        @return bool
        """
        return False

    @property
    def is_callback_function(self):
        """
        Returns True if |self| represents a CallbackFunction.
        @return bool
        """
        return False

    @property
    def is_callback_interface(self):
        """
        Returns True if |self| represents a CallbackInterface.
        @return bool
        """
        return False

    @property
    def is_enumeration(self):
        """
        Returns True if |self| represents an Enumeration.
        @return bool
        """
        return False
