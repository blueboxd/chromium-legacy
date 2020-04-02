# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates C++ code representing structured data objects from schema.org

This script generates C++ objects based on a JSON+LD schema file. Blink uses the
generated code to scrape schema.org data from web pages.
"""

import os
import sys
import json
import argparse

_current_dir = os.path.dirname(os.path.realpath(__file__))
# jinja2 is in chromium's third_party directory
# Insert at front to override system libraries, and after path[0] == script dir
sys.path.insert(
    1, os.path.join(_current_dir, *([os.pardir] * 2 + ['third_party'])))
import jinja2
from jinja2 import Environment, PackageLoader, select_autoescape
env = Environment(loader=PackageLoader('generate_schema_org_code', ''))
env.trim_blocks = True
env.lstrip_blocks = True

SCHEMA_ORG_PREFIX = 'http://schema.org/'


def schema_org_id(object_name):
    return SCHEMA_ORG_PREFIX + object_name


def object_name_from_id(the_id):
    """Get the object name from a schema.org ID."""
    return the_id[len(SCHEMA_ORG_PREFIX):]


def get_schema_obj(obj_id, schema):
    """Search the schema graph for an object with the given ID."""
    matches = [obj for obj in schema['@graph'] if obj['@id'] == obj_id]
    return matches[0] if len(matches) == 1 else None


def is_enum_type(class_obj):
    if 'rdfs:subClassOf' in class_obj:
        parent_class = class_obj['rdfs:subClassOf']
        if isinstance(parent_class, list):
            return any(parent['@id'] == schema_org_id('Enumeration')
                       for parent in parent_class)
        return parent_class['@id'] == schema_org_id('Enumeration')


def find_enum_options(obj_id, schema):
    return [
        object_name_from_id(obj['@id']) for obj in schema['@graph']
        if obj['@type'] == obj_id
    ]


def get_root_type(the_class, schema):
    """Get the base type the class is descended from."""
    class_obj = get_schema_obj(the_class['@id'], schema)
    if class_obj is None:
        return the_class

    if class_obj['@id'] == schema_org_id('Thing'):
        return class_obj
    # Consider URLs to be a base type as we will use have a struct field for
    # them specifically.
    if class_obj['@id'] == schema_org_id('URL'):
        return class_obj
    if ('@type' in class_obj
            and schema_org_id('DataType') in class_obj['@type']):
        return class_obj
    if 'rdfs:subClassOf' in class_obj:
        parent_class = class_obj['rdfs:subClassOf']
        # All classes that use multiple inheritance are Thing type.
        if isinstance(parent_class, list):
            return get_schema_obj(schema_org_id('Thing'), schema)
        # Enumeration classes are treated specially. Return the specific type
        # of enum this class is.
        if parent_class['@id'] == schema_org_id('Enumeration'):
            return class_obj
        return get_root_type(parent_class, schema)
    return class_obj


def parse_property(prop, schema):
    """Parse out details about the property, including what type it can be."""
    parsed_prop = {
        'name': object_name_from_id(prop['@id']),
        'thing_types': [],
        'enum_types': []
    }

    if not schema_org_id('rangeIncludes') in prop:
        return parsed_prop

    rangeIncludes = prop[schema_org_id('rangeIncludes')]
    if not isinstance(rangeIncludes, list):
        rangeIncludes = [rangeIncludes]

    for possible_type in rangeIncludes:
        root_type = get_root_type(possible_type, schema)
        if root_type['@id'] == schema_org_id('Thing'):
            parsed_prop['thing_types'].append(possible_type['@id'])
        elif root_type['@id'] == schema_org_id('Text'):
            parsed_prop['has_text'] = True
        elif root_type['@id'] == schema_org_id('Date'):
            parsed_prop['has_date'] = True
        elif root_type['@id'] == schema_org_id('Time'):
            parsed_prop['has_time'] = True
        elif root_type['@id'] == schema_org_id('Boolean'):
            parsed_prop['has_boolean'] = True
        elif root_type['@id'] == schema_org_id('Number'):
            parsed_prop['has_number'] = True
        elif root_type['@id'] == schema_org_id('DateTime'):
            parsed_prop['has_date_time'] = True
        elif root_type['@id'] == schema_org_id('URL'):
            parsed_prop['has_url'] = True
        elif is_enum_type(root_type):
            parsed_prop['enum_types'].append(possible_type['@id'])
    return parsed_prop


def merge_with_schema(schema, overrides, thing):
    indices = [
        i for i, x in enumerate(schema['@graph']) if x['@id'] == thing['@id']
    ]
    for index in indices:
        schema['@graph'][index] = thing
    if not indices:
        schema['@graph'].append(thing)


def get_template_vars(schema_file_path, overrides_file_path):
    """Read the needed template variables from the schema file."""
    template_vars = {'entities': [], 'properties': [], 'enums': []}

    with open(schema_file_path) as schema_file:
        schema = json.loads(schema_file.read())

    if overrides_file_path:
        with open(overrides_file_path) as overrides_file:
            overrides = json.loads(overrides_file.read())
        for thing in overrides['@graph']:
            merge_with_schema(schema, overrides, thing)

    for thing in schema['@graph']:
        if thing['@type'] == 'rdfs:Class':
            template_vars['entities'].append(object_name_from_id(thing['@id']))
            if is_enum_type(thing):
                template_vars['enums'].append({
                    'name':
                    object_name_from_id(thing['@id']),
                    'id':
                    thing['@id'],
                    'options':
                    find_enum_options(thing['@id'], schema)
                })
        elif thing['@type'] == 'rdf:Property':
            template_vars['properties'].append(parse_property(thing, schema))

    template_vars['entities'].sort()
    template_vars['properties'].sort(key=lambda p: p['name'])

    return template_vars


def generate_file(file_name, template_file, template_vars):
    """Generate and write file given a template and variables to render."""
    template_vars['header_file'] = os.path.basename(
        template_file[:template_file.index('.')])
    template_vars['header_guard'] = template_vars['header_file'].upper() + '_H'
    with open(file_name, 'w') as f:
        f.write(env.get_template(template_file).render(template_vars))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--schema-file',
        help='Schema.org JSON-LD schema file to use for code generation.')
    parser.add_argument(
        '--overrides-file',
        help='JSON-LD schema file with overrides to support changes not in the '
        'latest schema.org version. Optional.')
    parser.add_argument(
        '--output-dir',
        help='Output directory in which to place generated code files.')
    parser.add_argument('--templates', nargs='+')
    args = parser.parse_args()

    template_vars = get_template_vars(args.schema_file, args.overrides_file)
    for template_file in args.templates:
        generate_file(
            os.path.join(args.output_dir,
                         os.path.basename(template_file.replace('.tmpl', ''))),
            template_file, template_vars)


if __name__ == '__main__':
    sys.exit(main())
