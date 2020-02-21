# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import template_expander


class DocumentPolicyFeatureWriter(json5_generator.Writer):
    file_basename = 'document_policy_features'

    def __init__(self, json5_file_path, output_dir):
        super(DocumentPolicyFeatureWriter, self).__init__(json5_file_path, output_dir)

        def parse_default_value(default_value, value_type):
            """ Parses default_value string to actual usable C++ expression.
            @param default_value_str: default_value field specified in document_policy_features.json5
            @param value_type: value_type field specified in document_policy_features.json5
            """
            policy_value_type = "mojom::PolicyValueType::k{}".format(value_type)

            if default_value == 'max':
                return "PolicyValue::CreateMaxPolicyValue({})".format(policy_value_type)
            if default_value == 'min':
                return "PolicyValue::CreateMinPolicyValue({})".format(policy_value_type)

            if value_type in {'bool'}:  # types that have only one corresponding PolicyValueType
                return "PolicyValue({})".format(default_value)
            else:
                return "PolicyValue({}, {})".format(default_value, policy_value_type)


        @template_expander.use_jinja('templates/' + self.file_basename + '.cc.tmpl')
        def generate_implementation():
            return {
                'header_guard': self.make_header_guard(self._relative_output_dir + self.file_basename + '.h'),
                'input_files': self._input_files,
                'features': self.json5_file.name_dictionaries,
                'parse_default_value': parse_default_value
            }

        self._outputs = {
            self.file_basename + '.cc': generate_implementation,
        }


if __name__ == '__main__':
    json5_generator.Maker(DocumentPolicyFeatureWriter).main()
