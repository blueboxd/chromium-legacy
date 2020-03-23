# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Generate of table of APIs and their attributes based on the WebIDL database.

The columns of the table are as follows:

  * interface      : Name of interface.
  * name           : Member name. Will be empty for interfaces.
  * entity_type    : One of 'interface', 'namespace', 'const', 'attribute',
                     'operation', 'constructor', 'stringifier', 'iterable',
                     'maplike', 'setlike', 'dictionary'
  * idl_type       : Type of the object. For function-like entries, this is the
                     type of the return value. Note that the type is stripped
                     of nullable unions.
  * syntactic_form : Human readable idl_type.
  * use_counter    : If usage is being measured, this is the UseCounter.
  * secure_context : 'True' if the [SecureContext] extended attribute is
                     present. Empty otherwise.
  * high_entropy   : 'True' if the [HighEntropy] extended attribute is present.
                     Empty otherwise.
"""

import optparse
from io import BytesIO
from csv import DictWriter

from utilities import write_file
from v8_utilities import capitalize
import web_idl


def parse_options():
    parser = optparse.OptionParser(usage="%prog [options]")
    parser.add_option(
        "--web_idl_database",
        type="string",
        help="filepath of the input database")
    parser.add_option(
        "--output", type="string", help="filepath of output file")
    options, args = parser.parse_args()

    required_option_names = ("web_idl_database", "output")
    for opt_name in required_option_names:
        if getattr(options, opt_name) is None:
            parser.error("--{} is a required option.".format(opt_name))

    return options, args


def get_idl_type_name(idl_type):
    assert isinstance(idl_type, web_idl.IdlType)
    unwrapped_type = idl_type.unwrap()
    return unwrapped_type.type_name, unwrapped_type.syntactic_form


def true_or_nothing(v):
    return 'True' if v else ''


def record(csv_writer, entity):
    interface = ''
    name = ''
    entity_type = ''
    use_counter = ''
    secure_context = ''
    high_entropy = ''
    idl_type = ''
    syntactic_form = ''

    secure_context = ('SecureContext' in entity.extended_attributes)
    high_entropy = ('HighEntropy' in entity.extended_attributes)

    if 'Measure' in entity.extended_attributes:
        use_counter = capitalize(entity.identifier)
        if not isinstance(entity, web_idl.Interface):
            use_counter = (
                capitalize(entity.owner.identifier) + '_' + use_counter)
    elif 'MeasureAs' in entity.extended_attributes:
        use_counter = entity.extended_attributes.value_of('MeasureAs')

    if isinstance(entity, web_idl.Interface):
        interface = entity.identifier
        name = ''
        entity_type = 'interface'
    else:
        interface = entity.owner.identifier
        name = entity.identifier

        if isinstance(entity, web_idl.Attribute):
            entity_type = 'attribute'
            idl_type, syntactic_form = get_idl_type_name(entity.idl_type)
        elif isinstance(entity, web_idl.Operation):
            entity_type = 'operation'
            idl_type, syntactic_form = get_idl_type_name(entity.return_type)
        elif isinstance(entity, web_idl.Constant):
            entity_type = 'constant'
            idl_type, syntactic_form = get_idl_type_name(entity.idl_type)
        else:
            assert False, "Unexpected IDL construct"
    csv_writer.writerow({
        'interface': interface,
        'name': name,
        'entity_type': entity_type,
        'idl_type': idl_type,
        'syntactic_form': syntactic_form,
        'use_counter': use_counter,
        'secure_context': true_or_nothing(secure_context),
        'high_entropy': true_or_nothing(high_entropy)
    })


def main():
    options, _ = parse_options()
    with BytesIO() as out_buffer:
        csv_writer = DictWriter(
            out_buffer,
            fieldnames=[
                'interface', 'name', 'entity_type', 'idl_type',
                'syntactic_form', 'use_counter', 'secure_context',
                'high_entropy'
            ])
        csv_writer.writeheader()

        web_idl_database = web_idl.Database.read_from_file(
            options.web_idl_database)
        for interface in web_idl_database.interfaces:
            record(csv_writer, interface)
            for entity in (interface.attributes + interface.operations +
                           interface.constants):
                record(csv_writer, entity)
        write_file(out_buffer.getvalue(), options.output)


if __name__ == '__main__':
    main()
