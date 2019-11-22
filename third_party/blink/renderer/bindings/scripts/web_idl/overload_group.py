# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .composition_parts import WithIdentifier
from .function_like import FunctionLike
from .idl_type import IdlType


class OverloadGroup(WithIdentifier):
    class IR(WithIdentifier):
        def __init__(self, functions):
            assert isinstance(functions, (list, tuple))
            assert all(
                isinstance(function, FunctionLike.IR)
                for function in functions)
            assert len(set(
                [function.identifier for function in functions])) == 1
            assert len(set(
                [function.is_static for function in functions])) == 1

            WithIdentifier.__init__(self, functions[0].identifier)
            self.functions = list(functions)
            self.is_static = functions[0].is_static

        def __iter__(self):
            return iter(self.functions)

        def __len__(self):
            return len(self.functions)

    def __init__(self, functions):
        assert isinstance(functions, (list, tuple))
        assert all(
            isinstance(function, FunctionLike) for function in functions)
        assert len(set([function.identifier for function in functions])) == 1
        assert len(set([function.is_static for function in functions])) == 1

        WithIdentifier.__init__(self, functions[0].identifier)
        self._functions = tuple(functions)
        self._is_static = functions[0].is_static

    def __iter__(self):
        return iter(self._functions)

    def __len__(self):
        return len(self._functions)

    @property
    def is_static(self):
        return self._is_static

    @property
    def min_num_of_required_arguments(self):
        """
        Returns the minimum number of required arguments of overloaded
        functions.
        """
        return min(map(lambda func: func.num_of_required_arguments, self))

    def effective_overload_set(self, argument_count=None):
        """
        Returns the effective overload set.

        Returns:
            List of (FunctionLike, (IdlType...), (IdlType.Optionality...)).
        """
        assert argument_count is None or isinstance(argument_count,
                                                    (int, long))

        # https://heycam.github.io/webidl/#compute-the-effective-overload-set
        N = argument_count
        S = []
        F = self

        maxarg = max(map(lambda X: len(X.arguments), F))
        if N is None:
            N = 1 + max([len(X.arguments) for X in F if not X.is_variadic])

        for X in F:
            n = len(X.arguments)

            S.append((X, map(lambda arg: arg.idl_type, X.arguments),
                      map(lambda arg: arg.optionality, X.arguments)))

            if X.is_variadic:
                for i in xrange(n, max(maxarg, N)):
                    t = map(lambda arg: arg.idl_type, X.arguments)
                    o = map(lambda arg: arg.optionality, X.arguments)
                    for _ in xrange(n, i + 1):
                        t.append(X.arguments[-1].idl_type)
                        o.append(X.arguments[-1].optionality)
                    S.append((X, t, o))

            t = map(lambda arg: arg.idl_type, X.arguments)
            o = map(lambda arg: arg.optionality, X.arguments)
            for i in xrange(n - 1, -1, -1):
                if X.arguments[i].optionality == IdlType.Optionality.REQUIRED:
                    break
                S.append((X, t[:i], o[:i]))

        return S

    @staticmethod
    def are_distinguishable_types(idl_type1, idl_type2):
        """Returns True if the two given types are distinguishable."""
        assert isinstance(idl_type1, IdlType)
        assert isinstance(idl_type2, IdlType)

        # https://heycam.github.io/webidl/#dfn-distinguishable
        # step 1. If one type includes a nullable type and the other type either
        #   includes a nullable type, is a union type with flattened member
        #   types including a dictionary type, or is a dictionary type, ...
        type1_nullable = (idl_type1.does_include_nullable_type
                          or idl_type1.unwrap().is_dictionary)
        type2_nullable = (idl_type2.does_include_nullable_type
                          or idl_type2.unwrap().is_dictionary)
        if type1_nullable and type2_nullable:
            return False

        type1 = idl_type1.unwrap()
        type2 = idl_type2.unwrap()

        # step 2. If both types are either a union type or nullable union type,
        #   ...
        if type1.is_union and type2.is_union:
            for member1 in type1.member_types:
                for member2 in type2.member_types:
                    if not OverloadGroup.are_distinguishable_types(
                            member1, member2):
                        return False
            return True

        # step 3. If one type is a union type or nullable union type, ...
        if type1.is_union or type2.is_union:
            union = type1 if type1.is_union else type2
            other = type2 if type1.is_union else type1
            for member in union.member_types:
                if not OverloadGroup.are_distinguishable_types(member, other):
                    return False
            return True

        # step 4. Consider the two "innermost" types ...
        def is_interface_like(idl_type):
            # TODO(yukishiino): Add buffer source types into IdlType.
            return idl_type.is_interface  # or buffer source types

        def is_dictionary_like(idl_type):
            return (idl_type.is_dictionary or idl_type.is_record
                    or idl_type.is_callback_interface)

        def is_sequence_like(idl_type):
            return idl_type.is_sequence or idl_type.is_frozen_array

        if not (type2.is_boolean or type2.is_numeric or type2.is_string
                or type2.is_object or type2.is_symbol
                or is_interface_like(type2) or type2.is_callback_function
                or is_dictionary_like(type2) or is_sequence_like(type2)):
            return False  # Out of the table

        if type1.is_boolean:
            return not type2.is_boolean
        if type1.is_numeric:
            return not type2.is_numeric
        if type1.is_string:
            return not type2.is_string
        if type1.is_object:
            return (type2.is_boolean or type2.is_numeric or type2.is_string
                    or type2.is_symbol)
        if type1.is_symbol:
            return not type2.is_symbol
        if is_interface_like(type1):
            if type2.is_object:
                return False
            if not is_interface_like(type2):
                return True
            # Additional requirements: The two identified interface-like types
            # are not the same, and no single platform object implements both
            # interface-like types.
            if not (type1.is_interface and type2.is_interface):
                return type1.identifier != type2.identifier
            interface1 = type1.type_definition_object
            interface2 = type2.type_definition_object
            return not (
                interface1 in interface2.inclusive_inherited_interfaces
                or interface2 in interface1.inclusive_inherited_interfaces)
        if type1.is_callback_function:
            return not (type2.is_object or type2.is_callback_function
                        or is_dictionary_like(type2))
        if is_dictionary_like(type1):
            return not (type2.is_object or type2.is_callback_function
                        or is_dictionary_like(type2))
        if is_sequence_like(type1):
            return not (type2.is_object or is_sequence_like(type2))
        return False  # Out of the table
