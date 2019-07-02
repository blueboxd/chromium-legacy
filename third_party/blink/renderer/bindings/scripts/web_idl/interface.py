# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .common import WithCodeGeneratorInfo
from .common import WithComponent
from .common import WithDebugInfo
from .common import WithExposure
from .common import WithExtendedAttributes
from .identifier_ir_map import IdentifierIRMap
from .idl_member import IdlMember
from .user_defined_type import UserDefinedType


class Interface(UserDefinedType, WithExtendedAttributes, WithExposure,
                WithCodeGeneratorInfo, WithComponent, WithDebugInfo):
    """A summarized interface definition in IDL.

    Interface provides information about an interface, partial interfaces,
    interface mixins, and partial interface mixins, as if they were all
    gathered in an interface.
    https://heycam.github.io/webidl/#idl-interfaces
    """

    class IR(IdentifierIRMap.IR, WithExtendedAttributes, WithExposure,
             WithCodeGeneratorInfo, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     is_partial,
                     is_mixin,
                     extended_attributes=None,
                     exposures=None,
                     code_generator_info=None,
                     component=None,
                     debug_info=None):
            kind = None
            if is_partial:
                if is_mixin:
                    kind = IdentifierIRMap.IR.Kind.PARTIAL_INTERFACE_MIXIN
                else:
                    kind = IdentifierIRMap.IR.Kind.PARTIAL_INTERFACE
            else:
                if is_mixin:
                    kind = IdentifierIRMap.IR.Kind.INTERFACE_MIXIN
                else:
                    kind = IdentifierIRMap.IR.Kind.INTERFACE
            IdentifierIRMap.IR.__init__(self, identifier=identifier, kind=kind)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithExposure.__init__(self, exposures)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

    @property
    def inherited_interface(self):
        """
        Returns an Interface from which this interface inherits. If this
        interface does not inherit, returns None.
        @return Interface?
        """
        raise exceptions.NotImplementedError()

    @property
    def attributes(self):
        """
        Returns a tuple of attributes including [Unforgeable] attributes in
        ancestors.
        @return tuple(Attribute)
        """
        raise exceptions.NotImplementedError()

    @property
    def operation_groups(self):
        """
        Returns a tuple of OperationGroup. Each OperationGroup has operation(s)
        defined in this interface and [Unforgeable] operations in ancestors.
        @return tuple(OperationGroup)
        """
        raise exceptions.NotImplementedError()

    @property
    def constants(self):
        """
        Returns a tuple of constants defined in this interface.
        @return tuple(Constant)
        """
        raise exceptions.NotImplementedError()

    @property
    def constructors(self):
        """
        Returns ConstructorGroup instance for this interface.
        @return tuple(ConstructorGroup)
        """
        raise exceptions.NotImplementedError()

    @property
    def named_constructor(self):
        """
        Returns a named constructor, if this interface has it. Otherwise, returns
        None.
        @return NamedConstructor?
        """
        raise exceptions.NotImplementedError()

    @property
    def exposed_interfaces(self):
        """
        Returns a tuple of Interfaces that are exposed to |self|. If |self| is
        not a global interface, returns an empty tuple.
        @return tuple(Interface)
        """
        raise exceptions.NotImplementedError()

    # Special operations
    @property
    def indexed_property_handler(self):
        """
        Returns a set of handlers (getter/setter/deleter) for the indexed
        property.
        @return IndexedPropertyHandler?
        """
        # TODO: Include anonymous handlers of ancestors. https://crbug.com/695972
        raise exceptions.NotImplementedError()

    @property
    def named_property_handler(self):
        """
        Returns a set of handlers (getter/setter/deleter) for the named
        property.
        @return NamedPropertyHandler?
        """
        # TODO: Include anonymous handlers of ancestors. https://crbug.com/695972
        raise exceptions.NotImplementedError()

    @property
    def stringifier(self):
        """
        Returns stringifier if it is defined. Returns None otherwise.
        @return TBD?
        """
        raise exceptions.NotImplementedError()

    @property
    def iterable(self):
        """
        Returns iterable if it is defined. Returns None otherwise.
        @return Iterable?
        """
        raise exceptions.NotImplementedError()

    @property
    def maplike(self):
        """
        Returns maplike if it is defined. Returns None otherwise.
        @return Maplike?
        """
        raise exceptions.NotImplementedError()

    @property
    def setlike(self):
        """
        Returns setlike if it is defined. Returns None otherwise.
        @return Setlike?
        """
        raise exceptions.NotImplementedError()

    # UserDefinedType overrides
    @property
    def is_interface(self):
        return True


class Iterable(IdlMember):
    """https://heycam.github.io/webidl/#idl-iterable"""

    @property
    def key_type(self):
        """
        Returns its key type or None.
        @return IdlType?
        """
        raise exceptions.NotImplementedError()

    @property
    def value_type(self):
        """
        Returns its value type.
        @return IdlType
        """
        raise exceptions.NotImplementedError()


class Maplike(object):
    """https://heycam.github.io/webidl/#idl-maplike"""

    @property
    def key_type(self):
        """
        Returns its key type.
        @return IdlType
        """
        raise exceptions.NotImplementedError()

    @property
    def value_type(self):
        """
        Returns its value type.
        @return IdlType
        """
        raise exceptions.NotImplementedError()

    @property
    def is_readonly(self):
        """
        Returns True if it's readonly.
        @return bool
        """
        raise exceptions.NotImplementedError()


class Setlike(object):
    """https://heycam.github.io/webidl/#idl-setlike"""

    @property
    def value_type(self):
        """
        Returns its value type.
        @return IdlType
        """
        raise exceptions.NotImplementedError()

    @property
    def is_readonly(self):
        """
        Returns True if it's readonly.
        @return bool
        """
        raise exceptions.NotImplementedError()


class IndexedPropertyHandler(IdlMember):
    @property
    def getter(self):
        """
        Returns an Operation for indexed property getter.
        @return Operation?
        """
        raise exceptions.NotImplementedError()

    @property
    def setter(self):
        """
        Returns an Operation for indexed property setter.
        @return Operation?
        """
        raise exceptions.NotImplementedError()

    @property
    def deleter(self):
        """
        Returns an Operation for indexed property deleter.
        @return Operation?
        """
        raise exceptions.NotImplementedError()


class NamedPropertyHandler(IdlMember):
    @property
    def getter(self):
        """
        Returns an Operation for named property getter.
        @return Operation?
        """
        raise exceptions.NotImplementedError()

    @property
    def setter(self):
        """
        Returns an Operation for named property setter.
        @return Operation?
        """
        raise exceptions.NotImplementedError()

    @property
    def deleter(self):
        """
        Returns an Operation for named property deleter.
        @return Operation?
        """
        raise exceptions.NotImplementedError()
