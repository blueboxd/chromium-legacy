# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from .extended_attribute import ExtendedAttribute
from .extended_attribute import ExtendedAttributes
from .idl_types import AnnotatedType
from .idl_types import FrozenArrayType
from .idl_types import NullableType
from .idl_types import PromiseType
from .idl_types import RecordType
from .idl_types import SequenceType
from .idl_types import SimpleType
from .idl_types import UnionType


class IdlTypesTest(unittest.TestCase):
    def test_property(self):
        self.assertTrue(SimpleType('any').is_any)
        self.assertTrue(SimpleType('boolean').is_boolean)
        self.assertTrue(SimpleType('object').is_object)
        self.assertTrue(SimpleType('void').is_void)
        self.assertTrue(SimpleType('symbol').is_symbol)

        for x in ('byte', 'octet', 'short', 'unsigned short', 'long',
                  'unsigned long', 'long long', 'unsigned long long'):
            self.assertTrue(SimpleType(x).is_numeric)
            self.assertTrue(SimpleType(x).is_integer)
        for x in ('float', 'unrestricted float', 'double',
                  'unrestricted double'):
            self.assertTrue(SimpleType(x).is_numeric)
            self.assertFalse(SimpleType(x).is_integer)
        for x in ('DOMString', 'ByteString', 'USVString'):
            self.assertTrue(SimpleType(x).is_string)

        short_type = SimpleType('short')
        string_type = SimpleType('DOMString')
        ext_attrs = ExtendedAttributes([ExtendedAttribute('Clamp')])
        self.assertTrue(PromiseType(short_type).is_promise)
        self.assertTrue(RecordType(short_type, string_type).is_record)
        self.assertTrue(SequenceType(short_type).is_sequence)
        self.assertTrue(FrozenArrayType(short_type).is_frozen_array)
        self.assertTrue(UnionType([short_type, string_type]).is_union)
        self.assertTrue(NullableType(short_type).is_nullable)
        annotated_type = AnnotatedType(short_type, ext_attrs)
        self.assertTrue(annotated_type.is_annotated)
        # Predictors are not transparent
        self.assertFalse(annotated_type.is_numeric)

        self.assertFalse(SimpleType('long').is_string)
        self.assertFalse(SimpleType('DOMString').is_object)
        self.assertFalse(SimpleType('symbol').is_string)

    def test_type_name(self):
        type_names = {
            'byte': 'Byte',
            'unsigned long long': 'UnsignedLongLong',
            'unrestricted double': 'UnrestrictedDouble',
            'DOMString': 'String',
            'ByteString': 'ByteString',
            'USVString': 'USVString',
            'any': 'Any',
            'boolean': 'Boolean',
            'object': 'Object',
            'void': 'Void',
            'symbol': 'Symbol',
        }
        for name, expect in type_names.iteritems():
            self.assertEqual(expect, SimpleType(name).type_name)

        short_type = SimpleType('short')
        string_type = SimpleType('DOMString')
        self.assertEqual('ShortOrString',
                         UnionType([short_type, string_type]).type_name)
        self.assertEqual('StringOrShort',
                         UnionType([string_type, short_type]).type_name)
        self.assertEqual('ShortPromise', PromiseType(short_type).type_name)
        self.assertEqual('ShortStringRecord',
                         RecordType(short_type, string_type).type_name)
        self.assertEqual('ShortSequence', SequenceType(short_type).type_name)
        self.assertEqual('ShortArray', FrozenArrayType(short_type).type_name)
        self.assertEqual('ShortOrNull', NullableType(short_type).type_name)

        ext_attrs = ExtendedAttributes(
            [ExtendedAttribute('TreatNullAs', 'EmptyString')])
        self.assertEqual('StringTreatNullAs',
                         AnnotatedType(string_type, ext_attrs).type_name)

    def test_union_types(self):
        # Test target: ((unrestricted double or object)? or
        #               [TreatNullAs=EmptyString] DOMString)
        treat_null_as = ExtendedAttribute('TreatNullAs', 'EmptyString')
        annotated_string = AnnotatedType(
            SimpleType('DOMString'), ExtendedAttributes([treat_null_as]))
        obj = SimpleType('object')
        unrestricted_double = SimpleType('unrestricted double')
        union = UnionType(
            [UnionType([unrestricted_double, obj]), annotated_string])

        self.assertEqual(len(union.member_types), 2)
        # TODO(peria): Enable following tests.
        # self.assertEqual(len(union.flattened_member_types), 3)
        # self.assertTrue(union.does_include_nullable_type)

    # TODO(peria): Implement tests for ReferenceType, DefinitionType, and
    # TypeAlias
