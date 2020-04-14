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


class IOSAppConfigWriterUnitTests(writer_unittest_common.WriterUnittestCommon):
  '''Unit tests for IOSAppConfigWriter.'''

  def _GetTestPolicyTemplate(self, policy_name, policy_type):
    return '''
{
  'policy_definitions': [
    {
      'name': '%s',
      'type': '%s',
      'supported_on':['ios:80-'],
      'caption': '',
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
''' % (policy_name, policy_type)

  def _GetExpectedOutput(self, version, tag):
    if tag:
      policies = '<dict>\n    %s\n  </dict>' % tag
    else:
      policies = '<dict/>'

    return '''<?xml version="1.0" ?>
<managedAppConfiguration xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="/com.google.chrome/appconfig/appconfig.xsd">
  <version>%s</version>
  <bundleId>com.google.chrome</bundleId>
  %s
</managedAppConfiguration>''' % (version, policies)

  def testStringPolicy(self):
    policy_json = self._GetTestPolicyTemplate('StringPolicy', 'string')
    expected = self._GetExpectedOutput('83.0.4089.0',
                                       '<string keyName="StringPolicy"/>')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())

  def testIntPolicy(self):
    policy_json = self._GetTestPolicyTemplate('IntPolicy', 'int')
    expected = self._GetExpectedOutput('83.0.4089.0',
                                       '<integer keyName="IntPolicy"/>')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())

  def testIntEnumPolicy(self):
    policy_json = self._GetTestPolicyTemplate('IntEnumPolicy', 'int-enum')
    expected = self._GetExpectedOutput('83.0.4089.0',
                                       '<integer keyName="IntEnumPolicy"/>')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())

  def testStringEnumPolicy(self):
    policy_json = self._GetTestPolicyTemplate('StringEnumPolicy', 'string-enum')
    expected = self._GetExpectedOutput('83.0.4089.0',
                                       '<string keyName="StringEnumPolicy"/>')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())

  def testStringEnumListPolicy(self):
    policy_json = self._GetTestPolicyTemplate('StringEnumListPolicy',
                                              'string-enum-list')
    expected = self._GetExpectedOutput(
        '83.0.4089.0', '<stringArray keyName="StringEnumListPolicy"/>')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())

  def testBooleanPolicy(self):
    policy_json = self._GetTestPolicyTemplate('BooleanPolicy', 'main')
    expected = self._GetExpectedOutput('83.0.4089.0',
                                       '<boolean keyName="BooleanPolicy"/>')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())

  def testListPolicy(self):
    policy_json = self._GetTestPolicyTemplate('ListPolicy', 'list')
    expected = self._GetExpectedOutput('83.0.4089.0',
                                       '<stringArray keyName="ListPolicy"/>')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())

  def testDictPolicy(self):
    policy_json = self._GetTestPolicyTemplate('DictPolicy', 'dict')
    # Dict policies are not supported by the appconfig.xml format, therefore
    # they are treated as JSON strings.
    expected = self._GetExpectedOutput('83.0.4089.0',
                                       '<string keyName="DictPolicy"/>')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())


if __name__ == '__main__':
  unittest.main()
