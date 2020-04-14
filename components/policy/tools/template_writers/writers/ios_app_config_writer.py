#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from xml.dom import minidom
from writers import xml_formatted_writer


def GetWriter(config):
  '''Factory method for instanciating the IOSAppConfigWriter. Every Writer needs
  a GetWriter method because the TemplateFormatter uses this method to
  instantiate a Writer.
  '''
  return IOSAppConfigWriter(['ios'], config)  # platforms unused


class IOSAppConfigWriter(xml_formatted_writer.XMLFormattedWriter):
  '''Simple writer that writes app_config.xml files.
  '''

  def CreateDocument(self):
    dom_impl = minidom.getDOMImplementation('')
    return dom_impl.createDocument('http://www.w3.org/2001/XMLSchema-instance',
                                   'managedAppConfiguration', None)

  def BeginTemplate(self):
    self._app_config.attributes[
        'xmlns:xsi'] = 'http://www.w3.org/2001/XMLSchema-instance'
    schema_location = '/%s/appconfig/appconfig.xsd' % (self.config['bundle_id'])
    self._app_config.attributes[
        'xsi:noNamespaceSchemaLocation'] = schema_location

    version = self.AddElement(self._app_config, 'version', {})
    self.AddText(version, self.config['version'])

    bundle_id = self.AddElement(self._app_config, 'bundleId', {})
    self.AddText(bundle_id, self.config['bundle_id'])
    self._policies = self.AddElement(self._app_config, 'dict', {})

  def WritePolicy(self, policy):
    element_type = self.policy_type_to_xml_tag[policy['type']]
    if element_type:
      self.AddElement(self._policies, element_type, {'keyName': policy['name']})

  def Init(self):
    self._doc = self.CreateDocument()
    self._app_config = self._doc.documentElement
    self.policy_type_to_xml_tag = {
        'string': 'string',
        'int': 'integer',
        'int-enum': 'integer',
        'string-enum': 'string',
        'string-enum-list': 'stringArray',
        'main': 'boolean',
        'list': 'stringArray',
        'dict': 'string',
    }

  def GetTemplateText(self):
    return self.ToPrettyXml(self._doc)
