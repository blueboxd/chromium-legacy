# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from xml_validations import UkmXmlValidation
from xml.dom import minidom


class UkmXmlValidationTest(unittest.TestCase):

  def toUkmConfig(self, xml_string):
    dom = minidom.parseString(xml_string)
    [ukm_config] = dom.getElementsByTagName('ukm-configuration')
    return ukm_config

  def testEventsHaveOwners(self):
    ukm_config = self.toUkmConfig("""
        <ukm-configuration>
          <event name="Event1">
            <owner>dev@chromium.org</owner>
          </event>
        </ukm-configuration>
        """.strip())
    validator = UkmXmlValidation(ukm_config)
    success, errors = validator.checkEventsHaveOwners()
    self.assertTrue(success)
    self.assertListEqual([], errors)

  def testEventsMissingOwners(self):
    ukm_config = self.toUkmConfig("""
        <ukm-configuration>
          <event name="Event1"/>
          <event name="Event2">
            <owner></owner>
          </event>
          <event name="Event3">
            <owner>johndoe</owner>
          </event>
        </ukm-configuration>
        """.strip())
    expected_errors = [
        "<owner> tag is required for event 'Event1'.",
        "<owner> tag for event 'Event2' should not be empty.",
        "<owner> tag for event 'Event3' expects a Chromium or Google email "
        "address.",
    ]

    validator = UkmXmlValidation(ukm_config)
    success, errors = validator.checkEventsHaveOwners()
    self.assertFalse(success)
    self.assertListEqual(expected_errors, errors)

  def testMetricHasUndefinedEnum(self):
    ukm_config = self.toUkmConfig("""
        <ukm-configuration>
          <event name="Event1">
            <metric name="Metric2" enum="FeatureObserver"/>
          </event>
          <event name="Event2">
            <metric name="Metric1" enum="BadEnum"/>
            <metric name="Metric2" enum="FeatureObserver"/>
            <metric name="Metric3" unit="ms"/>
            <metric name="Metric4"/>
          </event>
        </ukm-configuration>
        """.strip())
    expected_errors = [
        "Unknown enum BadEnum in ukm metric Event2:Metric1.",
    ]

    expected_warnings = [
        "Warning: Neither 'enum' or 'unit' is specified for ukm metric "
        "Event2:Metric4.",
    ]

    validator = UkmXmlValidation(ukm_config)
    success, errors, warnings = validator.checkMetricTypeIsSpecified()
    self.assertFalse(success)
    self.assertListEqual(expected_errors, errors)
    self.assertListEqual(expected_warnings, warnings)

if __name__ == '__main__':
  unittest.main()
