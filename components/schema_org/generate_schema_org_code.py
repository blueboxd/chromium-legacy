# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates C++ code representing structured data objects from schema.org

This script generates C++ objects based on a JSON+LD schema file. Blink uses the
generated code to scrape schema.org data from web pages.
"""

import argparse
import json
import sys
import os

_current_dir = os.path.dirname(os.path.realpath(__file__))
# jinja2 is in chromium's third_party directory
# Insert at front to override system libraries, and after path[0] == script dir
sys.path.insert(
    1, os.path.join(_current_dir, *([os.pardir] * 2 + ['third_party'])))
import jinja2
from jinja2 import Environment, PackageLoader, select_autoescape
env = Environment(loader=PackageLoader('generate_schema_org_code', ''))


def object_name_from_id(schema_org_id):
    """Get the object name from a schema.org ID."""
    return schema_org_id[len('http://schema.org/'):]


def get_template_vars(schema_file_path):
    """Read the needed template variables from the schema file."""
    template_vars = {'entities': [], 'properties': []}

    with open(schema_file_path) as schema_file:
        schema = json.loads(schema_file.read())

    for thing in schema['@graph']:
        if thing['@type'] == 'rdfs:Class':
            template_vars['entities'].append(object_name_from_id(thing['@id']))
        elif thing['@type'] == 'rdf:Property':
            template_vars['properties'].append(
                object_name_from_id(thing['@id']))

    template_vars['entities'].sort()
    template_vars['properties'].sort()

    return template_vars


def generate_file(file_name, template_file, template_vars):
    """Generate and write file given a template and variables to render."""
    template_vars['header_file'] = os.path.basename(
        template_file[template_file.index('.')])
    template_vars['header_guard'] = template_vars['header_file'].upper() + '_H'
    with open(file_name, 'w') as f:
        f.write(env.get_template(template_file).render(template_vars))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--schema-file',
        help='Schema.org schema file to use for code generation.')
    parser.add_argument(
        '--output-dir',
        help='Output directory in which to place generated code files.')
    parser.add_argument('--templates', nargs='+')
    args = parser.parse_args()

    template_vars = get_template_vars(args.schema_file)
    for template_file in args.templates:
        generate_file(
            os.path.join(args.output_dir,
                         os.path.basename(template_file.replace('.tmpl', ''))),
            template_file, template_vars)


if __name__ == '__main__':
    sys.exit(main())
