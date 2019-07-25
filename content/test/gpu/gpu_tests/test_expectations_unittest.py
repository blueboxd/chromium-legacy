# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gpu_project_config
import inspect
import itertools
import mock
import os
import tempfile
import unittest

from collections import defaultdict

from gpu_tests import gpu_helper
from gpu_tests import gpu_integration_test
from gpu_tests import pixel_integration_test
from gpu_tests import pixel_test_pages
from gpu_tests import webgl_conformance_integration_test
from gpu_tests import webgl_test_util

from py_utils import discover

from typ import expectations_parser
from typ import json_results

OS_CONDITIONS = ['win', 'mac', 'android']
GPU_CONDITIONS = ['amd', 'arm', 'broadcom', 'hisilicon', 'intel', 'imagination',
                  'nvidia', 'qualcomm', 'vivante']
WIN_CONDITIONS = ['xp', 'vista', 'win7', 'win8', 'win10']
MAC_CONDITIONS = ['leopard', 'snowleopard', 'lion', 'mountainlion',
                  'mavericks', 'yosemite', 'sierra', 'highsierra', 'mojave']
# These aren't expanded out into "lollipop", "marshmallow", etc.
ANDROID_CONDITIONS = ['l', 'm', 'n', 'o', 'p', 'q']
GENERIC_CONDITIONS = OS_CONDITIONS + GPU_CONDITIONS

_map_specific_to_generic = {sos:'win' for sos in WIN_CONDITIONS}
_map_specific_to_generic.update({sos:'mac' for sos in MAC_CONDITIONS})
_map_specific_to_generic.update({sos:'android' for sos in ANDROID_CONDITIONS})

_get_generic = lambda tags: set(
    [_map_specific_to_generic.get(tag, tag) for tag in tags])

ResultType = json_results.ResultType


def _IsCollision(s1, s2, tag_sets):
  # s1 and s2 do not collide when there exists a tag in s1 and tag in s2 that
  # are from the same tag declaration set and are not equal to each other
  # and one tag is not a generic tag that covers the other.
  _generic_and_specific_tags_collide = lambda t1, t2: (
      t1 in GENERIC_CONDITIONS and t2 in _map_specific_to_generic and
      _map_specific_to_generic[t2] == t1)
  for tag_set in tag_sets:
    for t1, t2 in itertools.product(s1, s2):
      if (t1 in tag_set and t2 in tag_set and t1 != t2 and
          not _generic_and_specific_tags_collide(t1, t2) and
          not _generic_and_specific_tags_collide(t2, t1)):
        return False
  return True


def _IsDriverTagDuplicated(s1, s2, test_class, tag_sets):
  if not test_class:
    return False

  def classify_tags(tags):
    driver_tag = None
    other_tags = []
    for tag in tags:
      if test_class.MatchDriverTag(tag):
        assert not driver_tag
        driver_tag = tag
      else:
        other_tags.append(tag)
    return driver_tag, other_tags

  driver_tag1, other_tags1 = classify_tags(s1)
  driver_tag2, other_tags2 = classify_tags(s2)
  if not driver_tag1 and not driver_tag2:
    return False
  if not _IsCollision(other_tags1, other_tags2, tag_sets):
    return False
  if driver_tag1 == driver_tag2 or (not driver_tag1 or not driver_tag2):
    return True

  match = test_class.MatchDriverTag(driver_tag1)
  tag1 = (match.group(1), match.group(2), match.group(3))
  match = test_class.MatchDriverTag(driver_tag2)
  tag2 = (match.group(1), match.group(2), match.group(3))
  if not tag1[0] == tag2[0]:
    return False

  operation1, version1 = tag1[1:]
  operation2, version2 = tag2[1:]
  if operation1 == 'ne':
    return not (operation2 == 'eq' and version1 == version2)
  elif operation2 == 'ne':
    return not (operation1 == 'eq' and version1 == version2)
  elif operation1 == 'eq':
    return test_class.EvaluateVersionComparison(
        version1, operation2, version2)
  elif operation2 == 'eq':
    return test_class.EvaluateVersionComparison(
        version2, operation1, version1)

  if operation1 == 'ge' or operation1 == 'gt':
    if operation2 == 'ge' or operation2 == 'gt':
      return True
  elif operation1 == 'le' or operation1 == 'lt':
    if operation2 == 'le' or operation2 == 'lt':
      return True

  if operation1 == 'ge':
    if operation2 == 'le':
      return not test_class.EvaluateVersionComparison(version1, 'gt', version2)
    elif operation2 == 'lt':
      return not test_class.EvaluateVersionComparison(version1, 'ge', version2)
  elif operation1 == 'gt':
    return not test_class.EvaluateVersionComparison(version1, 'ge', version2)
  elif operation1 == 'le':
    if operation2 == 'ge':
      return not test_class.EvaluateVersionComparison(version1, 'lt', version2)
    elif operation2 == 'gt':
      return not test_class.EvaluateVersionComparison(version1, 'le', version2)
  elif operation1 == 'lt':
    return not test_class.EvaluateVersionComparison(version1, 'le', version2)
  else:
    assert False


def _ExtractUnitTestTestExpectations(file_name):
  file_name = os.path.join(os.path.dirname(os.path.abspath(__file__)),
      '..', 'unittest_data', 'test_expectations', file_name)
  test_expectations_list = []
  with open(file_name, 'r') as test_data:
    test_expectations = ''
    reach_end = False
    while not reach_end:
      line = test_data.readline()
      if line:
        line = line.strip()
        if line:
          test_expectations += line + '\n'
          continue
      else:
        reach_end = True

      if test_expectations:
        test_expectations_list.append(test_expectations)
        test_expectations = ''

  assert test_expectations_list
  return test_expectations_list


def _MapGpuDevicesToVendors(tag_sets):
  for tag_set in tag_sets:
    if any(gpu in tag_set for gpu in GPU_CONDITIONS):
      _map_specific_to_generic.update(
          {t[0]: t[1] for t in
           itertools.permutations(tag_set, 2) if t[0].startswith(t[1] + '-')})
      break


def CheckTestExpectationsAreForExistingTests(
    test_class, mock_options, test_names=None):
  test_names = test_names or [
      args[0] for args in
      test_class.GenerateGpuTests(mock_options)]
  expectations_file = test_class.ExpectationsFiles()[0]
  trie = {}
  for test in test_names:
    _trie = trie.setdefault(test[0], {})
    for l in test[1:]:
      _trie = _trie.setdefault(l, {})
    _trie.setdefault('$', {})
  f = open(expectations_file, 'r')
  expectations_file = os.path.basename(expectations_file)
  expectations = f.read()
  f.close()
  parser = expectations_parser.TaggedTestListParser(expectations)
  for exp in parser.expectations:
    _trie = trie
    is_glob = False
    for l in exp.test:
      if l == '*':
        is_glob = True
        break
      assert l in _trie, (
        "%s:%d: Pattern '%s' does not match with any tests in the %s test suite"
        % (expectations_file, exp.lineno, exp.test, test_class.Name()))
      _trie = _trie[l]
    assert is_glob or '$' in _trie, (
        "%s:%d: Pattern '%s' does not match with any tests in the %s test suite"
        % (expectations_file, exp.lineno, exp.test, test_class.Name()))


def CheckTestExpectationPatternsForCollision(
    expectations, file_name, test_class=None):
  """This function makes sure that any test expectations that have the same
  pattern do not collide with each other. They collide when one expectation's
  tags are a subset of the other expectation's tags. If the expectation with
  the larger tag set is active during a test run, then the expectation's whose
  tag set is a subset of the tags will be active as well.

  Args:
  expectations: A string containing test expectations in the new format
  file_name: Name of the file that the test expectations came from
  """
  parser = expectations_parser.TaggedTestListParser(expectations)
  tests_to_exps = defaultdict(list)
  master_conflicts_found = False
  error_msg = ''
  _MapGpuDevicesToVendors(parser.tag_sets)
  for exp in parser.expectations:
    tests_to_exps[exp.test].append(exp)
  for pattern, exps in tests_to_exps.items():
    conflicts_found = False
    for e1, e2 in itertools.combinations(exps, 2):
      if (_IsCollision(e1.tags, e2.tags, parser.tag_sets) or
          _IsCollision(e2.tags, e1.tags, parser.tag_sets) or
          _IsDriverTagDuplicated(
              e1.tags, e2.tags, test_class, parser.tag_sets)):
        if not conflicts_found:
          error_msg += ('\n\nFound conflicts for test %s in %s:\n' %
                        (pattern, file_name))
        master_conflicts_found = conflicts_found = True
        error_msg += ('  line %d conflicts with line %d\n' %
                      (e1.lineno, e2.lineno))
  assert not master_conflicts_found, error_msg


def _TestCheckTestExpectationsAreForExistingTests(expectations):
  options = gpu_helper.GetMockArgs()
  expectations_file = tempfile.NamedTemporaryFile(delete=False)
  expectations_file.write(expectations)
  expectations_file.close()
  gpu_tests = gpu_integration_test.GpuIntegrationTest
  with mock.patch.object(
      gpu_tests, 'GenerateGpuTests', return_value=[('a/b/c', ())]):
    with mock.patch.object(
        gpu_tests, 'ExpectationsFiles', return_value=[expectations_file.name]):
      CheckTestExpectationsAreForExistingTests(gpu_tests, options)


def _CheckPatternIsValid(pattern):
  if not '*' in pattern and not 'WebglExtension_' in pattern:
    full_path = os.path.normpath(os.path.join(
        webgl_test_util.conformance_path, pattern))
    if not os.path.exists(full_path):
      raise Exception('The WebGL conformance test path specified in ' +
        'expectation does not exist: ' + full_path)


def _FindTestCases():
  test_cases = []
  for start_dir in gpu_project_config.CONFIG.start_dirs:
    modules_to_classes = discover.DiscoverClasses(
        start_dir,
        gpu_project_config.CONFIG.top_level_dir,
        base_class=gpu_integration_test.GpuIntegrationTest)
    test_cases.extend(modules_to_classes.values())
  return test_cases


class GpuTestExpectationsValidation(unittest.TestCase):
  def testNoCollisionsForSameTestsInGpuTestExpectations(self):
    webgl_conformance_test_class = (
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest)
    for test_case in _FindTestCases():
      if 'gpu_tests.gpu_integration_test_unittest' not in test_case.__module__:
        for i in xrange(1, 2 + (test_case == webgl_conformance_test_class)):
          _ = list(test_case.GenerateGpuTests(
              gpu_helper.GetMockArgs(webgl_version=('%d.0.0' % i))))
          if test_case.ExpectationsFiles():
            with open(test_case.ExpectationsFiles()[0]) as f:
              CheckTestExpectationPatternsForCollision(f.read(),
              os.path.basename(f.name))

  def testGpuTestExpectationsAreForExistingTests(self):
    options = gpu_helper.GetMockArgs()
    for test_case in _FindTestCases():
      if 'gpu_tests.gpu_integration_test_unittest' not in test_case.__module__:
        if (test_case.Name() not in ('pixel', 'webgl_conformance')
            and test_case.ExpectationsFiles()):
          CheckTestExpectationsAreForExistingTests(test_case, options)

  def testWebglTestPathsExist(self):
    webgl_test_class = (
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest)
    for test_case in _FindTestCases():
      if test_case == webgl_test_class:
        for i in xrange(1, 3):
          _ = list(test_case.GenerateGpuTests(
              gpu_helper.GetMockArgs(webgl_version='%d.0.0' % i)))
          with open(test_case.ExpectationsFiles()[0], 'r') as f:
            expectations = expectations_parser.TaggedTestListParser(f.read())
            for exp in expectations.expectations:
              _CheckPatternIsValid(exp.test)

  def testPixelTestsExpectationsAreForExistingTests(self):
    pixel_test_names = []
    for _, method in inspect.getmembers(
        pixel_test_pages.PixelTestPages, predicate=inspect.isfunction):
      pixel_test_names.extend(
          [p.name for p in method(
              pixel_integration_test.PixelIntegrationTest.test_base_name)])
    CheckTestExpectationsAreForExistingTests(
        pixel_integration_test.PixelIntegrationTest,
        gpu_helper.GetMockArgs(), pixel_test_names)

  def testWebglTestExpectationsForDriverTags(self):
    webgl_conformance_test_class = (
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest)
    expectations_driver_tags = set()
    for i in range(1, 3):
      _ = list(webgl_conformance_test_class.GenerateGpuTests(
          gpu_helper.GetMockArgs(webgl_version=('%d.0.0' % i))))
      with open(webgl_conformance_test_class.ExpectationsFiles()[0], 'r') as f:
        parser = expectations_parser.TaggedTestListParser(f.read())
        driver_tag_set = set()
        for tag_set in parser.tag_sets:
          if webgl_conformance_test_class.MatchDriverTag(list(tag_set)[0]):
            for tag in tag_set:
              assert webgl_conformance_test_class.MatchDriverTag(tag)
            assert not driver_tag_set
            driver_tag_set = tag_set
          else:
            for tag in tag_set:
              assert not webgl_conformance_test_class.MatchDriverTag(tag)
        expectations_driver_tags |= driver_tag_set

    self.assertEqual(
        webgl_conformance_test_class.ExpectationsDriverTags(),
        expectations_driver_tags)

class TestGpuTestExpectationsValidators(unittest.TestCase):
  def testCollisionInTestExpectationsWithGpuDriverTags(self):
    webgl_conformance_test_class = (
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest)
    failed_test_expectations = _ExtractUnitTestTestExpectations(
        'failed_test_expectations_with_driver_tags.txt')
    for test_expectations in failed_test_expectations:
      with self.assertRaises(AssertionError):
        CheckTestExpectationPatternsForCollision(
            test_expectations, 'test.txt', webgl_conformance_test_class)

  def testNoCollisionInTestExpectationsWithGpuDriverTags(self):
    webgl_conformance_test_class = (
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest)
    passed_test_expectations = _ExtractUnitTestTestExpectations(
        'passed_test_expectations_with_driver_tags.txt')
    for test_expectations in passed_test_expectations:
      CheckTestExpectationPatternsForCollision(
          test_expectations, 'test.txt', webgl_conformance_test_class)

  def testCollisionsBetweenAngleAndNonAngleConfiguration(self):
    test_expectations = '''
    # tags: [ android ]
    # tags: [ qualcomm-adreno-(tm)-418 ]
    # tags: [ opengles ]
    # results: [ RetryOnFailure Skip ]
    [ android qualcomm-adreno-(tm)-418 ] a/b/c/d [ RetryOnFailure ]
    [ android opengles ] a/b/c/d [ Skip ]
    '''
    with self.assertRaises(AssertionError):
      CheckTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testCollisionInTestExpectationsWithSpecifcAndGenericOsTags(self):
    test_expectations = '''# tags: [ mac win linux xp ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    # results: [ Failure Skip ]
    [ intel win ] a/b/c/d [ Failure ]
    [ intel xp debug ] a/b/c/d [ Skip ]
    '''
    with self.assertRaises(AssertionError):
      CheckTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testNoCollisionBetweenSpecificOsTags(self):
    test_expectations = '''# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    # results: [ Failure Skip ]
    [ intel win7 ] a/b/c/d [ Failure ]
    [ intel xp debug ] a/b/c/d [ Skip ]
    '''
    CheckTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testCollisionInTestExpectationsWithGpuVendorAndDeviceTags(self):
    test_expectations = '''# tags: [ mac win linux xp ]
    # tags: [ intel amd nvidia nvidia-0x01 ]
    # tags: [ debug release ]
    # results: [ Failure Skip ]
    [ nvidia win ] a/b/c/d [ Failure ]
    [ nvidia-0x01 xp debug ] a/b/c/d [ Skip ]
    '''
    with self.assertRaises(AssertionError):
      CheckTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testCollisionInTestExpectationsWithGpuVendorAndDeviceTags2(self):
    test_expectations = '''# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia nvidia-0x01 nvidia-0x02 ]
    # tags: [ debug release ]
    # results: [ Failure Skip ]
    [ nvidia-0x01 win ] a/b/c/* [ Failure ]
    [ nvidia win7 debug ] a/b/c/* [ Skip ]
    '''
    with self.assertRaises(AssertionError):
      CheckTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testNoCollisionBetweenGpuDeviceTags(self):
    test_expectations = '''# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia nvidia-0x01 nvidia-0x02 ]
    # tags: [ debug release ]
    # results: [ Failure Skip ]
    [ nvidia-0x01 win7 ] a/b/c/d [ Failure ]
    [ nvidia-0x02 win7 debug ] a/b/c/d [ Skip ]
    [ nvidia win debug ] a/b/c/* [ Skip ]
    '''
    CheckTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testMixGenericandSpecificTagsInCollidingSets(self):
    test_expectations = '''# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia nvidia-0x01 ]
    # tags: [ debug release ]
    # results: [ Failure Skip ]
    [ nvidia-0x01 win ] a/b/c/d [ Failure ]
    [ nvidia win7 debug ] a/b/c/d [ Skip ]
    '''
    with self.assertRaises(AssertionError):
      CheckTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testCollisionInTestExpectationCausesAssertion(self):
    test_expectations = '''# tags: [ mac win linux ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    # results: [ Failure Skip RetryOnFailure ]
    [ intel win ] a/b/c/d [ Failure ]
    [ intel win debug ] a/b/c/d [ Skip ]
    [ intel  ] a/b/c/d [ Failure ]
    [ amd mac ] a/b [ RetryOnFailure ]
    [ mac ] a/b [ Skip ]
    [ amd mac ] a/b/c [ Failure ]
    [ intel mac ] a/b/c [ Failure ]
    '''
    with self.assertRaises(AssertionError) as context:
      CheckTestExpectationPatternsForCollision(test_expectations, 'test.txt')
    self.assertIn("Found conflicts for test a/b/c/d in test.txt:",
      str(context.exception))
    self.assertIn('line 5 conflicts with line 6',
      str(context.exception))
    self.assertIn('line 5 conflicts with line 7',
      str(context.exception))
    self.assertIn('line 6 conflicts with line 7',
      str(context.exception))
    self.assertIn("Found conflicts for test a/b in test.txt:",
      str(context.exception))
    self.assertIn('line 8 conflicts with line 9',
      str(context.exception))
    self.assertNotIn("Found conflicts for test a/b/c in test.txt:",
      str(context.exception))

  def testNoCollisionInTestExpectations(self):
    test_expectations = '''# tags: [ mac win linux ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    # results: [ Failure ]
    [ intel win release ] a/b/* [ Failure ]
    [ intel debug ] a/b/c/d [ Failure ]
    [ nvidia debug ] a/b/c/d [ Failure ]
    '''
    CheckTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testExpectationPatternNotInGeneratedTests(self):
    with self.assertRaises(AssertionError) as context:
      _TestCheckTestExpectationsAreForExistingTests(
          '# results: [ Failure ]\na/b/d [ Failure ]')
    self.assertIn(("2: Pattern 'a/b/d' does not match with any"
                  " tests in the GpuIntegrationTest test suite"),
                  str(context.exception))

  def testExpectationPatternIsShorterThanAnyTestName(self):
    with self.assertRaises(AssertionError) as context:
      _TestCheckTestExpectationsAreForExistingTests(
          '# results: [ Failure ]\na/b [ Failure ]')
    self.assertIn(("2: Pattern 'a/b' does not match with any"
                  " tests in the GpuIntegrationTest test suite"),
                  str(context.exception))

  def testGlobMatchesTestName(self):
    _TestCheckTestExpectationsAreForExistingTests(
        '# results: [ Failure ]\na/b* [ Failure ]')
