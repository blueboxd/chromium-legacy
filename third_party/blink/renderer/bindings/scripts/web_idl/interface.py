# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .attribute import Attribute
from .code_generator_info import CodeGeneratorInfo
from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExposure
from .composition_parts import WithExtendedAttributes
from .composition_parts import WithIdentifier
from .composition_parts import WithOwner
from .constant import Constant
from .constructor import Constructor
from .constructor import ConstructorGroup
from .exposure import Exposure
from .idl_type import IdlType
from .ir_map import IRMap
from .make_copy import make_copy
from .operation import Operation
from .operation import OperationGroup
from .reference import RefById
from .user_defined_type import UserDefinedType


class Interface(UserDefinedType, WithExtendedAttributes, WithCodeGeneratorInfo,
                WithExposure, WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-interfaces"""

    class IR(IRMap.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithExposure, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     is_partial,
                     is_mixin,
                     inherited=None,
                     attributes=None,
                     constants=None,
                     constructors=None,
                     named_constructors=None,
                     operations=None,
                     indexed_and_named_properties=None,
                     stringifier=None,
                     iterable=None,
                     maplike=None,
                     setlike=None,
                     extended_attributes=None,
                     component=None,
                     debug_info=None):
            assert isinstance(is_partial, bool)
            assert isinstance(is_mixin, bool)
            assert inherited is None or isinstance(inherited, RefById)
            assert attributes is None or isinstance(attributes, (list, tuple))
            assert constants is None or isinstance(constants, (list, tuple))
            assert constructors is None or isinstance(constructors,
                                                      (list, tuple))
            assert named_constructors is None or isinstance(
                named_constructors, (list, tuple))
            assert operations is None or isinstance(operations, (list, tuple))
            assert indexed_and_named_properties is None or isinstance(
                indexed_and_named_properties, IndexedAndNamedProperties.IR)
            assert stringifier is None or isinstance(stringifier,
                                                     Stringifier.IR)
            assert iterable is None or isinstance(iterable, Iterable)
            assert maplike is None or isinstance(maplike, Maplike)
            assert setlike is None or isinstance(setlike, Setlike)

            attributes = attributes or []
            constants = constants or []
            constructors = constructors or []
            named_constructors = named_constructors or []
            operations = operations or []
            assert all(
                isinstance(attribute, Attribute.IR)
                for attribute in attributes)
            assert all(
                isinstance(constant, Constant.IR) for constant in constants)
            assert all(
                isinstance(constructor, Constructor.IR)
                for constructor in constructors)
            assert all(
                isinstance(named_constructor, Constructor.IR)
                for named_constructor in named_constructors)
            assert all(
                isinstance(operation, Operation.IR)
                for operation in operations)

            kind = None
            if is_partial:
                if is_mixin:
                    kind = IRMap.IR.Kind.PARTIAL_INTERFACE_MIXIN
                else:
                    kind = IRMap.IR.Kind.PARTIAL_INTERFACE
            else:
                if is_mixin:
                    kind = IRMap.IR.Kind.INTERFACE_MIXIN
                else:
                    kind = IRMap.IR.Kind.INTERFACE
            IRMap.IR.__init__(self, identifier=identifier, kind=kind)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self)
            WithExposure.__init__(self)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

            self.is_partial = is_partial
            self.is_mixin = is_mixin
            self.inherited = inherited
            self.attributes = list(attributes)
            self.constants = list(constants)
            self.constructors = list(constructors)
            self.constructor_groups = []
            self.named_constructors = list(named_constructors)
            self.named_constructor_groups = []
            self.operations = list(operations)
            self.operation_groups = []
            self.exposed_constructs = []
            self.legacy_window_aliases = []
            self.indexed_and_named_properties = indexed_and_named_properties
            self.stringifier = stringifier
            self.iterable = iterable
            self.maplike = maplike
            self.setlike = setlike

        def iter_all_members(self):
            for attribute in self.attributes:
                yield attribute
            for constant in self.constants:
                yield constant
            for constructor in self.constructors:
                yield constructor
            for named_constructor in self.named_constructors:
                yield named_constructor
            for operation in self.operations:
                yield operation

    def __init__(self, ir):
        assert isinstance(ir, Interface.IR)
        assert not ir.is_partial

        ir = make_copy(ir)
        UserDefinedType.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self, ir, readonly=True)
        WithCodeGeneratorInfo.__init__(self, ir, readonly=True)
        WithExposure.__init__(self, ir, readonly=True)
        WithComponent.__init__(self, ir, readonly=True)
        WithDebugInfo.__init__(self, ir)

        self._is_mixin = ir.is_mixin
        self._inherited = ir.inherited
        self._attributes = tuple([
            Attribute(attribute_ir, owner=self)
            for attribute_ir in ir.attributes
        ])
        self._constants = tuple([
            Constant(constant_ir, owner=self) for constant_ir in ir.constants
        ])
        self._constructors = tuple([
            Constructor(constructor_ir, owner=self)
            for constructor_ir in ir.constructors
        ])
        self._constructor_groups = tuple([
            ConstructorGroup(
                group_ir,
                filter(lambda x: x.identifier == group_ir.identifier,
                       self._constructors),
                owner=self) for group_ir in ir.constructor_groups
        ])
        assert len(self._constructor_groups) <= 1
        self._named_constructors = tuple([
            Constructor(named_constructor_ir, owner=self)
            for named_constructor_ir in ir.named_constructors
        ])
        self._named_constructor_groups = tuple([
            ConstructorGroup(
                group_ir,
                filter(lambda x: x.identifier == group_ir.identifier,
                       self._named_constructors),
                owner=self) for group_ir in ir.named_constructor_groups
        ])
        self._operations = tuple([
            Operation(operation_ir, owner=self)
            for operation_ir in ir.operations
        ])
        self._operation_groups = tuple([
            OperationGroup(
                group_ir,
                filter(lambda x: x.identifier == group_ir.identifier,
                       self._operations),
                owner=self) for group_ir in ir.operation_groups
        ])
        self._exposed_constructs = tuple(ir.exposed_constructs)
        self._legacy_window_aliases = tuple(ir.legacy_window_aliases)
        self._indexed_and_named_properties = None
        if ir.indexed_and_named_properties:
            operations = filter(
                lambda x: x.is_indexed_or_named_property_operation,
                self._operations)
            self._indexed_and_named_properties = IndexedAndNamedProperties(
                ir.indexed_and_named_properties, operations, owner=self)
        self._stringifier = None
        if ir.stringifier:
            operations = filter(lambda x: x.is_stringifier, self._operations)
            assert len(operations) == 1
            attributes = [None]
            if ir.stringifier.attribute:
                attr_id = ir.stringifier.attribute.identifier
                attributes = filter(lambda x: x.identifier == attr_id,
                                    self._attributes)
            assert len(attributes) == 1
            self._stringifier = Stringifier(
                ir.stringifier,
                operation=operations[0],
                attribute=attributes[0],
                owner=self)
        self._iterable = ir.iterable
        self._maplike = ir.maplike
        self._setlike = ir.setlike

    @property
    def is_mixin(self):
        """Returns True if this is a mixin interface."""
        return self._is_mixin

    @property
    def inherited(self):
        """Returns the inherited interface or None."""
        return self._inherited.target_object if self._inherited else None

    @property
    def inclusive_inherited_interfaces(self):
        """
        Returns the list of inclusive inherited interfaces.

        https://heycam.github.io/webidl/#interface-inclusive-inherited-interfaces
        """
        result = []
        interface = self
        while interface is not None:
            result.append(interface)
            interface = interface.inherited
        return result

    def does_implement(self, identifier):
        """
        Returns True if this is or inherits from the given interface.
        """
        assert isinstance(identifier, str)

        for interface in self.inclusive_inherited_interfaces:
            if interface.identifier == identifier:
                return True
        return False

    @property
    def attributes(self):
        """
        Returns attributes, including [Unforgeable] attributes in ancestors.
        """
        return self._attributes

    @property
    def constants(self):
        """Returns constants."""
        return self._constants

    @property
    def constructors(self):
        """Returns constructors."""
        return self._constructors

    @property
    def constructor_groups(self):
        """
        Returns groups of constructors.

        Constructors are grouped as operations are. There is 0 or 1 group.
        """
        return self._constructor_groups

    @property
    def named_constructors(self):
        """Returns named constructors."""
        return self._named_constructors

    @property
    def named_constructor_groups(self):
        """Returns groups of overloaded named constructors."""
        return self._named_constructor_groups

    @property
    def operations(self):
        """
        Returns all operations, including special operations without an
        identifier, as well as [Unforgeable] operations in ancestors.
        """
        return self._operations

    @property
    def operation_groups(self):
        """
        Returns groups of overloaded operations, including [Unforgeable]
        operations in ancestors.

        All operations that have an identifier are grouped by identifier, thus
        it's possible that there is a single operation in a certain operation
        group.  If an operation doesn't have an identifier, i.e. if it's a
        merely special operation, then the operation doesn't appear in any
        operation group.
        """
        return self._operation_groups

    @property
    def exposed_constructs(self):
        """
        Returns a list of the constructs that are exposed on this global object.
        """
        return tuple(
            map(lambda ref: ref.target_object, self._exposed_constructs))

    @property
    def legacy_window_aliases(self):
        """Returns a list of properties exposed as [LegacyWindowAlias]."""
        return self._legacy_window_aliases

    @property
    def indexed_and_named_properties(self):
        """Returns a IndexedAndNamedProperties or None."""
        return self._indexed_and_named_properties

    @property
    def stringifier(self):
        """Returns a Stringifier or None."""
        return self._stringifier

    @property
    def iterable(self):
        """Returns an Iterable or None."""
        return self._iterable

    @property
    def maplike(self):
        """Returns a Maplike or None."""
        return self._maplike

    @property
    def setlike(self):
        """Returns a Setlike or None."""
        return self._setlike

    # UserDefinedType overrides
    @property
    def is_interface(self):
        return True


class LegacyWindowAlias(WithIdentifier, WithExtendedAttributes, WithExposure):
    """
    Represents a property exposed on a Window object as [LegacyWindowAlias].
    """

    def __init__(self, identifier, original, extended_attributes, exposure):
        assert isinstance(original, RefById)

        WithIdentifier.__init__(self, identifier)
        WithExtendedAttributes.__init__(
            self, extended_attributes, readonly=True)
        WithExposure.__init__(self, exposure, readonly=True)

        self._original = original

    @property
    def original(self):
        """Returns the original object of this alias."""
        return self._original.target_object


class IndexedAndNamedProperties(WithOwner, WithDebugInfo):
    """
    Represents a set of indexed/named getter/setter/deleter.

    https://heycam.github.io/webidl/#idl-indexed-properties
    https://heycam.github.io/webidl/#idl-named-properties
    """

    class IR(WithDebugInfo):
        def __init__(self, operations, debug_info=None):
            assert isinstance(operations, (list, tuple))
            assert all(
                isinstance(operation, Operation.IR)
                for operation in operations)

            WithDebugInfo.__init__(self, debug_info)

            self.indexed_getter = None
            self.indexed_setter = None
            self.named_getter = None
            self.named_setter = None
            self.named_deleter = None

            for operation in operations:
                arg1_type = operation.arguments[0].idl_type
                if arg1_type.is_integer:
                    if operation.is_getter:
                        assert self.indexed_getter is None
                        self.indexed_getter = operation
                    elif operation.is_setter:
                        assert self.indexed_setter is None
                        self.indexed_setter = operation
                    else:
                        assert False
                elif arg1_type.is_string:
                    if operation.is_getter:
                        assert self.named_getter is None
                        self.named_getter = operation
                    elif operation.is_setter:
                        assert self.named_setter is None
                        self.named_setter = operation
                    elif operation.is_deleter:
                        assert self.named_deleter is None
                        self.named_deleter = operation
                    else:
                        assert False
                else:
                    assert False

    def __init__(self, ir, operations, owner):
        assert isinstance(ir, IndexedAndNamedProperties.IR)
        assert isinstance(operations, (list, tuple))
        assert all(
            isinstance(operation, Operation) for operation in operations)

        WithOwner.__init__(self, owner)
        WithDebugInfo.__init__(self, ir)

        self._own_indexed_getter = None
        self._own_indexed_setter = None
        self._own_named_getter = None
        self._own_named_setter = None
        self._own_named_deleter = None

        for operation in operations:
            arg1_type = operation.arguments[0].idl_type
            if arg1_type.is_integer:
                if operation.is_getter:
                    assert self._own_indexed_getter is None
                    self._own_indexed_getter = operation
                elif operation.is_setter:
                    assert self._own_indexed_setter is None
                    self._own_indexed_setter = operation
                else:
                    assert False
            elif arg1_type.is_string:
                if operation.is_getter:
                    assert self._own_named_getter is None
                    self._own_named_getter = operation
                elif operation.is_setter:
                    assert self._own_named_setter is None
                    self._own_named_setter = operation
                elif operation.is_deleter:
                    assert self._own_named_deleter is None
                    self._own_named_deleter = operation
                else:
                    assert False
            else:
                assert False

    @property
    def has_indexed_properties(self):
        return self.indexed_getter or self.indexed_setter

    @property
    def has_named_properties(self):
        return self.named_getter or self.named_setter or self.named_deleter

    @property
    def is_named_property_enumerable(self):
        named_getter = self.named_getter
        return bool(named_getter
                    and 'NotEnumerable' not in named_getter.extended_attributes
                    and 'LegacyUnenumerableNamedProperties' not in self.owner.
                    extended_attributes)

    @property
    def indexed_getter(self):
        return self._find_accessor('own_indexed_getter')

    @property
    def indexed_setter(self):
        return self._find_accessor('own_indexed_setter')

    @property
    def named_getter(self):
        return self._find_accessor('own_named_getter')

    @property
    def named_setter(self):
        return self._find_accessor('own_named_setter')

    @property
    def named_deleter(self):
        return self._find_accessor('own_named_deleter')

    @property
    def own_indexed_getter(self):
        return self._own_indexed_getter

    @property
    def own_indexed_setter(self):
        return self._own_indexed_setter

    @property
    def own_named_getter(self):
        return self._own_named_getter

    @property
    def own_named_setter(self):
        return self._own_named_setter

    @property
    def own_named_deleter(self):
        return self._own_named_deleter

    def _find_accessor(self, attr):
        for interface in self.owner.inclusive_inherited_interfaces:
            props = interface.indexed_and_named_properties
            if props:
                accessor = getattr(props, attr)
                if accessor:
                    return accessor
        return None


class Stringifier(WithOwner, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-stringifiers"""

    class IR(WithDebugInfo):
        def __init__(self, operation=None, attribute=None, debug_info=None):
            assert isinstance(operation, Operation.IR)
            assert attribute is None or isinstance(attribute, Attribute.IR)

            WithDebugInfo.__init__(self, debug_info)

            self.operation = operation
            self.attribute = attribute

    def __init__(self, ir, operation, attribute, owner):
        assert isinstance(ir, Stringifier.IR)
        assert isinstance(operation, Operation)
        assert attribute is None or isinstance(attribute, Attribute)

        WithOwner.__init__(self, owner)
        WithDebugInfo.__init__(self, ir)

        self._operation = operation
        self._attribute = attribute

    @property
    def operation(self):
        return self._operation

    @property
    def attribute(self):
        return self._attribute


class Iterable(WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-iterable"""

    def __init__(self, key_type=None, value_type=None, debug_info=None):
        assert key_type is None or isinstance(key_type, IdlType)
        # iterable is declared in either form of
        #     iterable<value_type>
        #     iterable<key_type, value_type>
        # thus |value_type| can't be None.  However, we put it after |key_type|
        # to be consistent with the format of IDL.
        assert isinstance(value_type, IdlType), "value_type must be specified"

        WithDebugInfo.__init__(self, debug_info)

        self._key_type = key_type
        self._value_type = value_type

    @property
    def key_type(self):
        """Returns the key type or None."""
        return self._key_type

    @property
    def value_type(self):
        """Returns the value type."""
        return self._value_type


class Maplike(WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-maplike"""

    def __init__(self,
                 key_type,
                 value_type,
                 is_readonly=False,
                 debug_info=None):
        assert isinstance(key_type, IdlType)
        assert isinstance(value_type, IdlType)
        assert isinstance(is_readonly, bool)

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


class Setlike(WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-setlike"""

    def __init__(self, value_type, is_readonly=False, debug_info=None):
        assert isinstance(value_type, IdlType)
        assert isinstance(is_readonly, bool)

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
