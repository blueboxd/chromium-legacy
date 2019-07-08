# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions

from .extended_attribute import ExtendedAttributes
from .exposure import Exposure


Identifier = str


class WithIdentifier(object):
    """WithIdentifier class is an interface that indicates the class has an
    identifier."""

    def __init__(self, identifier):
        assert isinstance(identifier, Identifier)
        self._identifier = identifier

    @property
    def identifier(self):
        """
        Returns the identifier.
        @return Identifier
        """
        return self._identifier


# ExtendedAttribute and ExtendedAttributes are defined in extended_attribute.py
class WithExtendedAttributes(object):
    """WithExtendedAttributes class is an interface that indicates the implemented
    class can have extended attributes."""

    def __init__(self, extended_attributes=None):
        self._extended_attributes = extended_attributes or ExtendedAttributes()

    @property
    def extended_attributes(self):
        """
        Returns the extended attributes.
        @return ExtendedAttributes
        """
        return self._extended_attributes


CodeGeneratorInfo = dict


class WithCodeGeneratorInfo(object):
    """WithCodeGeneratorInfo class is an interface that its inheritances can
    provide some information for code generators."""

    def __init__(self, code_generator_info=None):
        self._code_generator_info = code_generator_info or CodeGeneratorInfo()

    @property
    def code_generator_info(self):
        """
        Returns information for code generator.
        @return CodeGeneratorInfo
        """
        return self._code_generator_info


class WithExposure(object):
    """WithExposure class is an interface that its inheritances can have Exposed
    extended attributes."""

    def __init__(self, exposures=None):
        assert (exposures is None
                or (isinstance(exposures, (list, tuple)) and all(
                    isinstance(exposure, Exposure) for exposure in exposures)))
        self._exposures = tuple(exposures or ())

    @property
    def exposures(self):
        """
        Returns a set of Exposure's that are applicable on an object.
        https://heycam.github.io/webidl/#Exposed
        @return tuple(Expsure)
        """
        return self._exposures


Component = str


class WithComponent(object):
    """WithComponent class is an interface to show which components this
    object belongs to."""

    # The order of |_COMPONENTS| shows the order of their dependencies.
    # DO NOT change the order.
    _COMPONENTS = (
        'core',
        'modules',
    )

    def __init__(self, component):
        self._components = [component]

    @property
    def components(self):
        """
        Returns a list of components' names where this definition is defined
        @return tuple(Component)
        """
        return self._components


class DebugInfo(object):
    """Provides information useful for debugging."""

    class Location(object):
        def __init__(self, filepath, line_number=None):
            self._filepath = filepath
            self._line_number = line_number

        def __str__(self):
            location = self.filepath
            if self.line_number is not None:
                location += '({})'.format(self.line_number)
            return location

        @property
        def filepath(self):
            return self._filepath

        @property
        def line_number(self):
            return self._line_number

    def __init__(self, filepath, line_number=None):
        self._locations = [DebugInfo.Location(filepath, line_number)]

    @property
    def filepaths(self):
        """
        Returns a list of filepaths where this IDL definition comes from.
        @return tuple(FilePath)
        """
        return tuple(location.filepath for location in self.locations)

    @property
    def locations(self):
        """
        Returns a list of locations where this IDL object comes from.
        @return tuple((FilePath, int))
        """
        return self._locations


class WithDebugInfo(object):
    """WithDebugInfo class is an interface that its inheritances can have DebugInfo."""

    def __init__(self, debug_info=None):
        self._debug_info = debug_info or DebugInfo(filepath='<<unspecified>>')

    @property
    def debug_info(self):
        """Returns DebugInfo."""
        return self._debug_info


class WithOwner(object):
    """WithOwner class provides information about who owns this object.
    If this object is a member, it points either of an interface,
    a namespace, a dictionary, and so on. If this object is an argument,
    it points a function like object."""

    def __init__(self, owner):
        self._owner = owner

    @property
    def owner(self):
        """
        Returns the owner of this instance.
        @return object
        """
        return self._owner
