#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../../../..'))

import unittest

from writers import writer_unittest_common


class JamfWriterUnitTests(writer_unittest_common.WriterUnittestCommon):
  '''Unit tests for JamfWriter.'''

  def _GetTestPolicyTemplate(self, policy_name, policy_type, policy_captio):
    return '''
{
  'policy_definitions': [
    {
      'name': '%s',
      'type': '%s',
      'supported_on':['chrome.mac:*-'],
      'caption': '%s',
      'desc': '',
      'items': [{
        'name': '',
          'value': 1,
          'caption': '',
        }]
    },
  ],
  'policy_atomic_group_definitions': [],
  'placeholders': [],
  'messages': {},
}
''' % (policy_name, policy_type, policy_captio)

  def _GetExpectedOutput(self, policy_name, policy_type, policy_captio):
    return '''{
  "description": "Google Chrome",
  "options": {
    "remove_empty_properties": true
  },
  "properties": {
    "%s": {
      "description": "%s",
      "title": "%s",
      "type": "%s"
    }
  },
  "title": "com.google.chrome"
}''' % (policy_name, policy_captio, policy_name, policy_type)

  def testStringPolicy(self):
    policy_json = self._GetTestPolicyTemplate('stringPolicy', 'string',
                                              'A string policy')
    expected = self._GetExpectedOutput('stringPolicy', 'string',
                                       'A string policy')
    output = self.GetOutput(policy_json, {'_google_chrome': '1'}, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testIntPolicy(self):
    policy_json = self._GetTestPolicyTemplate('intPolicy', 'int',
                                              'An int policy')
    expected = self._GetExpectedOutput('intPolicy', 'integer', 'An int policy')
    output = self.GetOutput(policy_json, {'_google_chrome': '1'}, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testIntEnumPolicy(self):
    policy_json = self._GetTestPolicyTemplate('intPolicy', 'int-enum',
                                              'An int-enum policy')
    expected = self._GetExpectedOutput('intPolicy', 'integer',
                                       'An int-enum policy')
    output = self.GetOutput(policy_json, {'_google_chrome': '1'}, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testStringEnumPolicy(self):
    policy_json = self._GetTestPolicyTemplate('stringPolicy', 'string-enum',
                                              'A string-enum policy')
    expected = self._GetExpectedOutput('stringPolicy', 'string',
                                       'A string-enum policy')
    output = self.GetOutput(policy_json, {'_google_chrome': '1'}, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testStringEnumListPolicy(self):
    policy_json = self._GetTestPolicyTemplate(
        'stringPolicy', 'string-enum-list', 'A string-enum-list policy')
    expected = self._GetExpectedOutput('stringPolicy', 'array',
                                       'A string-enum-list policy')
    output = self.GetOutput(policy_json, {'_google_chrome': '1'}, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testBooleanPolicy(self):
    policy_json = self._GetTestPolicyTemplate('booleanPolicy', 'main',
                                              'A boolean policy')
    expected = self._GetExpectedOutput('booleanPolicy', 'boolean',
                                       'A boolean policy')
    output = self.GetOutput(policy_json, {'_google_chrome': '1'}, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testListPolicy(self):
    policy_json = self._GetTestPolicyTemplate('listPolicy', 'list',
                                              'A list policy')
    expected = self._GetExpectedOutput('listPolicy', 'array', 'A list policy')
    output = self.GetOutput(policy_json, {'_google_chrome': '1'}, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testDictPolicy(self):
    policy_json = self._GetTestPolicyTemplate('dictPolicy', 'dict',
                                              'A dict policy')
    expected = self._GetExpectedOutput('dictPolicy', 'dictionary',
                                       'A dict policy')
    output = self.GetOutput(policy_json, {'_google_chrome': '1'}, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testExternalPolicy(self):
    policy_json = self._GetTestPolicyTemplate('externalPolicy', 'external',
                                              'A external policy')
    expected = self._GetExpectedOutput('externalPolicy', 'dictionary',
                                       'A external policy')
    output = self.GetOutput(policy_json, {'_google_chrome': '1'}, 'jamf')
    self.assertEquals(output.strip(), expected.strip())


if __name__ == '__main__':
  unittest.main()
