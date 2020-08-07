#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access
import os
import unittest
from xml.dom import minidom

import generate_jacoco_report

from pylib.constants import host_paths

_BUILD_UTILS_PATH = os.path.join(host_paths.DIR_SOURCE_ROOT, 'build', 'android',
                                 'gyp')
with host_paths.SysPath(_BUILD_UTILS_PATH, 0):
  from util import build_utils

# Does not work as """ string because of newlines indent whitespace in
# the xml parseString().
DEVICE_XML = (
    '<report name="Fake_Device_report">'
    '<sessioninfo id="123" start="456" dump="789"/>'
    '<package name="package1">'
    '<class name="class1" sourcefilename="class1.java">'
    # The coverage in these methods is less than the coverage in the HOST_XML.
    # Will make sure the higher coverage is set in the combined report.
    '<method name="method1" desc="method1_desc" line="19">'
    '<counter type="INSTRUCTION" missed="7" covered="0"/>'
    '<counter type="BRANCH" missed="2" covered="0"/>'
    '<counter type="LINE" missed="2" covered="0"/>'
    '<counter type="COMPLEXITY" missed="2" covered="0"/>'
    '<counter type="METHOD" missed="1" covered="0"/>'
    '</method>'
    '<method name="method2" desc="method2_desc" line="15">'
    '<counter type="INSTRUCTION" missed="8" covered="0"/>'
    '<counter type="BRANCH" missed="2" covered="0"/>'
    '<counter type="LINE" missed="1" covered="0"/>'
    '<counter type="COMPLEXITY" missed="2" covered="0"/>'
    '<counter type="METHOD" missed="1" covered="0"/>'
    '</method>'
    '<counter type="INSTRUCTION" missed="15" covered="0"/>'
    '<counter type="BRANCH" missed="4" covered="0"/>'
    '<counter type="LINE" missed="3" covered="0"/>'
    '<counter type="COMPLEXITY" missed="4" covered="0"/>'
    '<counter type="METHOD" missed="2" covered="0"/>'
    '<counter type="CLASS" missed="1" covered="0"/>'
    '</class>'
    # This class is not in the HOST_XML, will make sure it's still in the
    # combined report.
    '<class name="class2" sourcefilename="class2.java"/>'
    # This source file matches the one in HOST_XML.
    '<sourcefile name="class1.java">'
    # nr15 matches the HOST_XML but has less coverage. nr19/20 are not in
    # HOST_XML.
    '<line nr="15" mi="9" ci="0" mb="2" cb="0"/>'
    '<line nr="19" mi="5" ci="0" mb="2" cb="0"/>'
    '<line nr="20" mi="1" ci="1" mb="0" cb="0"/>'
    '<counter type="INSTRUCTION" missed="15" covered="1"/>'
    '<counter type="BRANCH" missed="4" covered="0"/>'
    '<counter type="LINE" missed="3" covered="0"/>'
    '<counter type="COMPLEXITY" missed="4" covered="0"/>'
    '<counter type="METHOD" missed="2" covered="0"/>'
    '<counter type="CLASS" missed="1" covered="0"/>'
    '</sourcefile>'
    '<counter type="INSTRUCTION" missed="15" covered="1"/>'
    '<counter type="BRANCH" missed="4" covered="0"/>'
    '<counter type="LINE" missed="3" covered="0"/>'
    '<counter type="COMPLEXITY" missed="4" covered="0"/>'
    '<counter type="METHOD" missed="2" covered="0"/>'
    '<counter type="CLASS" missed="1" covered="0"/>'
    '</package>'
    # This device package 3 contains more coverage in its sourcefile
    # than the one in HOST_XML. We make sure its coverage is chosen.
    '<package name="package3">'
    '<class name="class5" sourcefilename="class1.java">'
    '<method name="method1" desc="method1_desc" line="19">'
    '<counter type="INSTRUCTION" missed="100" covered="100"/>'
    '<counter type="BRANCH" missed="100" covered="100"/>'
    '<counter type="LINE" missed="100" covered="100"/>'
    '<counter type="COMPLEXITY" missed="100" covered="100"/>'
    '<counter type="METHOD" missed="100" covered="100"/>'
    '</method>'
    '<method name="method2" desc="method2_desc" line="15">'
    '<counter type="INSTRUCTION" missed="100" covered="100"/>'
    '<counter type="BRANCH" missed="100" covered="100"/>'
    '<counter type="LINE" missed="100" covered="100"/>'
    '<counter type="COMPLEXITY" missed="100" covered="100"/>'
    '<counter type="METHOD" missed="100" covered="100"/>'
    '</method>'
    '</class>'
    '<class name="class20" sourcefilename="class20.java"/>'
    '<sourcefile name="sourcefile_xxx1">'
    '<line nr="1" mi="100" ci="100" mb="100" cb="100"/>'
    '<line nr="12" mi="50" ci="50" mb="50" cb="50"/>'
    '<line nr="123" mi="50" ci="50" mb="50" cb="50"/>'
    '<counter type="INSTRUCTION" missed="200" covered="200"/>'
    '<counter type="BRANCH" missed="200" covered="200"/>'
    '<counter type="LINE" missed="200" covered="200"/>'
    '<counter type="COMPLEXITY" missed="200" covered="200"/>'
    '<counter type="METHOD" missed="200" covered="200"/>'
    '<counter type="CLASS" missed="1" covered="1"/>'
    '</sourcefile>'
    '<counter type="INSTRUCTION" missed="200" covered="200"/>'
    '<counter type="BRANCH" missed="200" covered="200"/>'
    '<counter type="LINE" missed="200" covered="200"/>'
    '<counter type="COMPLEXITY" missed="200" covered="200"/>'
    '<counter type="METHOD" missed="200" covered="200"/>'
    '<counter type="CLASS" missed="0" covered="1"/>'
    '</package>'
    # These counters don't matter, but would ideally be a sum of all the
    # source file counters, which should match the sum of all the method
    # coverages
    '<counter type="INSTRUCTION" missed="15" covered="1"/>'
    '<counter type="BRANCH" missed="4" covered="0"/>'
    '<counter type="LINE" missed="3" covered="0"/>'
    '<counter type="COMPLEXITY" missed="4" covered="0"/>'
    '<counter type="METHOD" missed="2" covered="0"/>'
    '<counter type="CLASS" missed="1" covered="0"/>'
    '</report>')
HOST_XML = (
    '<report name="Fake_Device_report">'
    '<sessioninfo id="789" start="456" dump="123"/>'
    '<package name="package1">'
    # This class and methods match those in DEVICE_XML, but have higher
    # coverage numbers so we'll check they make it into the combined
    # report.
    '<class name="class1" sourcefilename="class1.java">'
    '<method name="method1" desc="method1_desc" line="19">'
    '<counter type="INSTRUCTION" missed="6" covered="1"/>'
    '<counter type="BRANCH" missed="1" covered="1"/>'
    '<counter type="LINE" missed="1" covered="1"/>'
    '<counter type="COMPLEXITY" missed="1" covered="1"/>'
    '<counter type="METHOD" missed="0" covered="1"/>'
    '</method>'
    '<method name="method2" desc="method2_desc" line="15">'
    '<counter type="INSTRUCTION" missed="7" covered="1"/>'
    '<counter type="BRANCH" missed="1" covered="1"/>'
    '<counter type="LINE" missed="0" covered="1"/>'
    '<counter type="COMPLEXITY" missed="1" covered="1"/>'
    '<counter type="METHOD" missed="0" covered="1"/>'
    '</method>'
    # Method 3 is not in DEVICE_XML, we'll make sure it's in the
    # combined report.
    '<method name="method3" desc="method3_desc" line="70"/>'
    # This is a total of the above method coverage. The numbers don't actually
    # matter as the combination report resums them.
    '<counter type="INSTRUCTION" missed="13" covered="2"/>'
    '<counter type="BRANCH" missed="2" covered="2"/>'
    '<counter type="LINE" missed="1" covered="2"/>'
    '<counter type="COMPLEXITY" missed="2" covered="2"/>'
    '<counter type="METHOD" missed="0" covered="2"/>'
    '<counter type="CLASS" missed="0" covered="1"/>'
    '</class>'
    # This class 3 is not in the other package so we make sure it is
    # present after the combination.
    '<class name="not_in_dev_package" sourcefilename="class3.java"/>'
    '<sourcefile name="class1.java">'
    # nr15 has higher coverage than DEVICE_XML's nr15, we'll make sure
    # the combined report uses it.
    '<line nr="15" mi="7" ci="2" mb="1" cb="1"/>'
    # nr190/200 are not in DEVICE_XML. We'll make sure they're included
    # in the combined report.
    '<line nr="190" mi="5" ci="0" mb="1" cb="1"/>'
    '<line nr="200" mi="1" ci="0" mb="0" cb="0"/>'
    # These counters get summed after the report combination so don't
    # really matter for us, but normally they would reflect the totals
    # from the above 3 lines of mi,ci,cb,mb.
    '<counter type="INSTRUCTION" missed="13" covered="2"/>'
    '<counter type="BRANCH" missed="2" covered="2"/>'
    '<counter type="LINE" missed="1" covered="2"/>'
    '<counter type="COMPLEXITY" missed="2" covered="2"/>'
    '<counter type="METHOD" missed="0" covered="2"/>'
    '<counter type="CLASS" missed="0" covered="1"/>'
    '</sourcefile>'
    # This source file is not found in DEVICE_XML. We'll want to make
    # sure it's in the combined report, and that it's coverage
    # numbers are included.
    '<sourcefile name="new sourcefile.java">'
    '<line nr="1" mi="5" ci="6" mb="2" cb="3"/>'
    '<line nr="2" mi="3" ci="4" mb="1" cb="2"/>'
    '<line nr="3" mi="100" ci="0" mb="0" cb="0"/>'
    '<counter type="INSTRUCTION" missed="108" covered="10"/>'
    '<counter type="BRANCH" missed="3" covered="5"/>'
    '<counter type="LINE" missed="0" covered="0"/>'
    '<counter type="COMPLEXITY" missed="0" covered="0"/>'
    '<counter type="METHOD" missed="1" covered="3"/>'
    '<counter type="CLASS" missed="0" covered="0"/>'
    '</sourcefile>'
    '<counter type="INSTRUCTION" missed="13" covered="2"/>'
    '<counter type="BRANCH" missed="2" covered="2"/>'
    '<counter type="LINE" missed="1" covered="2"/>'
    '<counter type="COMPLEXITY" missed="2" covered="2"/>'
    '<counter type="METHOD" missed="0" covered="2"/>'
    '<counter type="CLASS" missed="0" covered="1"/>'
    '</package>'
    '<package name="not_in_device_xml"/>'
    '<package name="package3">'
    '<class name="class5" sourcefilename="class1.java">'
    '<method name="method1" desc="method1_desc" line="19">'
    '<counter type="INSTRUCTION" missed="100" covered="100"/>'
    '<counter type="BRANCH" missed="100" covered="100"/>'
    '<counter type="LINE" missed="100" covered="100"/>'
    '<counter type="COMPLEXITY" missed="100" covered="100"/>'
    '<counter type="METHOD" missed="100" covered="100"/>'
    '</method>'
    '<method name="method2" desc="method2_desc" line="15">'
    '<counter type="INSTRUCTION" missed="100" covered="100"/>'
    '<counter type="BRANCH" missed="100" covered="100"/>'
    '<counter type="LINE" missed="100" covered="100"/>'
    '<counter type="COMPLEXITY" missed="100" covered="100"/>'
    '<counter type="METHOD" missed="100" covered="100"/>'
    '</method>'
    '</class>'
    '<class name="class20" sourcefilename="class20.java"/>'
    # This sourcefile has coverage less than that of DEVICE_XML so we
    # want to ensure that DEVICE_XML's coverage numbers are used.
    '<sourcefile name="sourcefile_xxx1">'
    '<line nr="1" mi="50" ci="50" mb="50" cb="50"/>'
    '<line nr="12" mi="50" ci="50" mb="50" cb="50"/>'
    '<line nr="123" mi="50" ci="50" mb="50" cb="50"/>'
    # These coverage numbers don't matter as the report sums up new
    # ones.
    '<counter type="INSTRUCTION" missed="150" covered="150"/>'
    '<counter type="BRANCH" missed="150" covered="150"/>'
    '<counter type="LINE" missed="200" covered="200"/>'
    '<counter type="COMPLEXITY" missed="200" covered="200"/>'
    '<counter type="METHOD" missed="200" covered="200"/>'
    '<counter type="CLASS" missed="1" covered="1"/>'
    '</sourcefile>'
    # These coverage numbers don't matter as the report sums up new
    # ones.
    '<counter type="INSTRUCTION" missed="200" covered="200"/>'
    '<counter type="BRANCH" missed="200" covered="200"/>'
    '<counter type="LINE" missed="200" covered="200"/>'
    '<counter type="COMPLEXITY" missed="200" covered="200"/>'
    '<counter type="METHOD" missed="200" covered="200"/>'
    '<counter type="CLASS" missed="0" covered="1"/>'
    '</package>'
    # These coverage numbers don't matter as the report sums up new
    # ones.
    '<counter type="INSTRUCTION" missed="163" covered="327"/>'
    '<counter type="BRANCH" missed="207" covered="207"/>'
    '<counter type="LINE" missed="1" covered="2"/>'
    '<counter type="COMPLEXITY" missed="2" covered="2"/>'
    '<counter type="METHOD" missed="205" covered="205"/>'
    '<counter type="CLASS" missed="0" covered="1"/>'
    '</report>')


class GenerateJacocoReportTest(unittest.TestCase):
  """Tests for _GenerateJacocoReport.
  """

  def setUp(self):
    super(GenerateJacocoReportTest, self).setUp()
    # Need to reparse every test as some functions modify the tree inplace.
    self.dev_tree = minidom.parseString(DEVICE_XML)
    self.host_tree = minidom.parseString(HOST_XML)
    self.dev_root_node = self.dev_tree.getElementsByTagName('report')[0]
    self.host_root_node = self.host_tree.getElementsByTagName('report')[0]

  def verify_counters(self, node, expected_num_of_counters, answer_dict):
    # Takes a node, the expected counters for the node, and answer dict.
    # answer_dict maps an instruction to a tuple of covered and missed lines.
    counters = generate_jacoco_report._GetCountersList(node)
    self.assertEqual(len(counters), expected_num_of_counters)
    counter_map = generate_jacoco_report._CreateCounterMap(counters)
    for key in answer_dict:
      covered, missed = answer_dict[key]
      self.assertEqual(counter_map[key].getAttribute('covered'), covered)
      self.assertEqual(counter_map[key].getAttribute('missed'), missed)

  def testAddMissingNodes(self):
    device_packages = self.dev_root_node.getElementsByTagName('package')
    host_packages = self.host_root_node.getElementsByTagName('package')
    device_dict = generate_jacoco_report._CreateAttributeToObjectDict(
        device_packages, 'name')
    host_dict = generate_jacoco_report._CreateAttributeToObjectDict(
        host_packages, 'name')

    self.assertEqual(len(device_dict), 2)
    self.assertEqual(len(host_dict), 3)
    self.assertNotIn('not_in_device_xml', device_dict)
    generate_jacoco_report._AddMissingNodes(device_dict, host_dict,
                                            self.dev_root_node, 'name')
    self.assertEqual(len(device_dict), 3)
    self.assertIn('not_in_device_xml', device_dict)
    self.assertEqual(len(self.dev_root_node.getElementsByTagName('package')), 3)

  def testCombineXmlFiles(self):
    with build_utils.TempDir() as temp_dir:
      temp_device_f = os.path.join(temp_dir, 'temp_device')
      temp_host_f = os.path.join(temp_dir, 'temp_host')
      temp_result_f = os.path.join(temp_dir, 'temp_result')
      with open(temp_device_f, 'w') as dev_f, open(temp_host_f, 'w') as host_f:
        dev_f.write(DEVICE_XML)
        host_f.write(HOST_XML)

      generate_jacoco_report._CombineXmlFiles(temp_result_f, temp_device_f,
                                              temp_host_f)
      result_tree = minidom.parse(temp_result_f)
      result_root = result_tree.getElementsByTagName('report')[0]
      report_ans_dict = {
          'instruction': ('213', '327'),
          'branch': ('207', '207'),
          'method': ('205', '201'),
      }
      self.verify_counters(result_root, 6, report_ans_dict)

      self.assertEqual(len(result_root.getElementsByTagName('package')), 3)
      result_package = result_root.getElementsByTagName('package')[0]
      classes = result_package.getElementsByTagName('class')
      self.assertEqual(len(classes), 3)
      result_class_dict = generate_jacoco_report._CreateAttributeToObjectDict(
          classes, 'name')
      for key in result_class_dict:
        expected_classes = {'class1', 'class2', 'not_in_dev_package'}
        self.assertIn(key, expected_classes)

      class_ans_dict = {
          'instruction': ('2', '13'),
          'branch': ('2', '2'),
          'method': ('2', '0'),
      }
      self.verify_counters(result_class_dict['class1'], 6, class_ans_dict)

      methods = result_package.getElementsByTagName('method')
      self.assertEqual(len(methods), 3)
      result_method_dict = generate_jacoco_report._CreateAttributeToObjectDict(
          methods, 'name')
      for key in result_method_dict:
        expected_methods = {'method1', 'method2', 'method3'}
        self.assertIn(key, expected_methods)

      method_ans_dict = {
          'instruction': ('1', '6'),
          'branch': ('1', '1'),
          'line': ('1', '1'),
      }
      self.verify_counters(result_method_dict['method1'], 5, method_ans_dict)

      self.assertEqual(len(result_root.getElementsByTagName('sourcefile')), 3)
      source_file_node = result_root.getElementsByTagName('sourcefile')[0]
      source_ans_dict = {
          'instruction': ('3', '19'),
          'branch': ('2', '4'),
          'line': ('2', '1'),
          'class': ('1', '0'),
      }
      self.verify_counters(source_file_node, 6, source_ans_dict)

  def testCreateClassfileArgs(self):
    class_files = ['b17.plane', 'a10.plane', 'gato.sub', 'balao.sub', 'A.bomb']
    answer = [
        '--classfiles', 'b17.plane', '--classfiles', 'a10.plane',
        '--classfiles', 'gato.sub', '--classfiles', 'balao.sub', '--classfiles',
        'A.bomb'
    ]
    self.assertEqual(
        generate_jacoco_report._CreateClassfileArgs(class_files, ''), answer)
    self.assertEqual(
        generate_jacoco_report._CreateClassfileArgs(class_files, 'not_found'),
        answer)
    answer = [
        '--classfiles', 'gato.sub', '--classfiles', 'balao.sub', '--classfiles',
        'A.bomb'
    ]
    self.assertEqual(
        generate_jacoco_report._CreateClassfileArgs(class_files, 'plane'),
        answer)

  def testGetCountersList(self):
    node_ls = generate_jacoco_report._GetCountersList(self.dev_root_node)
    self.assertEqual(len(node_ls), 6)

    node = self.dev_root_node.childNodes[1]
    answers = [6, 6, 5]
    for ans in answers:
      node_ls = generate_jacoco_report._GetCountersList(node)
      self.assertEqual(len(node_ls), ans)
      node = node.firstChild

  def testCreateAttributeToObjectDict(self):
    device_packages = self.dev_root_node.getElementsByTagName('package')
    device_dict = generate_jacoco_report._CreateAttributeToObjectDict(
        device_packages, 'name')
    self.assertEqual(len(device_dict), 2)
    self.assertIn('package1', device_dict)
    self.assertIn('package3', device_dict)

  def testGetCounterTotalsForTagName(self):
    host_package = self.host_root_node.getElementsByTagName('package')[0]
    host_class = host_package.getElementsByTagName('class')[0]
    total_dicts = generate_jacoco_report._GetCounterTotalsForTagName(
        host_class, 'method')
    self.assertEqual(total_dicts['instruction']['covered'], 2)
    self.assertEqual(total_dicts['instruction']['missed'], 13)
    self.assertEqual(total_dicts['line']['covered'], 2)
    self.assertEqual(total_dicts['line']['missed'], 1)
    self.assertEqual(total_dicts['method']['covered'], 2)
    self.assertEqual(total_dicts['method']['missed'], 0)
    self.assertEqual(total_dicts['branch']['covered'], 2)
    self.assertEqual(total_dicts['branch']['missed'], 2)

  def testGetDictForEachElement(self):
    dev_package = self.dev_root_node.getElementsByTagName('package')[0]
    host_package = self.host_root_node.getElementsByTagName('package')[0]
    dev_dict, host_dict = generate_jacoco_report._GetDictForEachElement(
        dev_package, host_package, 'class', 'name')
    self.assertEqual(len(dev_dict), 2)
    self.assertEqual(len(host_dict), 2)
    self.assertIn('class1', dev_dict)
    self.assertIn('class2', dev_dict)
    self.assertIn('not_in_dev_package', host_dict)
    self.assertNotIn('not_in_dev_package', dev_dict)
    self.assertNotIn('class2', host_dict)
    dev_class_node = dev_dict['class1']
    host_class_node = host_dict['class1']

    # Verifies the node objects class1 is mapping to the ones we expected.
    dev_dict, host_dict = generate_jacoco_report._GetDictForEachElement(
        dev_class_node, host_class_node, 'method', 'line')
    self.assertEqual(len(dev_dict), 2)
    self.assertEqual(len(host_dict), 3)
    self.assertTrue('15' in dev_dict)
    self.assertTrue('19' in dev_dict)
    self.assertTrue('15' in host_dict)

  def testCreateTotalDicts(self):
    dicts = generate_jacoco_report._CreateTotalDicts()
    self.assertEqual(len(dicts),
                     len(generate_jacoco_report._JAVA_COVERAGE_METRICS))
    for metric in generate_jacoco_report._JAVA_COVERAGE_METRICS:
      self.assertIn(metric, dicts)

  def testSetHigherCounter(self):
    dev_package = self.dev_root_node.getElementsByTagName('package')[0]
    host_package = self.host_root_node.getElementsByTagName('package')[0]
    ans_dict = {'instruction': ('1', '15')}
    self.verify_counters(dev_package, 6, ans_dict)
    ans_dict = {'instruction': ('2', '13')}
    self.verify_counters(host_package, 6, ans_dict)

    dev_package_counters = generate_jacoco_report._GetCountersList(dev_package)
    host_package_counters = generate_jacoco_report._GetCountersList(
        host_package)
    dev_counter_map = generate_jacoco_report._CreateCounterMap(
        dev_package_counters)
    host_counter_map = generate_jacoco_report._CreateCounterMap(
        host_package_counters)
    for metric in dev_counter_map:
      generate_jacoco_report._SetHigherCounter(dev_counter_map[metric],
                                               host_counter_map[metric])
    ans_dict = {'instruction': ('2', '13')}
    self.verify_counters(dev_package, 6, ans_dict)
    ans_dict = {'instruction': ('2', '13')}
    self.verify_counters(host_package, 6, ans_dict)

  def testUpdateAllNodesToHigherCoverage(self):
    dev_package = self.dev_root_node.getElementsByTagName('package')[0]
    host_package = self.host_root_node.getElementsByTagName('package')[0]
    dev_name_to_package_dict = {'package1': dev_package}
    host_name_to_package_dict = {'package1': host_package}
    self.assertEqual(len(dev_package.getElementsByTagName('class')), 2)
    methods = dev_package.getElementsByTagName('method')
    ans_dict = {
        'instruction': ('0', '7'),
        'branch': ('0', '2'),
        'method': ('0', '1')
    }
    self.verify_counters(methods[0], 5, ans_dict)

    generate_jacoco_report._UpdateAllNodesToHigherCoverage(
        dev_name_to_package_dict, host_name_to_package_dict)
    self.assertEqual(len(dev_package.getElementsByTagName('class')), 3)
    self.assertEqual(len(methods), 2)
    ans_dict = {
        'instruction': ('1', '6'),
        'branch': ('1', '1'),
        'method': ('1', '0')
    }
    self.verify_counters(methods[0], 5, ans_dict)

  def testUpdateCountersFromTotal(self):
    dev_package = self.dev_root_node.getElementsByTagName('package')[0]
    dev_package_counters = generate_jacoco_report._GetCountersList(dev_package)
    dev_counter_map = generate_jacoco_report._CreateCounterMap(
        dev_package_counters)
    total_dicts = generate_jacoco_report._CreateTotalDicts()

    # Sanity check to make sure attribute will have changed.
    self.assertEqual(dev_counter_map['instruction'].getAttribute('missed'),
                     '15')

    for key in total_dicts:
      total_dicts[key]['covered'] = '5'
      total_dicts[key]['missed'] = '1337'
    generate_jacoco_report._UpdateCountersFromTotal(dev_package_counters,
                                                    total_dicts)
    for key in dev_counter_map:
      counter = dev_counter_map[key]
      covered, missed = generate_jacoco_report._GetCoveredAndMissedFromCounter(
          counter)
      self.assertEqual(covered, '5')
      self.assertEqual(missed, '1337')

  def testUpdatePackageSourceFiles(self):
    dev_package = self.dev_root_node.getElementsByTagName('package')[0]
    host_package = self.host_root_node.getElementsByTagName('package')[0]
    dev_source_node = dev_package.getElementsByTagName('sourcefile')[0]
    dev_line_dict = generate_jacoco_report._CreateAttributeToObjectDict(
        dev_source_node.getElementsByTagName('line'), 'nr')
    self.assertEqual(len(dev_line_dict), 3)
    self.assertIn('19', dev_line_dict)

    # The nr in host_package are now added to the dev_package.s
    generate_jacoco_report._UpdatePackageSourceFiles(dev_package, host_package)
    dev_line_dict = generate_jacoco_report._CreateAttributeToObjectDict(
        dev_source_node.getElementsByTagName('line'), 'nr')
    self.assertEqual(len(dev_line_dict), 5)
    for num in [15, 19, 20, 190, 200]:
      self.assertIn(str(num), dev_line_dict)

  def testUpdateSourceFileCounters(self):
    dev_package = self.dev_root_node.getElementsByTagName('package')[0]
    host_package = self.host_root_node.getElementsByTagName('package')[0]
    dev_source_node = dev_package.getElementsByTagName('sourcefile')[0]
    host_source_node = host_package.getElementsByTagName('sourcefile')[0]
    dev_counter_dict = generate_jacoco_report._CreateAttributeToObjectDict(
        dev_source_node.getElementsByTagName('counter'), 'type')
    self.assertEqual(dev_counter_dict['INSTRUCTION'].getAttribute('covered'),
                     '1')
    self.assertEqual(dev_counter_dict['INSTRUCTION'].getAttribute('missed'),
                     '15')
    self.assertEqual(dev_counter_dict['LINE'].getAttribute('covered'), '0')
    self.assertEqual(dev_counter_dict['LINE'].getAttribute('missed'), '3')

    total_dict = {'ci': 101, 'mi': 202, 'cb': 303, 'mb': 404}
    generate_jacoco_report._UpdateSourceFileCounters(dev_source_node,
                                                     host_source_node,
                                                     total_dict)
    self.assertEqual(dev_counter_dict['INSTRUCTION'].getAttribute('covered'),
                     '101')
    self.assertEqual(dev_counter_dict['INSTRUCTION'].getAttribute('missed'),
                     '202')
    self.assertEqual(dev_counter_dict['BRANCH'].getAttribute('covered'), '303')
    self.assertEqual(dev_counter_dict['BRANCH'].getAttribute('missed'), '404')
    self.assertEqual(dev_counter_dict['LINE'].getAttribute('covered'), '2')
    self.assertEqual(dev_counter_dict['LINE'].getAttribute('missed'), '1')


if __name__ == '__main__':
  unittest.main()
