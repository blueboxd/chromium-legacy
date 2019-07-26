# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .argument import Argument
from .ast_group import AstGroup
from .attribute import Attribute
from .callback_function import CallbackFunction
from .callback_interface import CallbackInterface
from .common import DebugInfo
from .common import Location
from .constant import Constant
from .dictionary import Dictionary
from .dictionary import DictionaryMember
from .enumeration import Enumeration
from .extended_attribute import ExtendedAttributes
from .idl_type import IdlTypeFactory
from .includes import Includes
from .interface import Interface
from .interface import Iterable
from .interface import Maplike
from .interface import Setlike
from .namespace import Namespace
from .operation import Operation
from .typedef import Typedef
from .values import ConstantValue
from .values import DefaultValue


def load_and_register_idl_definitions(
        filepaths, register_ir, create_ref_to_idl_def, create_ref_to_idl_type,
        idl_type_factory):
    """
    Reads ASTs from |filepaths| and builds IRs from ASTs.

    Args:
        filepaths: Paths to pickle files that store AST nodes.
        register_ir: A callback function that registers the argument as an
            IR.
        create_ref_to_idl_def: A callback function that creates a reference
            to an IDL definition from the given identifier.
        create_ref_to_idl_type: A callback function that creates a reference
            to an IdlType from the given identifier.
        idl_type_factory: All IdlType instances will be created through this
            factory.
    """
    assert callable(register_ir)

    for filepath in filepaths:
        asts_per_component = AstGroup.read_from_file(filepath)
        component = asts_per_component.component
        builder = _IRBuilder(
            component=component,
            create_ref_to_idl_def=create_ref_to_idl_def,
            create_ref_to_idl_type=create_ref_to_idl_type,
            idl_type_factory=idl_type_factory)

        for file_node in asts_per_component:
            assert file_node.GetClass() == 'File'
            for top_level_node in file_node.GetChildren():
                register_ir(builder.build_top_level_def(top_level_node))


class _IRBuilder(object):
    def __init__(self, component, create_ref_to_idl_def,
                 create_ref_to_idl_type, idl_type_factory):
        """
        Args:
            component: A Component to which the built IRs are associated.
            create_ref_to_idl_def: A callback function that creates a reference
                to an IDL definition from the given identifier.
            create_ref_to_idl_type: A callback function that creates a reference
                to an IdlType from the given identifier.
            idl_type_factory: All IdlType instances will be created through this
                factory.
        """
        assert callable(create_ref_to_idl_def)
        assert callable(create_ref_to_idl_type)
        assert isinstance(idl_type_factory, IdlTypeFactory)

        self._component = component
        self._create_ref_to_idl_def = create_ref_to_idl_def
        self._create_ref_to_idl_type = create_ref_to_idl_type
        self._idl_type_factory = idl_type_factory

    def build_top_level_def(self, node):
        build_functions = {
            'Callback': self._build_callback_function,
            'Dictionary': self._build_dictionary,
            'Enum': self._build_enumeration,
            'Includes': self._build_includes,
            'Interface': self._build_interface,
            'Namespace': self._build_namespace,
            'Typedef': self._build_typedef,
        }
        return build_functions[node.GetClass()](node)

    # Builder functions for top-level definitions

    def _build_interface(self, node):
        if node.GetProperty('CALLBACK'):
            return self._build_callback_interface(node)

        child_nodes = list(node.GetChildren())
        extended_attributes = self._take_extended_attributes(child_nodes)
        inherited = self._take_inheritance(child_nodes)
        iterable = self._take_iterable(child_nodes)
        maplike = self._take_maplike(child_nodes)
        setlike = self._take_setlike(child_nodes)
        # TODO(peria): Implement stringifier.
        _ = self._take_stringifier(child_nodes)

        members = map(self._build_interface_or_namespace_member, child_nodes)
        attributes = []
        constants = []
        operations = []
        for member in members:
            if isinstance(member, Attribute.IR):
                attributes.append(member)
            elif isinstance(member, Operation.IR):
                if member.identifier:
                    operations.append(member)
            elif isinstance(member, Constant.IR):
                constants.append(member)
            else:
                assert False
        # TODO(peria): Implement indexed/named property handlers from
        # |property_handlers|.

        return Interface.IR(
            identifier=node.GetName(),
            is_partial=bool(node.GetProperty('PARTIAL')),
            is_mixin=bool(node.GetProperty('MIXIN')),
            inherited=inherited,
            attributes=attributes,
            constants=constants,
            operations=operations,
            iterable=iterable,
            maplike=maplike,
            setlike=setlike,
            extended_attributes=extended_attributes,
            component=self._component,
            debug_info=self._build_debug_info(node))

    def _build_namespace(self, node):
        namespace = Namespace.IR(
            identifier=node.GetName(),
            is_partial=bool(node.GetProperty('PARTIAL')),
            component=self._component,
            debug_info=self._build_debug_info(node))
        # TODO(peria): Build members and register them in |namespace|
        return namespace

    def _build_interface_or_namespace_member(self, node):
        def build_attribute(node):
            child_nodes = list(node.GetChildren())
            idl_type = self._take_type(child_nodes)
            extended_attributes = self._take_extended_attributes(child_nodes)
            assert len(child_nodes) == 0
            return Attribute.IR(
                identifier=node.GetName(),
                idl_type=idl_type,
                is_static=bool(node.GetProperty('STATIC')),
                is_readonly=bool(node.GetProperty('READONLY')),
                does_inherit_getter=bool(node.GetProperty('INHERIT')),
                extended_attributes=extended_attributes,
                component=self._component,
                debug_info=self._build_debug_info(node))

        def build_constant(node):
            child_nodes = list(node.GetChildren())
            value = self._take_constant_value(child_nodes)
            extended_attributes = self._take_extended_attributes(child_nodes)
            assert len(child_nodes) == 1, child_nodes[0].GetClass()
            # idl_parser doesn't produce a 'Type' node for the type of a
            # constant, hence we need to skip one level.
            idl_type = self._build_type_internal(child_nodes)
            return Constant.IR(
                identifier=node.GetName(),
                value=value,
                idl_type=idl_type,
                extended_attributes=extended_attributes,
                component=self._component,
                debug_info=self._build_debug_info(node))

        def build_operation(node):
            child_nodes = list(node.GetChildren())
            arguments = self._take_arguments(child_nodes)
            return_type = self._take_type(child_nodes)
            extended_attributes = self._take_extended_attributes(child_nodes)
            assert len(child_nodes) == 0
            return Operation.IR(
                identifier=node.GetName(),
                arguments=arguments,
                return_type=return_type,
                is_static=bool(node.GetProperty('STATIC')),
                extended_attributes=extended_attributes,
                component=self._component,
                debug_info=self._build_debug_info(node))

        build_functions = {
            'Attribute': build_attribute,
            'Const': build_constant,
            'Operation': build_operation,
        }
        return build_functions[node.GetClass()](node)

    def _build_dictionary(self, node):
        child_nodes = list(node.GetChildren())
        extended_attributes = self._take_extended_attributes(child_nodes)
        inherited = self._take_inheritance(child_nodes)
        own_members = map(self._build_dictionary_member, child_nodes)

        return Dictionary.IR(
            identifier=node.GetName(),
            is_partial=bool(node.GetProperty('PARTIAL')),
            inherited=inherited,
            own_members=own_members,
            extended_attributes=extended_attributes,
            component=self._component,
            debug_info=self._build_debug_info(node))

    def _build_dictionary_member(self, node):
        assert node.GetClass() == 'Key'

        child_nodes = list(node.GetChildren())
        is_required = bool(node.GetProperty('REQUIRED'))
        idl_type = self._take_type(child_nodes, is_optional=(not is_required))
        default_value = self._take_default_value(child_nodes)
        extended_attributes = self._take_extended_attributes(child_nodes)
        assert len(child_nodes) == 0

        return DictionaryMember.IR(
            identifier=node.GetName(),
            idl_type=idl_type,
            default_value=default_value,
            extended_attributes=extended_attributes,
            component=self._component,
            debug_info=self._build_debug_info(node))

    def _build_callback_interface(self, node):
        callback_interface = CallbackInterface.IR(
            identifier=node.GetName(),
            component=self._component,
            debug_info=self._build_debug_info(node))
        # TODO(peria): Build members and register them in |callback_interface|
        return callback_interface

    def _build_callback_function(self, node):
        callback_function = CallbackFunction.IR(
            identifier=node.GetName(),
            component=self._component,
            debug_info=self._build_debug_info(node))
        # TODO(peria): Build members and register them in |callback_function|
        return callback_function

    def _build_enumeration(self, node):
        enumeration = Enumeration.IR(
            identifier=node.GetName(),
            values=[child.GetName() for child in node.GetChildren()],
            component=self._component,
            debug_info=self._build_debug_info(node))
        return enumeration

    def _build_typedef(self, node):
        child_nodes = list(node.GetChildren())
        idl_type = self._take_type(child_nodes)
        assert len(child_nodes) == 0

        typedef = Typedef.IR(
            identifier=node.GetName(),
            idl_type=idl_type,
            component=self._component,
            debug_info=self._build_debug_info(node))
        return typedef

    def _build_includes(self, node):
        includes = Includes.IR(
            interface_identifier=node.GetName(),
            mixin_identifier=node.GetProperty('REFERENCE'),
            component=self._component,
            debug_info=self._build_debug_info(node))
        return includes

    # Helper functions sorted alphabetically

    def _build_arguments(self, node):
        def build_argument(node, index):
            assert node.GetClass() == 'Argument'
            child_nodes = list(node.GetChildren())
            is_optional = bool(node.GetProperty('OPTIONAL'))
            is_variadic = bool(self._take_is_variadic_argument(child_nodes))
            idl_type = self._take_type(
                child_nodes, is_optional=is_optional, is_variadic=is_variadic)
            default_value = self._take_default_value(child_nodes)
            extended_attributes = self._take_extended_attributes(child_nodes)
            assert len(child_nodes) == 0
            return Argument.IR(
                identifier=node.GetName(),
                index=index,
                idl_type=idl_type,
                default_value=default_value,
                extended_attributes=extended_attributes)

        assert node.GetClass() == 'Arguments'
        return [
            build_argument(node, i)
            for i, node in enumerate(node.GetChildren())
        ]

    def _build_constant_value(self, node):
        assert node.GetClass() == 'Value'
        return ConstantValue()

    def _build_debug_info(self, node):
        return DebugInfo(
            location=Location(
                filepath=node.GetProperty('FILENAME'),
                line_number=node.GetProperty('LINENO'),
                position=node.GetProperty('POSITION')))

    def _build_default_value(self, node):
        assert node.GetClass() == 'Default'
        return DefaultValue()

    def _build_extended_attributes(self, node):
        assert node.GetClass() == 'ExtAttributes'
        return ExtendedAttributes()

    def _build_inheritance(self, node):
        assert node.GetClass() == 'Inherit'
        return self._create_ref_to_idl_def(node.GetName(),
                                           self._build_debug_info(node))

    def _build_is_variadic_argument(self, node):
        # idl_parser produces the following tree to indicate an argument is
        # variadic.
        #   Arguments
        #     := [Argument, Argument, ...]
        #   Argument
        #     := [Type, Argument(Name='...')]  # Argument inside Argument
        assert node.GetClass() == 'Argument'
        assert node.GetName() == '...'
        return True

    def _build_iterable(self, node):
        assert node.GetClass() == 'Iterable'
        types = map(self._build_type, node.GetChildren())
        if len(types) == 1:
            types.insert(0, None)
        assert len(types) == 2
        return Iterable(
            key_type=types[0],
            value_type=types[1],
            debug_info=self._build_debug_info(node))

    def _build_maplike(self, node):
        assert node.GetClass() == 'Maplike'
        types = map(self._build_type, node.GetChildren())
        assert len(types) == 2
        return Maplike(
            key_type=types[0],
            value_type=types[1],
            is_readonly=bool(node.GetProperty('READONLY')),
            debug_info=self._build_debug_info(node))

    def _build_setlike(self, node):
        assert node.GetClass() == 'Setlike'
        assert len(node.GetChildren()) == 1
        return Setlike(
            value_type=self._build_type(node.GetChildren()[0]),
            is_readonly=bool(node.GetProperty('READONLY')),
            debug_info=self._build_debug_info(node))

    def _build_stringifier(self, node):
        assert node.GetClass() == 'Stringifier'
        return None

    def _build_type(self, node, is_optional=False, is_variadic=False):
        def build_maybe_nullable_type(node):
            if node.GetProperty('NULLABLE'):
                return self._idl_type_factory.nullable_type(
                    self._build_type_internal(node.GetChildren()),
                    is_optional=is_optional)
            return self._build_type_internal(
                node.GetChildren(), is_optional=is_optional)

        assert node.GetClass() == 'Type'
        assert not (is_optional and is_variadic)
        if is_variadic:
            return self._idl_type_factory.variadic_type(
                element_type=build_maybe_nullable_type(node),
                debug_info=self._build_debug_info(node))
        return build_maybe_nullable_type(node)

    def _build_type_internal(self, nodes, is_optional=False):
        """
        Args:
            nodes: The child nodes of a 'Type' node.
        """

        def build_frozen_array_type(node, extended_attributes):
            assert len(node.GetChildren()) == 1
            return self._idl_type_factory.frozen_array_type(
                element_type=self._build_type(node.GetChildren()[0]),
                is_optional=is_optional,
                extended_attributes=extended_attributes,
                debug_info=self._build_debug_info(node))

        def build_promise_type(node, extended_attributes):
            assert len(node.GetChildren()) == 1
            return self._idl_type_factory.promise_type(
                result_type=self._build_type(node.GetChildren()[0]),
                is_optional=is_optional,
                extended_attributes=extended_attributes,
                debug_info=self._build_debug_info(node))

        def build_union_type(node, extended_attributes):
            return self._idl_type_factory.union_type(
                member_types=map(self._build_type, node.GetChildren()),
                is_optional=is_optional,
                extended_attributes=extended_attributes,
                debug_info=self._build_debug_info(node))

        def build_record_type(node, extended_attributes):
            key_node, value_node = node.GetChildren()
            return self._idl_type_factory.record_type(
                # idl_parser doesn't produce a 'Type' node for the key type,
                # hence we need to skip one level.
                key_type=self._build_type_internal([key_node]),
                value_type=self._build_type(value_node),
                is_optional=is_optional,
                extended_attributes=extended_attributes,
                debug_info=self._build_debug_info(node))

        def build_reference_type(node, extended_attributes):
            identifier = node.GetName()
            return self._idl_type_factory.reference_type(
                ref_to_idl_type=self._create_ref_to_idl_type(
                    identifier, self._build_debug_info(node)),
                is_optional=is_optional,
                extended_attributes=extended_attributes,
                debug_info=self._build_debug_info(node))

        def build_sequence_type(node, extended_attributes):
            return self._idl_type_factory.sequence_type(
                element_type=self._build_type(node.GetChildren()[0]),
                is_optional=is_optional,
                extended_attributes=extended_attributes,
                debug_info=self._build_debug_info(node))

        def build_simple_type(node, extended_attributes):
            name = node.GetName()
            if name is None:
                assert node.GetClass() == 'Any'
                name = node.GetClass().lower()
            if node.GetProperty('UNRESTRICTED'):
                name = 'unrestricted {}'.format(name)
            return self._idl_type_factory.simple_type(
                name=name,
                is_optional=is_optional,
                extended_attributes=extended_attributes,
                debug_info=self._build_debug_info(node))

        type_nodes = list(nodes)
        extended_attributes = self._take_extended_attributes(type_nodes)
        assert len(type_nodes) == 1
        body_node = type_nodes[0]

        build_functions = {
            'Any': build_simple_type,
            'FrozenArray': build_frozen_array_type,
            'PrimitiveType': build_simple_type,
            'Promise': build_promise_type,
            'Record': build_record_type,
            'Sequence': build_sequence_type,
            'StringType': build_simple_type,
            'Typeref': build_reference_type,
            'UnionType': build_union_type,
        }
        return build_functions[body_node.GetClass()](body_node,
                                                     extended_attributes)

    def _take_and_build(self, node_class, build_func, node_list, **kwargs):
        """
        Takes a node of |node_class| from |node_list| if any, and then builds
        and returns an IR.  The processed node is removed from |node_list|.
        Returns None if not found.
        """
        for node in node_list:
            if node.GetClass() == node_class:
                node_list.remove(node)
                return build_func(node, **kwargs)
        return None

    def _take_arguments(self, node_list):
        return self._take_and_build('Arguments', self._build_arguments,
                                    node_list)

    def _take_constant_value(self, node_list):
        return self._take_and_build('Value', self._build_constant_value,
                                    node_list)

    def _take_default_value(self, node_list):
        return self._take_and_build('Default', self._build_default_value,
                                    node_list)

    def _take_extended_attributes(self, node_list):
        return self._take_and_build('ExtAttributes',
                                    self._build_extended_attributes, node_list)

    def _take_inheritance(self, node_list):
        return self._take_and_build('Inherit', self._build_inheritance,
                                    node_list)

    def _take_is_variadic_argument(self, node_list):
        return self._take_and_build(
            'Argument', self._build_is_variadic_argument, node_list)

    def _take_iterable(self, node_list):
        return self._take_and_build('Iterable', self._build_iterable,
                                    node_list)

    def _take_maplike(self, node_list):
        return self._take_and_build('Maplike', self._build_maplike, node_list)

    def _take_setlike(self, node_list):
        return self._take_and_build('Setlike', self._build_setlike, node_list)

    def _take_stringifier(self, node_list):
        return self._take_and_build('Stringifier', self._build_stringifier,
                                    node_list)

    def _take_type(self, node_list, is_optional=False, is_variadic=False):
        return self._take_and_build(
            'Type',
            self._build_type,
            node_list,
            is_optional=is_optional,
            is_variadic=is_variadic)
