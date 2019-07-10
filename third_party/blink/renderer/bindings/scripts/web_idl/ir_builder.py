# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .callback_function import CallbackFunction
from .callback_interface import CallbackInterface
from .collection import Collection
from .common import DebugInfo
from .dictionary import Dictionary
from .dictionary import DictionaryMember
from .enumeration import Enumeration
from .extended_attribute import ExtendedAttributes
from .idl_types import AnnotatedType
from .idl_types import FrozenArrayType
from .idl_types import NullableType
from .idl_types import PromiseType
from .idl_types import RecordType
from .idl_types import ReferenceType
from .idl_types import SequenceType
from .idl_types import SimpleType
from .idl_types import UnionType
from .includes import Includes
from .interface import Interface
from .namespace import Namespace
from .typedef import Typedef
from .values import DefaultValue


def load_and_register_idl_definitions(
        filepaths, register_ir, create_ref_to_idl_type, create_ref_to_idl_def):
    """
    Reads ASTs from |filepaths| and builds IRs from ASTs.

    Args:
        filepaths: Paths to pickle files that store AST nodes.
        register_ir: A callback function that registers the argument as an
            IR.
        create_ref_to_idl_type: A callback function that creates a reference
            to an IdlType from the given identifier.
        create_ref_to_idl_def: A callback function that creates a reference
            to an IDL definition from the given identifier.
    """
    assert callable(register_ir)

    for filepath in filepaths:
        asts_per_component = Collection.load_from_file(filepath)
        component = asts_per_component.component
        builder = _IRBuilder(component, create_ref_to_idl_type,
                             create_ref_to_idl_def)

        for file_node in asts_per_component.asts:
            assert file_node.GetClass() == 'File'
            for top_level_node in file_node.GetChildren():
                register_ir(builder.build_top_level_def(top_level_node))


class _IRBuilder(object):
    def __init__(self, component, create_ref_to_idl_type,
                 create_ref_to_idl_def):
        """
        Args:
            component: A Component to which the built IRs are associated.
            create_ref_to_idl_type: A callback function that creates a reference
                to an IdlType from the given identifier.
            create_ref_to_idl_def: A callback function that creates a reference
                to an IDL definition from the given identifier.
        """
        assert callable(create_ref_to_idl_type)
        assert callable(create_ref_to_idl_def)

        self._component = component
        self._create_ref_to_idl_type = create_ref_to_idl_type
        self._create_ref_to_idl_def = create_ref_to_idl_def

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

        interface = Interface.IR(
            identifier=node.GetName(),
            is_partial=bool(node.GetProperty('PARTIAL')),
            is_mixin=bool(node.GetProperty('MIXIN')),
            component=self._component,
            debug_info=self._build_debug_info(node))
        # TODO(peria): Build members and register them in |interface|
        return interface

    def _build_namespace(self, node):
        namespace = Namespace.IR(
            identifier=node.GetName(),
            is_partial=bool(node.GetProperty('PARTIAL')),
            component=self._component,
            debug_info=self._build_debug_info(node))
        # TODO(peria): Build members and register them in |namespace|
        return namespace

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
        idl_type = self._take_type(child_nodes)
        default_value = self._take_default_value(child_nodes)
        extended_attributes = self._take_extended_attributes(child_nodes)
        assert len(child_nodes) == 0

        return DictionaryMember.IR(
            identifier=node.GetName(),
            idl_type=idl_type,
            is_required=bool(node.GetProperty('REQUIRED')),
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

    def _build_debug_info(self, node):
        return DebugInfo(
            location=DebugInfo.Location(
                filepath=node.GetProperty('FILENAME'),
                line_number=node.GetProperty('LINENO'),
                column_number=node.GetProperty('POSITION')))

    def _build_default_value(self, node):
        assert node.GetClass() == 'Default'
        return DefaultValue()

    def _build_extended_attributes(self, node):
        assert node.GetClass() == 'ExtAttributes'
        return ExtendedAttributes()

    def _build_inheritance(self, node):
        assert node.GetClass() == 'Inherit'
        return self._create_ref_to_idl_def(node.GetName())

    def _build_type(self, node):
        def build_maybe_inner_type(node):
            return self._build_type_internal(node.GetChildren()[0])

        def build_maybe_nullable_type(node):
            maybe_inner_type = build_maybe_inner_type(node)
            if node.GetProperty('NULLABLE'):
                return NullableType(maybe_inner_type)
            return maybe_inner_type

        def build_maybe_annotated_type(node):
            type_nodes = list(node.GetChildren())
            extended_attributes = self._take_extended_attributes(type_nodes)
            assert len(type_nodes) == 1
            maybe_inner_type = build_maybe_nullable_type(node)
            if extended_attributes:
                return AnnotatedType(
                    inner_type=maybe_inner_type,
                    extended_attributes=extended_attributes)
            return maybe_inner_type

        assert node.GetClass() == 'Type'
        return build_maybe_annotated_type(node)

    def _build_type_internal(self, node):
        """
        Args:
            node: The body node of the type definition, which is supposed to be
                the first child of a 'Type' node.
        """

        def build_frozen_array_type(node):
            assert len(node.GetChildren()) == 1
            return FrozenArrayType(
                element_type=self._build_type(node.GetChildren()[0]),
                debug_info=self._build_debug_info(node))

        def build_promise_type(node):
            assert len(node.GetChildren()) == 1
            return PromiseType(
                result_type=self._build_type(node.GetChildren()[0]),
                debug_info=self._build_debug_info(node))

        def build_union_type(node):
            union_type = UnionType(
                member_types=map(self._build_type, node.GetChildren()))
            return union_type

        def build_record_type(node):
            key_node, value_node = node.GetChildren()
            return RecordType(
                # idl_parser doesn't produce a 'Type' node for the key type,
                # hence we need to skip one level.
                key_type=self._build_type_internal(key_node),
                value_type=self._build_type(value_node),
                debug_info=self._build_debug_info(node))

        def build_reference_type(node):
            identifier = node.GetName()
            ref_type = ReferenceType(
                ref_to_idl_type=self._create_ref_to_idl_type(identifier),
                debug_info=self._build_debug_info(node))
            return ref_type

        def build_sequence_type(node):
            return SequenceType(
                element_type=self._build_type(node.GetChildren()[0]),
                debug_info=self._build_debug_info(node))

        def build_simple_type(node):
            type_name = node.GetName()
            if type_name is None:
                assert node.GetClass() == 'Any'
                type_name = node.GetClass().lower()
            if node.GetProperty('UNRESTRICTED'):
                type_name = 'unrestricted ' + type_name
            return SimpleType(
                name=type_name, debug_info=self._build_debug_info(node))

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
        return build_functions[node.GetClass()](node)

    def _take_and_build(self, node_class, build_func, node_list):
        """
        Takes a node of |node_class| from |node_list| if any, and then builds
        and returns an IR.  The processed node is removed from |node_list|.
        Returns None if not found.
        """
        for node in node_list:
            if node.GetClass() == node_class:
                node_list.remove(node)
                return build_func(node)
        return None

    def _take_default_value(self, node_list):
        return self._take_and_build('Default', self._build_default_value,
                                    node_list)

    def _take_extended_attributes(self, node_list):
        return self._take_and_build('ExtAttributes',
                                    self._build_extended_attributes, node_list)

    def _take_inheritance(self, node_list):
        return self._take_and_build('Inherit', self._build_inheritance,
                                    node_list)

    def _take_type(self, node_list):
        return self._take_and_build('Type', self._build_type, node_list)
