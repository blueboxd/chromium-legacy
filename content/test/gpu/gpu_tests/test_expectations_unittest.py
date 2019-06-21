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

def _ConflictLeaksRegression(possible_collision, exp):
  reason_template = (
      'Pattern \'{0}\' on line {1} has the %s expectation however the '
      'the pattern on \'{2}\' line {3} has the Pass expectation'). format(
          possible_collision.test, possible_collision.lineno, exp.test,
          exp.lineno)
  causes_regression = not(
      ResultType.Failure in exp.results or ResultType.Skip in exp.results)
  if (ResultType.Skip in possible_collision.results and
      causes_regression):
    return reason_template % 'Skip'
  if (ResultType.Failure in possible_collision.results and
      causes_regression):
    return reason_template % 'Failure'
  return ''


def _IsCollision(s1, s2):
  # s1 collides with s2 when s1 is a subset of s2
  # A tag is in both sets if its a generic tag
  # and its in one set while a specifc tag covered by the generic tag is in
  # the other set.
  for tag in s1:
    if (not tag in s2 and not (
        tag in GENERIC_CONDITIONS and
        any(_map_specific_to_generic.get(t, t) == tag for t in s2)) and
        not _map_specific_to_generic.get(tag, tag) in s2):
      return False
  return True


def _MapGpuDevicesToVendors(tag_sets):
  for tag_set in tag_sets:
    if any(gpu in tag_set for gpu in GPU_CONDITIONS):
      _map_specific_to_generic.update(
          {t[0]: t[1] for t in
           itertools.permutations(tag_set, 2) if (t[0] + '-').startswith(t[1])})
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
  f = open(expectations_file, 'r')
  expectations_file = os.path.basename(expectations_file)
  expectations = f.read()
  f.close()
  parser = expectations_parser.TaggedTestListParser(expectations)
  for exp in parser.expectations:
    _trie = trie
    for l in exp.test:
      if l == '*':
        break
      assert l in _trie, (
        "%s:%d: Glob '%s' does not match with any tests in the %s test suite" %
        (expectations_file, exp.lineno, exp.test, test_class.Name()))
      _trie = _trie[l]


def CheckTestExpectationGlobsForCollision(expectations, file_name):
  """This function looks for collisions between test expectations with patterns
  that match with test expectation patterns that are globs. A test expectation
  collides with another if its pattern matches with another's glob and if its
  tags is a super set of the other expectation's tags. The less specific test
  expectation must have a failure or skip expectation while the more specific
  test expectation does not. The more specific test expectation will trump
  the less specific test expectation and may cause an unexpected regression.

  Args:
  expectations: A string containing test expectations in the new format
  file_name: Name of the file that the test expectations came from
  """
  master_conflicts_found = False
  error_msg = ''
  trie = {}
  parser = expectations_parser.TaggedTestListParser(expectations)
  globs_to_expectations = defaultdict(list)

  _MapGpuDevicesToVendors(parser.tag_sets)

  for exp in parser.expectations:
    _trie = trie.setdefault(exp.test[0], {})
    for l in exp.test[1:]:
      _trie = _trie.setdefault(l, {})
    _trie.setdefault('$', []).append(exp)

  for exp in parser.expectations:
    _trie = trie
    glob = ''
    for l in exp.test:
      if '*' in _trie:
        globs_to_expectations[glob + '*'].append(exp)
      glob += l
      if l in _trie:
        _trie = _trie[l]
      else:
        break
    if '*' in _trie:
      globs_to_expectations[glob + '*'].append(exp)

  for glob, expectations in globs_to_expectations.items():
    conflicts_found = False
    globs_to_match = [e for e in expectations if e.test == glob]
    matched_to_globs = [e for e in expectations if e.test != glob]
    for match in matched_to_globs:
      for possible_collision in globs_to_match:
        reason = _ConflictLeaksRegression(possible_collision, match)
        if (reason and
            _IsCollision(possible_collision.tags, match.tags)):
          if not conflicts_found:
            error_msg += ('\n\nFound conflicts for pattern %s in %s:\n' %
                          (glob, file_name))
          master_conflicts_found = conflicts_found = True
          error_msg += ('  line %d conflicts with line %d: %s\n' %
                        (possible_collision.lineno, match.lineno, reason))
  assert not master_conflicts_found, error_msg


def CheckTestExpectationPatternsForCollision(expectations, file_name):
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
      if _IsCollision(e1.tags, e2.tags) or _IsCollision(e2.tags, e1.tags):
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
  generate_gpu_fn = gpu_tests.GenerateGpuTests
  expectations_files_fn = gpu_tests.ExpectationsFiles
  gpu_tests.GenerateGpuTests = mock.MagicMock(return_value=[('a/b/c', ())])
  gpu_tests.ExpectationsFiles = mock.MagicMock(
      return_value=[expectations_file.name])
  try:
    CheckTestExpectationsAreForExistingTests(gpu_tests, options)
  finally:
    gpu_tests.GenerateGpuTests = generate_gpu_fn
    gpu_tests.ExpectationsFiles = expectations_files_fn


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

  def testNoCollisionsWithGlobsInGpuTestExpectations(self):
    webgl_conformance_test_class = (
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest)
    for test_case in _FindTestCases():
      if 'gpu_tests.gpu_integration_test_unittest' not in test_case.__module__:
        for i in xrange(1, 2 + (test_case == webgl_conformance_test_class)):
          _ = list(test_case.GenerateGpuTests(
              gpu_helper.GetMockArgs(webgl_version=('%d.0.0' % i))))
          if test_case.ExpectationsFiles():
            with open(test_case.ExpectationsFiles()[0]) as f:
              CheckTestExpectationGlobsForCollision(f.read(),
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


class TestGpuTestExpectationsValidators(unittest.TestCase):
  def testCollisionInTestExpectationsWithSpecifcAndGenericOsTags(self):
    test_expectations = '''# tags: [ mac win linux xp ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    [ intel win ] a/b/c/d [ Failure ]
    [ intel xp debug ] a/b/c/d [ Skip ]
    '''
    with self.assertRaises(AssertionError):
      CheckTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testNoCollisionBetweenSpecificOsTags(self):
    test_expectations = '''# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    [ intel win7 ] a/b/c/d [ Failure ]
    [ intel xp debug ] a/b/c/d [ Skip ]
    '''
    CheckTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testCollisionInTestExpectationsWithGpuVendorAndDeviceTags(self):
    test_expectations = '''# tags: [ mac win linux xp ]
    # tags: [ intel amd nvidia nvidia-0x01 ]
    # tags: [ debug release ]
    [ nvidia win ] a/b/c/d [ Failure ]
    [ nvidia-0x01 xp debug ] a/b/c/d [ Skip ]
    '''
    with self.assertRaises(AssertionError):
      CheckTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testCollisionInTestExpectationsWithGpuVendorAndDeviceTags2(self):
    test_expectations = '''# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia nvidia-0x01 nvidia-0x02 ]
    # tags: [ debug release ]
    [ nvidia-0x01 win ] a/b/c/* [ Failure ]
    [ nvidia win7 debug ] a/b/c/* [ Skip ]
    '''
    with self.assertRaises(AssertionError):
      CheckTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testNoCollisionBetweenGpuDeviceTags(self):
    test_expectations = '''# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia nvidia-0x01 nvidia-0x02 ]
    # tags: [ debug release ]
    [ nvidia-0x01 win7 ] a/b/c/d [ Failure ]
    [ nvidia-0x02 win7 debug ] a/b/c/d [ Skip ]
    [ nvidia win debug ] a/b/c/* [ Skip ]
    '''
    CheckTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testMixGenericandSpecificTagsInCollidingSets(self):
    test_expectations = '''# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia nvidia-0x01 ]
    # tags: [ debug release ]
    [ nvidia-0x01 win ] a/b/c/d [ Failure ]
    [ nvidia win7 debug ] a/b/c/d [ Skip ]
    '''
    with self.assertRaises(AssertionError):
      CheckTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testCollisionInTestExpectationCausesAssertion(self):
    test_expectations = '''# tags: [ mac win linux ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
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
    self.assertIn('line 4 conflicts with line 5',
      str(context.exception))
    self.assertIn('line 4 conflicts with line 6',
      str(context.exception))
    self.assertIn('line 5 conflicts with line 6',
      str(context.exception))
    self.assertIn("Found conflicts for test a/b in test.txt:",
      str(context.exception))
    self.assertIn('line 7 conflicts with line 8',
      str(context.exception))
    self.assertNotIn("Found conflicts for test a/b/c in test.txt:",
      str(context.exception))

  def testCollisionWithGlobsWithFailureExpectation(self):
    test_expectations = '''# tags: [ mac win linux ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    [ intel debug ] a/b/c/d* [ Failure ]
    [ intel debug mac ] a/b/c/d [ RetryOnFailure ]
    [ intel debug mac ] a/b/c/d/e [ Failure ]
    '''
    with self.assertRaises(AssertionError) as context:
      CheckTestExpectationGlobsForCollision(test_expectations, 'test.txt')
    self.assertIn('Found conflicts for pattern a/b/c/d* in test.txt:',
        str(context.exception))
    self.assertIn(("line 4 conflicts with line 5: Pattern 'a/b/c/d*' on line 4 "
        "has the Failure expectation however the the pattern on 'a/b/c/d'"
        " line 5 has the Pass expectation"),
        str(context.exception))
    self.assertNotIn('line 4 conflicts with line 6',
        str(context.exception))

  def testNoCollisionWithGlobsWithFailureExpectation(self):
    test_expectations = '''# tags: [ mac win linux ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    [ intel debug mac ] a/b/c/* [ Failure ]
    [ intel debug ] a/b/c/d [ Failure ]
    [ intel debug ] a/b/c/d [ Skip ]
    '''
    CheckTestExpectationGlobsForCollision(test_expectations, 'test.txt')

  def testCollisionWithGlobsWithSkipExpectation(self):
    test_expectations = '''# tags: [ mac win linux ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    [ intel debug ] a/b/c/d* [ Skip ]
    [ intel debug mac ] a/b/c/d [ Failure ]
    [ intel debug mac ] a/b/c/d/e [ RetryOnFailure ]
    '''
    with self.assertRaises(AssertionError) as context:
      CheckTestExpectationGlobsForCollision(test_expectations, 'test.txt')
    self.assertIn('Found conflicts for pattern a/b/c/d* in test.txt:',
        str(context.exception))
    self.assertNotIn('line 4 conflicts with line 5',
        str(context.exception))
    self.assertIn('line 4 conflicts with line 6',
        str(context.exception))
    self.assertIn(("line 4 conflicts with line 6: Pattern 'a/b/c/d*' on line 4 "
        "has the Skip expectation however the the pattern on 'a/b/c/d/e'"
        " line 6 has the Pass expectation"),
        str(context.exception))

  def testNoCollisionWithGlobsWithSkipExpectation(self):
    test_expectations = '''# tags: [ mac win linux ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    [ intel debug mac ] a/b/c/* [ Skip ]
    [ intel debug ] a/b/c/d [ Skip ]
    [ intel debug ] a/b/c/d [ Skip ]
    '''
    CheckTestExpectationGlobsForCollision(test_expectations, 'test.txt')

  def testNoCollisionInTestExpectations(self):
    test_expectations = '''# tags: [ mac win linux ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    [ intel win release ] a/b/* [ Failure ]
    [ intel debug ] a/b/c/d [ Failure ]
    [ nvidia debug ] a/b/c/d [ Failure ]
    '''
    CheckTestExpectationPatternsForCollision(test_expectations, 'test.txt')

  def testExpectationPatternNotInGeneratedTests(self):
    with self.assertRaises(AssertionError) as context:
      _TestCheckTestExpectationsAreForExistingTests('a/b/d [ Failure ]')
    self.assertIn(("1: Glob 'a/b/d' does not match with any"
                  " tests in the GpuIntegrationTest test suite"),
                  str(context.exception))

  def testGlobMatchesTestName(self):
    _TestCheckTestExpectationsAreForExistingTests('a/b* [ Failure ]')
