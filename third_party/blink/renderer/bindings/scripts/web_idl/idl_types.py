# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from blinkbuild.name_style_converter import NameStyleConverter
from .common import WithCodeGeneratorInfo
from .common import WithDebugInfo
from .common import WithExtendedAttributes
from .common import WithIdentifier
from .idl_reference_proxy import RefByIdFactory
from .idl_reference_proxy import Proxy
from .user_defined_type import UserDefinedType

# This file defines following classes to provide IDL types.
#
# <Classes>
# IdlType
# + SimpleType
# + ReferenceType
# + DefinitionType
# + TypedefType
# + _ArrayLikeType
# | + SequenceType
# | + FrozenArrayType
# + RecordType
# + PromiseType
# + UnionType
# + NullableType


class IdlType(WithExtendedAttributes, WithCodeGeneratorInfo, WithDebugInfo):
    """
    Represents a 'type' in Web IDL

    IdlType is an interface of types in Web IDL, and also provides all the
    information that is necessary for type conversions.  For example, given the
    conversion rules of ECMAScript bindings, you can produce a type converter
    between Blink types and V8 types with using an IdlType.

    Note that IdlType is designed to _not_ include knowledge about a certain
    language bindings (such as ECMAScript bindings), thus it's out of scope for
    IdlType to tell whether IDL dictionary type accepts ES null value or not.

    Nullable type and typedef are implemented as if they're a container type
    like record type and promise type.
    """

    def __init__(self,
                 extended_attributes=None,
                 code_generator_info=None,
                 debug_info=None):
        WithExtendedAttributes.__init__(self, extended_attributes)
        WithCodeGeneratorInfo.__init__(self, code_generator_info)
        WithDebugInfo.__init__(self, debug_info)

    def __str__(self):
        return self.syntactic_form

    @property
    def syntactic_form(self):
        """
        Returns a text representation of the type in the form of Web IDL syntax.
        @return str
        """
        raise exceptions.NotImplementedError()

    @property
    def type_name(self):
        """
        Returns the type name.
        https://heycam.github.io/webidl/#dfn-type-name
        Note that a type name is not necessarily unique.
        @return str
        """
        raise exceptions.NotImplementedError()

    @property
    def does_include_nullable_type(self):
        """
        Returns True if |self| includes a nulllable type.
        https://heycam.github.io/webidl/#dfn-includes-a-nullable-type
        @return bool
        """
        return False

    @property
    def is_numeric(self):
        """
        Returns True if |self| is a number type.
        @return bool
        """
        return False

    @property
    def is_integer(self):
        """
        Returns True if |self| is an integer type.
        @return bool
        """
        return False

    @property
    def is_boolean(self):
        """
        Returns True if |self| is a boolean type.
        @return bool
        """
        return False

    @property
    def is_string(self):
        """
        Returns True if |self| is a string type.
        @return bool
        """
        return False

    @property
    def is_object(self):
        """
        Returns True if |self| is an object type.
        @return bool
        """
        return False

    @property
    def is_symbol(self):
        """
        Returns True if |self| is a symbol type.
        @return bool
        """
        return False

    @property
    def is_any(self):
        """
        Returns True if |self| is an any type.
        @return bool
        """
        return False

    @property
    def is_void(self):
        """
        Returns True if |self| is a void type.
        @return bool
        """
        return False

    @property
    def is_interface(self):
        """
        Returns True if |self| is an interface type.
        @return bool
        """
        return False

    @property
    def is_callback_interface(self):
        """
        Returns True if |self| is a callback interface type.
        @return bool
        """
        return False

    @property
    def is_dictionary(self):
        """
        Returns True if |self| is a dictionary type.
        @return bool
        """
        return False

    @property
    def is_enumeration(self):
        """
        Returns True if |self| is an enumeration type.
        @return bool
        """
        return False

    @property
    def is_callback_function(self):
        """
        Returns True if |self| is a callback function type.
        @return bool
        """
        return False

    @property
    def is_typedef(self):
        """
        Returns True if |self| is a typedef.
        @return bool
        """
        return False

    @property
    def is_nullable(self):
        """
        Returns True if |self| is a nullable type.

        NOTE: If |self| is a union type which includes a nullable type, this
        returns False, because |self| itself is not a nullable type.
        Use |does_include_nullable_type| in such a case.
        @return bool
        """
        return False

    @property
    def is_annotated(self):
        """
        Returns True if |self| is annotated.
        @return bool
        """
        return bool(self.extended_attributes)

    @property
    def is_promise(self):
        """
        Returns True if |self| is a promise type.
        @return bool
        """
        return False

    @property
    def is_record(self):
        """
        Returns True if |self| is a record type.
        @return bool
        """
        return False

    @property
    def is_sequence(self):
        """
        Returns True if |self| is a sequence type.
        @return bool
        """
        return False

    @property
    def is_frozen_array(self):
        """
        Returns True if |self| is a froen array type.
        @return bool
        """
        return False

    @property
    def is_union(self):
        """
        Returns True if |self| is a union type.
        @return bool
        """
        return False

    def _format_syntactic_form(self, syntactic_form_inner):
        """Helper function to implement |syntactic_form|."""
        if self.extended_attributes:
            return '{} {}'.format(self.extended_attributes,
                                  syntactic_form_inner)
        return syntactic_form_inner

    def _format_type_name(self, type_name_inner):
        """Helper function to implement |type_name|."""
        return '{}{}'.format(type_name_inner, ''.join(
            sorted(self.extended_attributes.keys())))


class SimpleType(IdlType):
    """
    Represents built-in types that do not contain other types internally.
    e.g. primitive types, string types, and object types.
    https://heycam.github.io/webidl/#idl-types
    """

    _INTEGER_TYPES = ('byte', 'octet', 'short', 'unsigned short', 'long',
                      'unsigned long', 'long long', 'unsigned long long')
    _NUMERIC_TYPES = ('float', 'unrestricted float', 'double',
                      'unrestricted double') + _INTEGER_TYPES
    _STRING_TYPES = ('DOMString', 'ByteString', 'USVString')
    _VALID_TYPES = ('any', 'boolean', 'object', 'symbol',
                    'void') + _NUMERIC_TYPES + _STRING_TYPES

    def __init__(self,
                 name,
                 extended_attributes=None,
                 code_generator_info=None,
                 debug_info=None):
        IdlType.__init__(
            self,
            extended_attributes=extended_attributes,
            code_generator_info=code_generator_info,
            debug_info=debug_info)
        assert name in SimpleType._VALID_TYPES, (
            'Unknown type name: {}'.format(name))
        self._name = name

    # IdlType overrides
    @property
    def syntactic_form(self):
        return self._format_syntactic_form(self._name)

    @property
    def type_name(self):
        name = 'String' if self._name == 'DOMString' else self._name
        return self._format_type_name(
            NameStyleConverter(name).to_upper_camel_case())

    @property
    def is_numeric(self):
        return self._name in SimpleType._NUMERIC_TYPES

    @property
    def is_integer(self):
        return self._name in SimpleType._INTEGER_TYPES

    @property
    def is_boolean(self):
        return self._name == 'boolean'

    @property
    def is_string(self):
        return self._name in SimpleType._STRING_TYPES

    @property
    def is_object(self):
        return self._name == 'object'

    @property
    def is_symbol(self):
        return self._name == 'symbol'

    @property
    def is_any(self):
        return self._name == 'any'

    @property
    def is_void(self):
        return self._name == 'void'


class ReferenceType(IdlType, WithIdentifier, Proxy):
    """
    Represents a type specified with the given identifier.

    As the exact type definitions are unknown in early compilation phases, it
    will be resolved in very end of the compilation phases.  Once everything is
    resolved, a ReferenceType behaves as a proxy to the resolved type.

    'typedef' in Web IDL is not a type, but we have TypedefType.  The
    identifier may be resolved to a TypedefType.
    """

    def __init__(self,
                 ref_to_idl_type,
                 extended_attributes=None,
                 code_generator_info=None,
                 debug_info=None):
        assert RefByIdFactory.is_reference(ref_to_idl_type)
        IdlType.__init__(
            self,
            extended_attributes=extended_attributes,
            code_generator_info=code_generator_info,
            debug_info=debug_info)
        WithIdentifier.__init__(self, ref_to_idl_type.identifier)
        # TODO(yukishiino): Set appropriate attributes to proxy.
        Proxy.__init__(self, target_object=ref_to_idl_type)


class DefinitionType(IdlType, WithIdentifier):
    """
    Represents a spec-author-defined type, e.g. interface type and dictionary
    type.

    Typedef and union type are not included.  They are represented as
    TypedefType and UnionType respectively.
    """

    def __init__(self,
                 user_def_type,
                 code_generator_info=None,
                 debug_info=None):
        assert isinstance(user_def_type, UserDefinedType)
        IdlType.__init__(
            self,
            code_generator_info=code_generator_info,
            debug_info=debug_info)
        WithIdentifier.__init__(self, user_def_type.identifier)
        self._definition = user_def_type

    # TODO(peria): Consider exposing access of |_definition|

    # IdlType overrides
    @property
    def syntactic_form(self):
        assert not self.extended_attributes
        return self.identifier

    @property
    def type_name(self):
        assert not self.extended_attributes
        return self.identifier

    @property
    def is_interface(self):
        return self._definition.is_interface

    @property
    def is_callback_interface(self):
        return self._definition.is_callback_interface

    @property
    def is_dictionary(self):
        return self._definition.is_dictionary

    @property
    def is_enumeration(self):
        return self._definition.is_enumeration

    @property
    def is_callback_function(self):
        return self._definition.is_callback_function


class TypedefType(IdlType, WithIdentifier):
    """
    Represents a typedef definition as an IdlType.

    'typedef' in Web IDL is not a type, however, there are use cases that have
    interest in typedef names.  Thus, the IDL compiler does not resolve
    typedefs transparently (i.e. does not remove typedefs entirely), and
    IdlTypes representing typedefs remain and behave like NullableType.  You
    can track down the typedef'ed type to |original_type|.
    """

    def __init__(self, typedef, code_generator_info=None, debug_info=None):
        IdlType.__init__(
            self,
            code_generator_info=code_generator_info,
            debug_info=debug_info)
        WithIdentifier.__init__(self, typedef.identifier)
        self._typedef = typedef

    @property
    def original_type(self):
        """
        Returns the typedef'ed original type
        @return IdlType
        """
        return self._typedef.idl_type

    # IdlType overrides
    @property
    def syntactic_form(self):
        assert not self.extended_attributes
        return self.identifier

    @property
    def type_name(self):
        assert not self.extended_attributes
        return self.original_type.type_name

    @property
    def does_include_nullable_type(self):
        return self.original_type.does_include_nullable_type

    @property
    def is_typedef(self):
        return True


class _ArrayLikeType(IdlType):
    def __init__(self,
                 element_type,
                 extended_attributes=None,
                 code_generator_info=None,
                 debug_info=None):
        IdlType.__init__(
            self,
            extended_attributes=extended_attributes,
            code_generator_info=code_generator_info,
            debug_info=debug_info)
        assert isinstance(element_type, IdlType)
        self._element_type = element_type

    @property
    def element_type(self):
        return self._element_type


class SequenceType(_ArrayLikeType):
    def __init__(self,
                 element_type,
                 extended_attributes=None,
                 code_generator_info=None,
                 debug_info=None):
        _ArrayLikeType.__init__(
            self,
            element_type,
            extended_attributes=extended_attributes,
            code_generator_info=code_generator_info,
            debug_info=debug_info)

    # IdlType overrides
    @property
    def syntactic_form(self):
        return self._format_syntactic_form('sequence<{}>'.format(
            self.element_type.syntactic_form))

    @property
    def type_name(self):
        return self._format_type_name('{}Sequence'.format(
            self.element_type.type_name))

    @property
    def is_sequence(self):
        return True


class FrozenArrayType(_ArrayLikeType):
    def __init__(self,
                 element_type,
                 extended_attributes=None,
                 code_generator_info=None,
                 debug_info=None):
        _ArrayLikeType.__init__(
            self,
            element_type,
            extended_attributes=extended_attributes,
            code_generator_info=code_generator_info,
            debug_info=debug_info)

    # IdlType overrides
    @property
    def syntactic_form(self):
        return self._format_syntactic_form('FrozenArray<{}>'.format(
            self.element_type.syntactic_form))

    @property
    def type_name(self):
        return self._format_type_name('{}Array'.format(
            self.element_type.type_name))

    @property
    def is_frozen_array(self):
        return True


class RecordType(IdlType):
    def __init__(self,
                 key_type,
                 value_type,
                 extended_attributes=None,
                 code_generator_info=None,
                 debug_info=None):
        IdlType.__init__(
            self,
            extended_attributes=extended_attributes,
            code_generator_info=code_generator_info,
            debug_info=debug_info)
        assert isinstance(key_type, IdlType)
        assert isinstance(value_type, IdlType)

        self._key_type = key_type
        self._value_type = value_type

    @property
    def key_type(self):
        """
        Returns the key type.
        @return IdlType
        """
        return self._key_type

    @property
    def value_type(self):
        """
        Returns the value type.
        @return IdlType
        """
        return self._value_type

    # IdlType overrides
    @property
    def syntactic_form(self):
        return self._format_syntactic_form('record<{}, {}>'.format(
            self.key_type.syntactic_form, self.value_type.syntactic_form))

    @property
    def type_name(self):
        return self._format_type_name('{}{}Record'.format(
            self.key_type.type_name, self.value_type.type_name))

    @property
    def is_record(self):
        return True


class PromiseType(IdlType):
    def __init__(self,
                 result_type,
                 extended_attributes=None,
                 code_generator_info=None,
                 debug_info=None):
        IdlType.__init__(
            self,
            extended_attributes=extended_attributes,
            code_generator_info=code_generator_info,
            debug_info=debug_info)
        assert isinstance(result_type, IdlType)
        self._result_type = result_type

    @property
    def result_type(self):
        """
        Returns the result type.
        @return IdlType
        """
        return self._result_type

    # IdlType overrides
    @property
    def syntactic_form(self):
        return self._format_syntactic_form('Promise<{}>'.format(
            self.result_type.syntactic_form))

    @property
    def type_name(self):
        return self._format_type_name('{}Promise'.format(
            self.result_type.type_name))

    @property
    def is_promise(self):
        return True


# https://heycam.github.io/webidl/#idl-union
class UnionType(IdlType):
    def __init__(self,
                 member_types,
                 extended_attributes=None,
                 code_generator_info=None,
                 debug_info=None):
        IdlType.__init__(
            self,
            extended_attributes=extended_attributes,
            code_generator_info=code_generator_info,
            debug_info=debug_info)
        assert isinstance(member_types, (list, tuple))
        assert all(isinstance(member, IdlType) for member in member_types)
        self._member_types = tuple(member_types)

    # TODO(peria): Make it possible to access the definition directly from this
    # object.

    @property
    def member_types(self):
        """
        Returns a list of member types.
        @return tuple(IdlType)
        """
        return self._member_types

    @property
    def flattened_member_types(self):
        """
        Returns a set of flattened member types.
        NOTE: Returned set does not contain nullable types even if |self|
        contains nullable types in its members.
        https://heycam.github.io/webidl/#dfn-flattened-union-member-types
        @return set(IdlType)
        """
        # TODO(peria): Implement this method.
        assert False, 'Not implemented yet'

    # IdlType overrides
    @property
    def syntactic_form(self):
        return self._format_syntactic_form('({})'.format(' or '.join(
            [member.syntactic_form for member in self.member_types])))

    @property
    def type_name(self):
        return self._format_type_name('Or'.join(
            [member.type_name for member in self.member_types]))

    @property
    def does_include_nullable_type(self):
        return any(
            member.does_include_nullable_type for member in self.member_types)

    @property
    def is_union(self):
        return True


class NullableType(IdlType):
    def __init__(self,
                 inner_type,
                 extended_attributes=None,
                 code_generator_info=None,
                 debug_info=None):
        IdlType.__init__(
            self,
            extended_attributes=extended_attributes,
            code_generator_info=code_generator_info,
            debug_info=debug_info)
        assert isinstance(inner_type, IdlType)
        self._inner_type = inner_type

    # IdlType overrides
    @property
    def syntactic_form(self):
        assert not self.extended_attributes
        return '{}?'.format(self.inner_type.syntactic_form)

    @property
    def type_name(self):
        assert not self.extended_attributes
        # https://heycam.github.io/webidl/#idl-annotated-types
        # Web IDL seems not supposing a case of [X] ([Y] Type)?, i.e. something
        # like [X] nullable<[Y] Type>, which should turn into "TypeYOrNullX".
        #
        # In case of '[Clamp] long?', it's interpreted as '([Clamp] long)?' but
        # the type name must be "LongOrNullClamp" instead of "LongClampOrNull".
        name = self.inner_type.type_name
        ext_attrs = ''.join(sorted(self.inner_type.extended_attributes.keys()))
        sep_index = len(name) - len(ext_attrs)
        return '{}OrNull{}'.format(name[0:sep_index], name[sep_index:])

    @property
    def does_include_nullable_type(self):
        return True

    @property
    def is_nullable(self):
        return True

    @property
    def inner_type(self):
        return self._inner_type
