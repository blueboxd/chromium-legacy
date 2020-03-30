#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from writers import template_writer


def GetWriter(config):
  '''Factory method for creating JamfWriter objects.
  See the constructor of TemplateWriter for description of
  arguments.
  '''
  return JamfWriter(['mac'], config)


class JamfWriter(template_writer.TemplateWriter):
  '''Simple writer that writes a jamf.json file.
  '''

  TYPE_TO_INPUT = {
      'string': 'string',
      'int': 'integer',
      'int-enum': 'integer',
      'string-enum': 'string',
      'string-enum-list': 'array',
      'main': 'boolean',
      'list': 'array',
      'dict': 'dictionary',
      'external': 'dictionary',
  }

  def WriteTemplate(self, template):
    '''Writes the given template definition.

    Args:
      template: Template definition to write.

    Returns:
      Generated output for the passed template definition.
    '''
    policies = [
        policy for policy in template['policy_definitions']
        if policy['type'] != 'group' and self.IsPolicySupported(policy)
    ]
    output = {
        'title': self.config['bundle_id'],
        'description': self.config['app_name'],
        'options': {
            'remove_empty_properties': True
        },
        'properties': {}
    }

    for policy in policies:
      output['properties'][policy['name']] = {
          'title': policy['name'],
          'description': policy['caption'],
          'type': self.TYPE_TO_INPUT[policy['type']]
      }

    return json.dumps(output, indent=2, sort_keys=True, separators=(',', ': '))
