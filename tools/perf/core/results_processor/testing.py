# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper functions to build intermediate results for testing."""

import json


_BENCHMARK_START_KEYS = set(['startTime'])


def IntermediateResults(test_results, start_time='2015-10-21T07:28:00.000Z',
                        finalized=True, interrupted=False, diagnostics=None):
  """Build a dict of 'parsed' intermediate results.

  Args:
    test_results: A sequence of testResult dicts.
    start_time: An optional UTC timestamp recording when a benchmark started
      running.
    finalized: An optional bool indicating whether the benchmark run finalized.
      Defaults to True.
    interrupted: An optional bool indicating whether the benchmark run was
      interrupted. Defaults to False.
  """
  return {
      'benchmarkRun': {
          'startTime': start_time,
          'finalized': finalized,
          'interrupted': interrupted,
          'diagnostics': diagnostics or {},
      },
      'testResults': list(test_results)
  }


def TestResult(test_path, status='PASS', is_expected=None,
               start_time='2015-10-21T07:28:00.000Z', run_duration='1.00s',
               output_artifacts=None, tags=None):
  """Build a TestResult dict.

  This follows the TestResultEntry spec of LUCI Test Results format.
  See: go/luci-test-results-design

  Args:
    test_path: A string with the path that identifies this test. Usually of
      the form '{benchmark_name}/{story_name}'.
    status: An optional string indicating the status of the test run. Usually
      one of 'PASS', 'SKIP', 'FAIL'. Defaults to 'PASS'.
    is_expected: An optional bool indicating whether the status result is
      expected. Defaults to True for 'PASS', 'SKIP'; and False otherwise.
    start_time: An optional UTC timestamp recording when the test run started.
    run_duration: An optional duration string recording the amount of time
      that the test run lasted.
    artifcats: An optional dict mapping artifact names to Artifact dicts.
    tags: An optional sequence of tags associated with this test run; each
      tag is given as a '{key}:{value}' string. Keys are not unique, the same
      key may appear multiple times.

  Returns:
    A TestResult dict.
  """
  if is_expected is None:
    is_expected = status in ('PASS', 'SKIP')
  test_result = {
      'testPath': test_path,
      'status': status,
      'isExpected': is_expected,
      'startTime': start_time,
      'runDuration': run_duration
  }
  if output_artifacts is not None:
    test_result['outputArtifacts'] = output_artifacts
  if tags is not None:
    test_result['tags'] = [_SplitTag(tag) for tag in tags]

  return test_result


def Artifact(file_path, remote_url=None,
             content_type='application/octet-stream'):
  """Build an Artifact dict.

  Args:
    file_path: A string with the absolute path where the artifact is stored.
    remote_url: An optional string with a URL where the artifact has been
      uploaded to.
    content_type: An optional string with the MIME type of the artifact.
  """
  artifact = {'filePath': file_path, 'contentType': content_type}
  if remote_url is not None:
    artifact['remoteUrl'] = remote_url
  return artifact


def SerializeIntermediateResults(in_results, filepath):
  """Serialize intermediate results to a filepath.

  Args:
    in_results: A dict with intermediate results, e.g. as produced by
      IntermediateResults or parsed from an intermediate results file.
    filpath: A file path where to serialize the intermediate results.
  """
  # Split benchmarkRun into fields recorded at startup and when finishing.
  benchmark_start = {}
  benchmark_finish = {}
  for key, value in in_results['benchmarkRun'].items():
    d = benchmark_start if key in _BENCHMARK_START_KEYS else benchmark_finish
    d[key] = value

  # Serialize individual records as a sequence of json lines.
  with open(filepath, 'w') as fp:
    _SerializeRecord({'benchmarkRun': benchmark_start}, fp)
    for test_result in in_results['testResults']:
      _SerializeRecord({'testResult': test_result}, fp)
    _SerializeRecord({'benchmarkRun': benchmark_finish}, fp)


def _SplitTag(tag):
  key, value = tag.split(':', 1)
  return {'key': key, 'value': value}


def _SerializeRecord(record, fp):
  fp.write(json.dumps(record, sort_keys=True, separators=(',', ':')) + '\n')
