# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for generate_schema_org_code."""

import sys
import unittest
import generate_schema_org_code
from generate_schema_org_code import schema_org_id
import os

SRC = os.path.join(os.path.dirname(__file__), os.path.pardir, os.path.pardir)
sys.path.append(os.path.join(SRC, 'third_party', 'pymock'))
import mock

_current_dir = os.path.dirname(os.path.realpath(__file__))
# jinja2 is in chromium's third_party directory
# Insert at front to override system libraries, and after path[0] == script dir
sys.path.insert(
    1, os.path.join(_current_dir, *([os.pardir] * 2 + ['third_party'])))
import jinja2


class GenerateSchemaOrgCodeTest(unittest.TestCase):
    def test_get_template_vars(self):
        schema = {
            "@graph": [{
                "@id": "http://schema.org/MediaObject",
                "@type": "rdfs:Class"
            },
                       {
                           "@id": "http://schema.org/propertyName",
                           "@type": "rdf:Property"
                       }]
        }

        names = {
            "http://schema.org/MediaObject": 1234,
            "MediaObject": 1235,
            "http://schema.org/propertyName": 2345,
            "propertyName": 2346
        }

        self.assertEqual(
            generate_schema_org_code.get_template_vars(schema, names), {
                'entities': [{
                    'name': 'MediaObject',
                    'name_hash': 1235
                }],
                'properties': [{
                    'name': 'propertyName',
                    'name_hash': 2346,
                    'thing_types': [],
                    'enum_types': []
                }],
                'enums': [],
                'entity_parent_lookup':
                [{
                    'name': 'MediaObject',
                    'name_hash': 1235,
                    'parents': [{
                        'name': 'MediaObject',
                        'name_hash': 1235
                    }]
                }]
            })

    def test_lookup_parents(self):
        thing = {'@id': schema_org_id('Thing')}
        intangible = {
            '@id': schema_org_id('Intangible'),
            'rdfs:subClassOf': thing
        }
        structured_value = {
            '@id': schema_org_id('StructuredValue'),
            'rdfs:subClassOf': intangible
        }
        brand = {'@id': schema_org_id('Brand'), 'rdfs:subClassOf': intangible}
        schema = {'@graph': [thing, intangible, structured_value, brand]}
        self.assertSetEqual(
            generate_schema_org_code.lookup_parents(brand, schema, {}),
            set(['Thing', 'Intangible', 'Brand']))

    def test_get_root_type_thing(self):
        thing = {'@id': schema_org_id('Thing')}
        intangible = {
            '@id': schema_org_id('Intangible'),
            'rdfs:subClassOf': thing
        }
        structured_value = {
            '@id': schema_org_id('StructuredValue'),
            'rdfs:subClassOf': intangible
        }
        schema = {'@graph': [thing, intangible, structured_value]}

        self.assertEqual(
            generate_schema_org_code.get_root_type(structured_value, schema),
            thing)

    def test_get_root_type_datatype(self):
        number = {
            '@id': schema_org_id('Number'),
            '@type': [schema_org_id('DataType'), 'rdfs:Class']
        }
        integer = {'@id': schema_org_id('Integer'), 'rdfs:subClassOf': number}
        schema = {'@graph': [integer, number]}

        self.assertEqual(
            generate_schema_org_code.get_root_type(integer, schema), number)

    def test_get_root_type_enum(self):
        thing = {'@id': schema_org_id('Thing')}
        intangible = {
            '@id': schema_org_id('Intangible'),
            'rdfs:subClassOf': thing
        }
        enumeration = {
            '@id': schema_org_id('Enumeration'),
            'rdfs:subClassOf': intangible
        }
        actionStatusType = {
            '@id': schema_org_id('ActionStatusType'),
            'rdfs:subClassOf': enumeration
        }
        schema = {'@graph': [thing, intangible, enumeration, actionStatusType]}

        self.assertEqual(
            generate_schema_org_code.get_root_type(actionStatusType, schema),
            actionStatusType)

    def test_parse_property_identifier(self):
        thing = {'@id': schema_org_id('Thing')}
        intangible = {
            '@id': schema_org_id('Intangible'),
            'rdfs:subClassOf': thing
        }
        structured_value = {
            '@id': schema_org_id('StructuredValue'),
            'rdfs:subClassOf': intangible
        }
        property_value = {
            '@id': schema_org_id('PropertyValue'),
            'rdfs:subClassOf': structured_value
        }
        number = {
            '@id': schema_org_id('Number'),
            '@type': [schema_org_id('DataType'), 'rdfs:Class']
        }
        integer = {'@id': schema_org_id('Integer'), 'rdfs:subClassOf': number}
        identifier = {
            '@id': schema_org_id('Identifier'),
            schema_org_id('rangeIncludes'): [property_value, integer, number]
        }
        schema = {
            '@graph': [
                thing, intangible, structured_value, property_value, number,
                integer, identifier
            ]
        }

        names = {"http://schema.org/Identifier": 1234, "Identifier": 1235}

        self.assertEqual(
            generate_schema_org_code.parse_property(identifier, schema, names),
            {
                'name': 'Identifier',
                'name_hash': 1235,
                'has_number': True,
                'thing_types': [property_value['@id']],
                'enum_types': []
            })


if __name__ == '__main__':
    unittest.main()
