# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions

from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithOwner


class Constrcutor(object):
    def is_custom(self):
        """
        Returns True if this Constructor is defined in the form of
        [CustomConstructor=(...)]
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def return_type(self):
        """
        Returns IDL interface type to construct.
        @return IdlInterfaceType
        """
        raise exceptions.NotImplementedError()

    @property
    def arguments(self):
        """
        Returns a list of arguments
        @return tuple(Argument)
        """
        raise exceptions.NotImplementedError()

    @property
    def overloaded_index(self):
        """
        Returns its index in ConstructorGroup
        @return int
        """
        raise exceptions.NotImplementedError()


class ConstructorGroup(object):
    def constructors(self):
        """
        Returns a list of constructors
        @return tuple(Constructor)
        """
        raise exceptions.NotImplementedError()


class NamedConstructor(object):
    @property
    def return_type(self):
        """
        Returns IDL type to construct.
        @return IdlInterfaceType
        """
        raise exceptions.NotImplementedError()

    @property
    def name(self):
        """
        Returns the name to be visible as.
        @return Identifier
        """
        raise exceptions.NotImplementedError()

    @property
    def arguments(self):
        """
        Returns a list of arguments.
        @return tuple(Argument)
        """
        raise exceptions.NotImplementedError()
