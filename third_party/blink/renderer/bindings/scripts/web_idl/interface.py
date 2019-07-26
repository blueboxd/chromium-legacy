# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions

from .attribute import Attribute
from .common import WithCodeGeneratorInfo
from .common import WithComponent
from .common import WithDebugInfo
from .common import WithExposure
from .common import WithExtendedAttributes
from .constant import Constant
from .identifier_ir_map import IdentifierIRMap
from .idl_member import IdlMember
from .idl_type import IdlType
from .operation import Operation
from .reference import RefById
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
                     inherited=None,
                     attributes=None,
                     constants=None,
                     operations=None,
                     iterable=None,
                     maplike=None,
                     setlike=None,
                     extended_attributes=None,
                     exposures=None,
                     code_generator_info=None,
                     component=None,
                     components=None,
                     debug_info=None):
            assert isinstance(is_partial, bool)
            assert isinstance(is_mixin, bool)
            assert inherited is None or isinstance(inherited, RefById)
            assert attributes is None or isinstance(attributes, (list, tuple))
            assert constants is None or isinstance(constants, (list, tuple))
            assert operations is None or isinstance(operations, (list, tuple))
            assert iterable is None or isinstance(iterable, Iterable)
            assert maplike is None or isinstance(maplike, Maplike)
            assert setlike is None or isinstance(setlike, Setlike)

            attributes = attributes or []
            constants = constants or []
            operations = operations or []
            assert all(
                isinstance(attribute, Attribute.IR)
                for attribute in attributes)
            assert all(
                isinstance(constant, Constant.IR) for constant in constants)
            assert all(
                isinstance(operation, Operation.IR)
                for operation in operations)

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
            WithComponent.__init__(
                self, component=component, components=components)
            WithDebugInfo.__init__(self, debug_info)

            self.is_partial = is_partial
            self.is_mixin = is_mixin
            self.inherited = inherited
            self.attributes = list(attributes)
            self.constants = list(constants)
            self.operations = list(operations)
            self.iterable = iterable
            self.maplike = maplike
            self.setlike = setlike

        def make_copy(self):
            return Interface.IR(
                identifier=self.identifier,
                is_partial=self.is_partial,
                is_mixin=self.is_mixin,
                inherited=self.inherited,
                attributes=map(Attribute.IR.make_copy, self.attributes),
                constants=map(Constant.IR.make_copy, self.constants),
                operations=map(Operation.IR.make_copy, self.operations),
                iterable=self.iterable,
                maplike=self.maplike,
                setlike=self.setlike,
                extended_attributes=self.extended_attributes.make_copy(),
                code_generator_info=self.code_generator_info.make_copy(),
                components=self.components,
                debug_info=self.debug_info.make_copy())


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


class Iterable(WithCodeGeneratorInfo, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-iterable"""

    def __init__(self,
                 key_type=None,
                 value_type=None,
                 code_generator_info=None,
                 debug_info=None):
        assert key_type is None or isinstance(key_type, IdlType)
        # iterable is declared in either form of
        #     iterable<value_type>
        #     iterable<key_type, value_type>
        # thus |value_type| can't be None.  However, we put it after |key_type|
        # to be consistent with the format of IDL.
        assert isinstance(value_type, IdlType), "value_type must be specified"

        WithCodeGeneratorInfo.__init__(self, code_generator_info)
        WithDebugInfo.__init__(self, debug_info)

        self._key_type = key_type
        self._value_type = value_type

    @property
    def key_type(self):
        """
        Returns the key type or None.
        @return IdlType?
        """
        return self._key_type

    @property
    def value_type(self):
        """
        Returns the value type.
        @return IdlType
        """
        return self._value_type


class Maplike(WithCodeGeneratorInfo, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-maplike"""

    def __init__(self,
                 key_type,
                 value_type,
                 is_readonly=False,
                 code_generator_info=None,
                 debug_info=None):
        assert isinstance(key_type, IdlType)
        assert isinstance(value_type, IdlType)
        assert isinstance(is_readonly, bool)

        WithCodeGeneratorInfo.__init__(self, code_generator_info)
        WithDebugInfo.__init__(self, debug_info)

        self._key_type = key_type
        self._value_type = value_type
        self._is_readonly = is_readonly

    @property
    def key_type(self):
        """
        Returns its key type.
        @return IdlType
        """
        return self._key_type

    @property
    def value_type(self):
        """
        Returns its value type.
        @return IdlType
        """
        return self._value_type

    @property
    def is_readonly(self):
        """
        Returns True if it's readonly.
        @return bool
        """
        return self._is_readonly


class Setlike(WithCodeGeneratorInfo, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-setlike"""

    def __init__(self,
                 value_type,
                 is_readonly=False,
                 code_generator_info=None,
                 debug_info=None):
        assert isinstance(value_type, IdlType)
        assert isinstance(is_readonly, bool)

        WithCodeGeneratorInfo.__init__(self, code_generator_info)
        WithDebugInfo.__init__(self, debug_info)

        self._value_type = value_type
        self._is_readonly = is_readonly

    @property
    def value_type(self):
        """
        Returns its value type.
        @return IdlType
        """
        return self._value_type

    @property
    def is_readonly(self):
        """
        Returns True if it's readonly.
        @return bool
        """
        return self._is_readonly


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
