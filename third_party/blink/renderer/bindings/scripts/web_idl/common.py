# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy

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
        assert (extended_attributes is None
                or isinstance(extended_attributes, ExtendedAttributes))
        self._extended_attributes = extended_attributes or ExtendedAttributes()

    @property
    def extended_attributes(self):
        """
        Returns the extended attributes.
        @return ExtendedAttributes
        """
        return self._extended_attributes


class CodeGeneratorInfo(dict):
    def make_copy(self):
        return copy.deepcopy(self)


class WithCodeGeneratorInfo(object):
    """WithCodeGeneratorInfo class is an interface that its inheritances can
    provide some information for code generators."""

    def __init__(self, code_generator_info=None):
        assert (code_generator_info is None
                or isinstance(code_generator_info, CodeGeneratorInfo))
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
    """
    Implements |components| which is a Blink-specific layering concept of
    components, such as 'core' and 'modules'.

    A single IDL definition such as 'interface' may consist from multiple IDL
    fragments like partial interfaces and mixins, which may exist across
    Blink components.  |components| is a list of Blink components of IDL
    fragments that are involved into this object.
    """

    def __init__(self, component=None, components=None):
        """
        Args:
            component:
            components: Either of |component| or |components| must be given.
        """
        assert component is None or isinstance(component, Component)
        assert components is None or (isinstance(components, (list, tuple))
                                      and all(
                                          isinstance(component, Component)
                                          for component in components))
        assert (component or components) and not (component and components)
        if components:
            self._components = list(components)
        else:
            self._components = [component]

    @property
    def components(self):
        """
        Returns a list of components' names where this definition is defined
        @return tuple(Component)
        """
        return tuple(self._components)

    def add_components(self, components):
        assert isinstance(components, (list, tuple)) and all(
            isinstance(component, Component) for component in components)
        for component in components:
            if component not in self.components:
                self._components.append(component)


class Location(object):
    def __init__(self, filepath=None, line_number=None, position=None):
        assert filepath is None or isinstance(filepath, str)
        assert line_number is None or isinstance(line_number, int)
        assert position is None or isinstance(position, int)
        self._filepath = filepath
        self._line_number = line_number
        self._position = position  # Position number in a file

    def __str__(self):
        text = '{}'.format(self._filepath or '<<unknown path>>')
        if self._line_number:
            text += ':{}'.format(self._line_number)
        return text

    def make_copy(self):
        return Location(
            filepath=self._filepath,
            line_number=self._line_number,
            position=self._position)

    @property
    def filepath(self):
        return self._filepath

    @property
    def line_number(self):
        return self._line_number

    @property
    def position_in_file(self):
        return self._position


class DebugInfo(object):
    """Provides information useful for debugging."""

    def __init__(self, location=None, locations=None):
        assert location is None or isinstance(location, Location)
        assert locations is None or (isinstance(locations, (list, tuple))
                                     and all(
                                         isinstance(location, Location)
                                         for location in locations))
        assert not (location and locations)
        # The first entry is the primary location, e.g. location of non-partial
        # interface.  The rest is secondary locations, e.g. location of partial
        # interfaces and mixins.
        if locations:
            self._locations = locations
        else:
            self._locations = [location or Location()]

    def make_copy(self):
        return DebugInfo(locations=map(Location.make_copy, self._locations))

    @property
    def location(self):
        """
        Returns the primary location, i.e. location of the main definition.
        """
        return self._locations[0]

    @property
    def all_locations(self):
        """
        Returns a list of locations of all related IDL definitions, including
        partial definitions and mixins.
        """
        return tuple(self._locations)

    def add_locations(self, locations):
        assert isinstance(locations, (list, tuple)) and all(
            isinstance(location, Location) for location in locations)
        self._locations.extend(locations)


class WithDebugInfo(object):
    """WithDebugInfo class is an interface that its inheritances can have
    DebugInfo."""

    def __init__(self, debug_info=None):
        assert debug_info is None or isinstance(debug_info, DebugInfo)
        self._debug_info = debug_info or DebugInfo()

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
        assert isinstance(owner, object)  # None is okay
        self._owner = owner

    @property
    def owner(self):
        """
        Returns the owner of this instance.
        @return object
        """
        return self._owner
