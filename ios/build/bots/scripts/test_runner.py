# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test runners for iOS."""

import errno
import signal
import sys

import collections
import glob
import json
import logging
from multiprocessing import pool
import os
import plistlib
import psutil
import re
import shutil
import subprocess
import tempfile
import threading
import time

import gtest_utils
import xctest_utils

LOGGER = logging.getLogger(__name__)
DERIVED_DATA = os.path.expanduser('~/Library/Developer/Xcode/DerivedData')
READLINE_TIMEOUT = 180


class Error(Exception):
  """Base class for errors."""
  pass


class TestRunnerError(Error):
  """Base class for TestRunner-related errors."""
  pass


class AppLaunchError(TestRunnerError):
  """The app failed to launch."""
  pass


class AppNotFoundError(TestRunnerError):
  """The requested app was not found."""
  def __init__(self, app_path):
    super(AppNotFoundError, self).__init__(
      'App does not exist: %s' % app_path)


class SystemAlertPresentError(TestRunnerError):
  """System alert is shown on the device."""
  def __init__(self):
    super(SystemAlertPresentError, self).__init__(
      'System alert is shown on the device.')


class DeviceDetectionError(TestRunnerError):
  """Unexpected number of devices detected."""
  def __init__(self, udids):
    super(DeviceDetectionError, self).__init__(
      'Expected one device, found %s:\n%s' % (len(udids), '\n'.join(udids)))


class DeviceRestartError(TestRunnerError):
  """Error restarting a device."""
  def __init__(self):
    super(DeviceRestartError, self).__init__('Error restarting a device')


class PlugInsNotFoundError(TestRunnerError):
  """The PlugIns directory was not found."""
  def __init__(self, plugins_dir):
    super(PlugInsNotFoundError, self).__init__(
      'PlugIns directory does not exist: %s' % plugins_dir)


class SimulatorNotFoundError(TestRunnerError):
  """The given simulator binary was not found."""
  def __init__(self, iossim_path):
    super(SimulatorNotFoundError, self).__init__(
        'Simulator does not exist: %s' % iossim_path)


class TestDataExtractionError(TestRunnerError):
  """Error extracting test data or crash reports from a device."""
  def __init__(self):
    super(TestDataExtractionError, self).__init__('Failed to extract test data')


class XcodeVersionNotFoundError(TestRunnerError):
  """The requested version of Xcode was not found."""
  def __init__(self, xcode_version):
    super(XcodeVersionNotFoundError, self).__init__(
        'Xcode version not found: %s' % xcode_version)


class XCTestPlugInNotFoundError(TestRunnerError):
  """The .xctest PlugIn was not found."""
  def __init__(self, xctest_path):
    super(XCTestPlugInNotFoundError, self).__init__(
        'XCTest not found: %s' % xctest_path)


class MacToolchainNotFoundError(TestRunnerError):
  """The mac_toolchain is not specified."""
  def __init__(self, mac_toolchain):
    super(MacToolchainNotFoundError, self).__init__(
        'mac_toolchain is not specified or not found: "%s"' % mac_toolchain)


class XcodePathNotFoundError(TestRunnerError):
  """The path to Xcode.app is not specified."""
  def __init__(self, xcode_path):
    super(XcodePathNotFoundError, self).__init__(
        'xcode_path is not specified or does not exist: "%s"' % xcode_path)


class ReplayPathNotFoundError(TestRunnerError):
  """The replay path was not found."""
  def __init__(self, replay_path):
    super(ReplayPathNotFoundError, self).__init__(
        'Replay path does not exist: %s' % replay_path)

class CertPathNotFoundError(TestRunnerError):
  """The certificate path was not found."""
  def __init__(self, replay_path):
    super(CertPathNotFoundError, self).__init__(
        'Cert path does not exist: %s' % replay_path)

class WprToolsNotFoundError(TestRunnerError):
  """wpr_tools_path is not specified."""
  def __init__(self, wpr_tools_path):
    super(WprToolsNotFoundError, self).__init__(
        'wpr_tools_path is not specified or not found: "%s"' % wpr_tools_path)


class ShardingDisabledError(TestRunnerError):
  """Temporary error indicating that sharding is not yet implemented."""
  def __init__(self):
    super(ShardingDisabledError, self).__init__(
      'Sharding has not been implemented!')


def terminate_process(proc):
  """Terminates the process.

  If an error occurs ignore it, just print out a message.

  Args:
    proc: A subprocess to terminate.
  """
  try:
    LOGGER.info('Killing hung process %s' % proc.pid)
    proc.terminate()
    attempts_to_kill = 3
    ps = psutil.Process(proc.pid)
    for _ in range(attempts_to_kill):
      # Check whether proc.pid process is still alive.
      if ps.is_running():
        LOGGER.info(
            'Process iossim is still alive! Xcodebuild process might block it.')
        xcodebuild_processes = [
            p for p in psutil.process_iter()
            # Use as_dict() to avoid API changes across versions of psutil.
            if 'xcodebuild' == p.as_dict(attrs=['name'])['name']]
        if not xcodebuild_processes:
          LOGGER.debug('There are no running xcodebuild processes.')
          break
        LOGGER.debug('List of running xcodebuild processes: %s'
                     % xcodebuild_processes)
        # Killing xcodebuild processes
        for p in xcodebuild_processes:
          p.send_signal(signal.SIGKILL)
        psutil.wait_procs(xcodebuild_processes)
      else:
        LOGGER.info('Process was killed!')
        break
  except OSError as ex:
    LOGGER.info('Error while killing a process: %s' % ex)


def print_process_output(proc, parser, timeout=READLINE_TIMEOUT):
  """Logs process messages in console and waits until process is done.

  Method waits until no output message and if no message for timeout seconds,
  process will be terminated.

  Args:
    proc: A running process.
    Parser: A parser.
    timeout: Timeout(in seconds) to subprocess.stdout.readline method.
  """
  out = []
  while True:
    # subprocess.stdout.readline() might be stuck from time to time
    # and tests fail because of TIMEOUT.
    # Try to fix the issue by adding timer-thread for `timeout` seconds
    # that will kill `frozen` running process if no new line is read
    # and will finish test attempt.
    # If new line appears in timeout, just cancel timer.
    timer = threading.Timer(timeout, terminate_process, [proc])
    timer.start()
    line = proc.stdout.readline()
    timer.cancel()
    if not line:
      break
    line = line.rstrip()
    out.append(line)
    parser.ProcessLine(line)
    LOGGER.info(line)
    sys.stdout.flush()
  LOGGER.debug('Finished print_process_output.')
  return out


def get_kif_test_filter(tests, invert=False):
  """Returns the KIF test filter to filter the given test cases.

  Args:
    tests: List of test cases to filter.
    invert: Whether to invert the filter or not. Inverted, the filter will match
      everything except the given test cases.

  Returns:
    A string which can be supplied to GKIF_SCENARIO_FILTER.
  """
  # A pipe-separated list of test cases with the "KIF." prefix omitted.
  # e.g. NAME:a|b|c matches KIF.a, KIF.b, KIF.c.
  # e.g. -NAME:a|b|c matches everything except KIF.a, KIF.b, KIF.c.
  test_filter = '|'.join(test.split('KIF.', 1)[-1] for test in tests)
  if invert:
    return '-NAME:%s' % test_filter
  return 'NAME:%s' % test_filter


def get_gtest_filter(tests, invert=False):
  """Returns the GTest filter to filter the given test cases.

  Args:
    tests: List of test cases to filter.
    invert: Whether to invert the filter or not. Inverted, the filter will match
      everything except the given test cases.

  Returns:
    A string which can be supplied to --gtest_filter.
  """
  # A colon-separated list of tests cases.
  # e.g. a:b:c matches a, b, c.
  # e.g. -a:b:c matches everything except a, b, c.
  test_filter = ':'.join(test for test in tests)
  if invert:
    return '-%s' % test_filter
  return test_filter


def xcode_select(xcode_app_path):
  """Switch the default Xcode system-wide to `xcode_app_path`.

  Raises subprocess.CalledProcessError on failure.
  To be mocked in tests.
  """
  subprocess.check_call([
      'sudo',
      'xcode-select',
      '-switch',
      xcode_app_path,
  ])


def install_xcode(xcode_build_version, mac_toolchain_cmd, xcode_app_path):
  """Installs the requested Xcode build version.

  Args:
    xcode_build_version: (string) Xcode build version to install.
    mac_toolchain_cmd: (string) Path to mac_toolchain command to install Xcode.
    See https://chromium.googlesource.com/infra/infra/+/master/go/src/infra/cmd/mac_toolchain/
    xcode_app_path: (string) Path to install the contents of Xcode.app.

  Returns:
    True if installation was successful. False otherwise.
  """
  try:
    if not mac_toolchain_cmd:
      raise MacToolchainNotFoundError(mac_toolchain_cmd)
    # Guard against incorrect install paths. On swarming, this path
    # should be a requested named cache, and it must exist.
    if not os.path.exists(xcode_app_path):
      raise XcodePathNotFoundError(xcode_app_path)

    subprocess.check_call([
      mac_toolchain_cmd, 'install',
      '-kind', 'ios',
      '-xcode-version', xcode_build_version.lower(),
      '-output-dir', xcode_app_path,
    ])
    xcode_select(xcode_app_path)
  except subprocess.CalledProcessError as e:
    # Flush buffers to ensure correct output ordering.
    sys.stdout.flush()
    sys.stderr.write('Xcode build version %s failed to install: %s\n' % (
        xcode_build_version, e))
    sys.stderr.flush()
    return False

  return True


def get_current_xcode_info():
  """Returns the current Xcode path, version, and build number.

  Returns:
    A dict with 'path', 'version', and 'build' keys.
      'path': The absolute path to the Xcode installation.
      'version': The Xcode version.
      'build': The Xcode build version.
  """
  try:
    out = subprocess.check_output(['xcodebuild', '-version']).splitlines()
    version, build_version = out[0].split(' ')[-1], out[1].split(' ')[-1]
    path = subprocess.check_output(['xcode-select', '--print-path']).rstrip()
  except subprocess.CalledProcessError:
    version = build_version = path = None

  return {
    'path': path,
    'version': version,
    'build': build_version,
  }


def get_test_names(app_path):
  """Gets list of tests from test app.

  Args:
     app_path: A path to test target bundle.

  Returns:
     List of tests.
  """
  cmd = ['otool', '-ov', app_path]
  test_pattern = re.compile(
      'imp (?:0[xX][0-9a-fA-F]+ )?-\['
      '(?P<testSuite>[A-Za-z_][A-Za-z0-9_]*Test(?:Case)?)\s'
      '(?P<testMethod>test[A-Za-z0-9_]*)\]')
  return test_pattern.findall(subprocess.check_output(cmd))


def shard_xctest(object_path, shards, test_cases=None):
  """Gets EarlGrey test methods inside a test target and splits them into shards

  Args:
    object_path: Path of the test target bundle.
    shards: Number of shards to split tests.
    test_cases: Passed in test cases to run.

  Returns:
    A list of test shards.
  """
  test_names = get_test_names(object_path)
  # If test_cases are passed in, only shard the intersection of them and the
  # listed tests.  Format of passed-in test_cases can be either 'testSuite' or
  # 'testSuite/testMethod'.  The listed tests are tuples of ('testSuite',
  # 'testMethod').  The intersection includes both test suites and test methods.
  tests_set = set()
  if test_cases:
    for test in test_names:
      test_method = '%s/%s' % (test[0], test[1])
      if test[0] in test_cases or test_method in test_cases:
        tests_set.add(test_method)
  else:
    for test in test_names:
      # 'ChromeTestCase' is the parent class of all EarlGrey test classes. It
      # has no real tests.
      if 'ChromeTestCase' != test[0]:
        tests_set.add('%s/%s' % (test[0], test[1]))

  tests = sorted(tests_set)
  shard_len = len(tests)/shards  + (len(tests) % shards > 0)
  test_shards=[tests[i:i + shard_len] for i in range(0, len(tests), shard_len)]
  return test_shards


class TestRunner(object):
  """Base class containing common functionality."""

  def __init__(
    self,
    app_path,
    xcode_build_version,
    out_dir,
    env_vars=None,
    mac_toolchain='',
    retries=None,
    shards=None,
    test_args=None,
    test_cases=None,
    xcode_path='',
    xctest=False,
  ):
    """Initializes a new instance of this class.

    Args:
      app_path: Path to the compiled .app to run.
      xcode_build_version: Xcode build version to install before running tests.
      out_dir: Directory to emit test data into.
      env_vars: List of environment variables to pass to the test itself.
      mac_toolchain: Command to run `mac_toolchain` tool.
      retries: Number of times to retry failed test cases.
      test_args: List of strings to pass as arguments to the test when
        launching.
      test_cases: List of tests to be included in the test run. None or [] to
        include all tests.
      xcode_path: Path to Xcode.app folder where its contents will be installed.
      xctest: Whether or not this is an XCTest.

    Raises:
      AppNotFoundError: If the given app does not exist.
      PlugInsNotFoundError: If the PlugIns directory does not exist for XCTests.
      XcodeVersionNotFoundError: If the given Xcode version does not exist.
      XCTestPlugInNotFoundError: If the .xctest PlugIn does not exist.
    """
    app_path = os.path.abspath(app_path)
    if not os.path.exists(app_path):
      raise AppNotFoundError(app_path)

    if not install_xcode(xcode_build_version, mac_toolchain, xcode_path):
      raise XcodeVersionNotFoundError(xcode_build_version)

    xcode_info = get_current_xcode_info()
    LOGGER.info('Using Xcode version %s build %s at %s',
                 xcode_info['version'],
                 xcode_info['build'],
                 xcode_info['path'])

    if not os.path.exists(out_dir):
      os.makedirs(out_dir)

    self.app_name = os.path.splitext(os.path.split(app_path)[-1])[0]
    self.app_path = app_path
    self.cfbundleid = subprocess.check_output([
        '/usr/libexec/PlistBuddy',
        '-c', 'Print:CFBundleIdentifier',
        os.path.join(app_path, 'Info.plist'),
    ]).rstrip()
    self.env_vars = env_vars or []
    self.logs = collections.OrderedDict()
    self.out_dir = out_dir
    self.retries = retries or 0
    self.shards = shards or 1
    self.test_args = test_args or []
    self.test_cases = test_cases or []
    self.xctest_path = ''

    self.test_results = {}
    self.test_results['version'] = 3
    self.test_results['path_delimiter'] = '.'
    self.test_results['seconds_since_epoch'] = int(time.time())
    # This will be overwritten when the tests complete successfully.
    self.test_results['interrupted'] = True

    if xctest:
      plugins_dir = os.path.join(self.app_path, 'PlugIns')
      if not os.path.exists(plugins_dir):
        raise PlugInsNotFoundError(plugins_dir)
      for plugin in os.listdir(plugins_dir):
        if plugin.endswith('.xctest'):
          self.xctest_path = os.path.join(plugins_dir, plugin)
      if not os.path.exists(self.xctest_path):
        raise XCTestPlugInNotFoundError(self.xctest_path)

  def get_launch_command(self, test_filter=None, invert=False):
    """Returns the command that can be used to launch the test app.

    Args:
      test_filter: List of test cases to filter.
      invert: Whether to invert the filter or not. Inverted, the filter will
        match everything except the given test cases.

    Returns:
      A list of strings forming the command to launch the test.
    """
    raise NotImplementedError

  def get_launch_env(self):
    """Returns a dict of environment variables to use to launch the test app.

    Returns:
      A dict of environment variables.
    """
    return os.environ.copy()

  def shutdown_and_restart(self):
    """Restart a device or relaunch a simulator."""
    pass

  def set_up(self):
    """Performs setup actions which must occur prior to every test launch."""
    raise NotImplementedError

  def tear_down(self):
    """Performs cleanup actions which must occur after every test launch."""
    raise NotImplementedError

  def screenshot_desktop(self):
    """Saves a screenshot of the desktop in the output directory."""
    subprocess.check_call([
        'screencapture',
        os.path.join(self.out_dir, 'desktop_%s.png' % time.time()),
    ])

  def retrieve_derived_data(self):
    """Retrieves the contents of DerivedData"""
    # DerivedData contains some logs inside workspace-specific directories.
    # Since we don't control the name of the workspace or project, most of
    # the directories are just called "temporary", making it hard to tell
    # which directory we need to retrieve. Instead we just delete the
    # entire contents of this directory before starting and return the
    # entire contents after the test is over.
    if os.path.exists(DERIVED_DATA):
      os.mkdir(os.path.join(self.out_dir, 'DerivedData'))
      derived_data = os.path.join(self.out_dir, 'DerivedData')
      for directory in os.listdir(DERIVED_DATA):
        shutil.move(os.path.join(DERIVED_DATA, directory), derived_data)

  def wipe_derived_data(self):
    """Removes the contents of Xcode's DerivedData directory."""
    if os.path.exists(DERIVED_DATA):
      shutil.rmtree(DERIVED_DATA)
      os.mkdir(DERIVED_DATA)

  def run_tests(self, test_shard=None):
    """Runs passed-in tests.

    Args:
      test_shard: Test cases to be included in the run.

    Return:
      out: (list) List of strings of subprocess's output.
      udid: (string) Name of the simulator device in the run.
      returncode: (int) Return code of subprocess.
    """
    raise NotImplementedError

  def set_sigterm_handler(self, handler):
    """Sets the SIGTERM handler for the test runner.

    This is its own separate function so it can be mocked in tests.

    Args:
      handler: The handler to be called when a SIGTERM is caught

    Returns:
      The previous SIGTERM handler for the test runner.
    """
    LOGGER.debug('Setting sigterm handler.')
    return signal.signal(signal.SIGTERM, handler)

  def handle_sigterm(self, proc):
    """Handles a SIGTERM sent while a test command is executing.

    Will SIGKILL the currently executing test process, then
    attempt to exit gracefully.

    Args:
      proc: The currently executing test process.
    """
    LOGGER.warning('Sigterm caught during test run. Killing test process.')
    proc.kill()

  def _run(self, cmd, shards=1):
    """Runs the specified command, parsing GTest output.

    Args:
      cmd: List of strings forming the command to run.

    Returns:
      GTestResult instance.
    """
    result = gtest_utils.GTestResult(cmd)
    if self.xctest_path:
      parser = xctest_utils.XCTestLogParser()
    else:
      parser = gtest_utils.GTestLogParser()

    if shards > 1:
      test_shards = shard_xctest(
        os.path.join(self.app_path, self.app_name),
        shards,
        self.test_cases
      )

      thread_pool = pool.ThreadPool(processes=shards)
      for out, name, ret in thread_pool.imap_unordered(
        self.run_tests, test_shards):
        LOGGER.info('Simulator %s', name)
        for line in out:
          LOGGER.info(line)
          parser.ProcessLine(line)
        returncode = ret if ret else 0
      thread_pool.close()
      thread_pool.join()
    else:
      # TODO(crbug.com/812705): Implement test sharding for unit tests.
      # TODO(crbug.com/812712): Use thread pool for DeviceTestRunner as well.
      proc = subprocess.Popen(
          cmd,
          env=self.get_launch_env(),
          stdout=subprocess.PIPE,
          stderr=subprocess.STDOUT,
      )
      old_handler = self.set_sigterm_handler(
          lambda _signum, _frame: self.handle_sigterm(proc))
      print_process_output(proc, parser)

      LOGGER.info('Waiting for test process to terminate.')
      proc.wait()
      LOGGER.info('Test process terminated.')
      self.set_sigterm_handler(old_handler)
      sys.stdout.flush()
      LOGGER.debug('Stdout flushed after test process.')

      returncode = proc.returncode

    if self.xctest_path and parser.SystemAlertPresent():
      raise SystemAlertPresentError()

    LOGGER.debug('Processing test results.')
    for test in parser.FailedTests(include_flaky=True):
      # Test cases are named as <test group>.<test case>. If the test case
      # is prefixed with "FLAKY_", it should be reported as flaked not failed.
      if '.' in test and test.split('.', 1)[1].startswith('FLAKY_'):
        result.flaked_tests[test] = parser.FailureDescription(test)
      else:
        result.failed_tests[test] = parser.FailureDescription(test)

    result.passed_tests.extend(parser.PassedTests(include_flaky=True))

    LOGGER.info('%s returned %s\n', cmd[0], returncode)

    # iossim can return 5 if it exits noncleanly even if all tests passed.
    # Therefore we cannot rely on process exit code to determine success.
    result.finalize(returncode, parser.CompletedWithoutFailure())
    return result

  def launch(self):
    """Launches the test app."""
    self.set_up()
    cmd = self.get_launch_command()
    try:
      result = self._run(cmd=cmd, shards=self.shards or 1)
      if result.crashed and not result.crashed_test:
        # If the app crashed but not during any particular test case, assume
        # it crashed on startup. Try one more time.
        self.shutdown_and_restart()
        LOGGER.warning('Crashed on startup, retrying...\n')
        result = self._run(cmd)

      if result.crashed and not result.crashed_test:
        raise AppLaunchError

      passed = result.passed_tests
      failed = result.failed_tests
      flaked = result.flaked_tests

      try:
        # XCTests cannot currently be resumed at the next test case.
        while not self.xctest_path and result.crashed and result.crashed_test:
          # If the app crashes during a specific test case, then resume at the
          # next test case. This is achieved by filtering out every test case
          # which has already run.
          LOGGER.warning('Crashed during %s, resuming...\n',
                         result.crashed_test)
          result = self._run(self.get_launch_command(
              test_filter=passed + failed.keys() + flaked.keys(), invert=True,
          ))
          passed.extend(result.passed_tests)
          failed.update(result.failed_tests)
          flaked.update(result.flaked_tests)
      except OSError as e:
        if e.errno == errno.E2BIG:
          LOGGER.error('Too many test cases to resume.')
        else:
          raise

      # Retry failed test cases.
      retry_results = {}
      if self.retries and failed:
        LOGGER.warning('%s tests failed and will be retried.\n', len(failed))
        for i in xrange(self.retries):
          for test in failed.keys():
            LOGGER.info('Retry #%s for %s.\n', i + 1, test)
            retry_result = self._run(self.get_launch_command(
                test_filter=[test]
            ))
            # If the test passed on retry, consider it flake instead of failure.
            if test in retry_result.passed_tests:
              flaked[test] = failed.pop(test)
            # Save the result of the latest run for each test.
            retry_results[test] = retry_result

      # Build test_results.json.
      # Check if if any of the retries crashed in addition to the original run.
      interrupted = (result.crashed or
                     any([r.crashed for r in retry_results.values()]))
      self.test_results['interrupted'] = interrupted
      self.test_results['num_failures_by_type'] = {
        'FAIL': len(failed) + len(flaked),
        'PASS': len(passed),
      }
      tests = collections.OrderedDict()
      for test in passed:
        tests[test] = { 'expected': 'PASS', 'actual': 'PASS' }
      for test in failed:
        tests[test] = { 'expected': 'PASS', 'actual': 'FAIL' }
      for test in flaked:
        tests[test] = { 'expected': 'PASS', 'actual': 'FAIL' }
      self.test_results['tests'] = tests

      self.logs['passed tests'] = passed
      if flaked:
        self.logs['flaked tests'] = flaked
      if failed:
        self.logs['failed tests'] = failed
      for test, log_lines in failed.iteritems():
        self.logs[test] = log_lines
      for test, log_lines in flaked.iteritems():
        self.logs[test] = log_lines

      return not failed and not interrupted
    finally:
      self.tear_down()


class SimulatorTestRunner(TestRunner):
  """Class for running tests on iossim."""

  def __init__(
      self,
      app_path,
      iossim_path,
      platform,
      version,
      xcode_build_version,
      out_dir,
      env_vars=None,
      mac_toolchain='',
      retries=None,
      shards=None,
      test_args=None,
      test_cases=None,
      use_trusted_cert=False,
      wpr_tools_path='',
      xcode_path='',
      xctest=False,
  ):
    """Initializes a new instance of this class.

    Args:
      app_path: Path to the compiled .app or .ipa to run.
      iossim_path: Path to the compiled iossim binary to use.
      platform: Name of the platform to simulate. Supported values can be found
        by running "iossim -l". e.g. "iPhone 5s", "iPad Retina".
      version: Version of iOS the platform should be running. Supported values
        can be found by running "iossim -l". e.g. "9.3", "8.2", "7.1".
      xcode_build_version: Xcode build version to install before running tests.
      out_dir: Directory to emit test data into.
      env_vars: List of environment variables to pass to the test itself.
      mac_toolchain: Command to run `mac_toolchain` tool.
      retries: Number of times to retry failed test cases.
      test_args: List of strings to pass as arguments to the test when
        launching.
      test_cases: List of tests to be included in the test run. None or [] to
        include all tests.
      use_trusted_cert: Whether to install to the sim a cert that allows for
        HTTPS tests to run locally.
      wpr_tools_path: Path to pre-installed WPR-related tools
      xcode_path: Path to Xcode.app folder where its contents will be installed.
      xctest: Whether or not this is an XCTest.

    Raises:
      AppNotFoundError: If the given app does not exist.
      PlugInsNotFoundError: If the PlugIns directory does not exist for XCTests.
      XcodeVersionNotFoundError: If the given Xcode version does not exist.
      XCTestPlugInNotFoundError: If the .xctest PlugIn does not exist.
    """
    super(SimulatorTestRunner, self).__init__(
        app_path,
        xcode_build_version,
        out_dir,
        env_vars=env_vars,
        mac_toolchain=mac_toolchain,
        retries=retries,
        test_args=test_args,
        test_cases=test_cases,
        xcode_path=xcode_path,
        xctest=xctest,
    )

    iossim_path = os.path.abspath(iossim_path)
    if not os.path.exists(iossim_path):
      raise SimulatorNotFoundError(iossim_path)

    self.homedir = ''
    self.iossim_path = iossim_path
    self.platform = platform
    self.start_time = None
    self.version = version
    self.shards = shards
    self.use_trusted_cert = use_trusted_cert
    self.wpr_tools_path = wpr_tools_path

  @staticmethod
  def kill_simulators():
    """Kills all running simulators."""
    try:
      LOGGER.info('Killing simulators.')
      subprocess.check_call([
          'pkill',
          '-9',
          '-x',
          # The simulator's name varies by Xcode version.
          'com.apple.CoreSimulator.CoreSimulatorService', # crbug.com/684305
          'iPhone Simulator', # Xcode 5
          'iOS Simulator', # Xcode 6
          'Simulator', # Xcode 7+
          'simctl', # https://crbug.com/637429
          'xcodebuild', # https://crbug.com/684305
      ])
      # If a signal was sent, wait for the simulators to actually be killed.
      time.sleep(5)
    except subprocess.CalledProcessError as e:
      if e.returncode != 1:
        # Ignore a 1 exit code (which means there were no simulators to kill).
        raise

  def wipe_simulator(self):
    """Wipes the simulator."""
    subprocess.check_call([
        self.iossim_path,
        '-d', self.platform,
        '-s', self.version,
        '-w',
    ])

  def get_home_directory(self):
    """Returns the simulator's home directory."""
    return subprocess.check_output([
        self.iossim_path,
        '-d', self.platform,
        '-p',
        '-s', self.version,
    ]).rstrip()

  def set_up(self):
    """Performs setup actions which must occur prior to every test launch."""
    self.kill_simulators()
    self.wipe_simulator()
    self.wipe_derived_data()
    self.homedir = self.get_home_directory()
    # Crash reports have a timestamp in their file name, formatted as
    # YYYY-MM-DD-HHMMSS. Save the current time in the same format so
    # we can compare and fetch crash reports from this run later on.
    self.start_time = time.strftime('%Y-%m-%d-%H%M%S', time.localtime())

  def extract_test_data(self):
    """Extracts data emitted by the test."""
    # Find the Documents directory of the test app. The app directory names
    # don't correspond with any known information, so we have to examine them
    # all until we find one with a matching CFBundleIdentifier.
    apps_dir = os.path.join(
        self.homedir, 'Containers', 'Data', 'Application')
    if os.path.exists(apps_dir):
      for appid_dir in os.listdir(apps_dir):
        docs_dir = os.path.join(apps_dir, appid_dir, 'Documents')
        metadata_plist = os.path.join(
            apps_dir,
            appid_dir,
            '.com.apple.mobile_container_manager.metadata.plist',
        )
        if os.path.exists(docs_dir) and os.path.exists(metadata_plist):
          cfbundleid = subprocess.check_output([
              '/usr/libexec/PlistBuddy',
              '-c', 'Print:MCMMetadataIdentifier',
              metadata_plist,
          ]).rstrip()
          if cfbundleid == self.cfbundleid:
            shutil.copytree(docs_dir, os.path.join(self.out_dir, 'Documents'))
            return

  def retrieve_crash_reports(self):
    """Retrieves crash reports produced by the test."""
    # A crash report's naming scheme is [app]_[timestamp]_[hostname].crash.
    # e.g. net_unittests_2014-05-13-15-0900_vm1-a1.crash.
    crash_reports_dir = os.path.expanduser(os.path.join(
        '~', 'Library', 'Logs', 'DiagnosticReports'))

    if not os.path.exists(crash_reports_dir):
      return

    for crash_report in os.listdir(crash_reports_dir):
      report_name, ext = os.path.splitext(crash_report)
      if report_name.startswith(self.app_name) and ext == '.crash':
        report_time = report_name[len(self.app_name) + 1:].split('_')[0]

        # The timestamp format in a crash report is big-endian and therefore
        # a staight string comparison works.
        if report_time > self.start_time:
          with open(os.path.join(crash_reports_dir, crash_report)) as f:
            self.logs['crash report (%s)' % report_time] = (
                f.read().splitlines())

  def tear_down(self):
    """Performs cleanup actions which must occur after every test launch."""
    LOGGER.debug('Extracting test data.')
    self.extract_test_data()
    LOGGER.debug('Retrieving crash reports.')
    self.retrieve_crash_reports()
    LOGGER.debug('Retrieving derived data.')
    self.retrieve_derived_data()
    LOGGER.debug('Making desktop screenshots.')
    self.screenshot_desktop()
    LOGGER.debug('Killing simulators.')
    self.kill_simulators()
    LOGGER.debug('Wiping simulator.')
    self.wipe_simulator()
    if os.path.exists(self.homedir):
      shutil.rmtree(self.homedir, ignore_errors=True)
      self.homedir = ''
    LOGGER.debug('End of tear_down.')

  def run_tests(self, test_shard=None):
    """Runs passed-in tests. Builds a command and create a simulator to
      run tests.
    Args:
      test_shard: Test cases to be included in the run.

    Return:
      out: (list) List of strings of subprocess's output.
      udid: (string) Name of the simulator device in the run.
      returncode: (int) Return code of subprocess.
    """
    udid = self.getSimulator()
    cmd = self.sharding_cmd[:]
    cmd.extend(['-u', udid])
    if test_shard:
      for test in test_shard:
        cmd.extend(['-t', test])

    cmd.append(self.app_path)
    if self.xctest_path:
      cmd.append(self.xctest_path)

    proc = subprocess.Popen(
        cmd,
        env=self.get_launch_env(),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

    out = print_process_output(proc, xctest_utils.XCTestLogParser())
    self.deleteSimulator(udid)
    return (out, udid, proc.returncode)

  def getSimulator(self):
    """Creates a simulator device by device types and runtimes. Returns the
      udid for the created simulator instance.

    Returns:
      An udid of a simulator device.
    """
    simctl_list = json.loads(subprocess.check_output(
                             ['xcrun', 'simctl', 'list', '-j']))
    runtimes = simctl_list['runtimes']
    devices = simctl_list['devicetypes']

    device_type_id = ''
    for device in devices:
      if device['name'] == self.platform:
        device_type_id = device['identifier']

    runtime_id = ''
    for runtime in runtimes:
      if runtime['name'] == 'iOS %s' % self.version:
        runtime_id = runtime['identifier']

    name = '%s test' % self.platform
    LOGGER.info('creating simulator %s', name)
    udid = subprocess.check_output([
      'xcrun', 'simctl', 'create', name, device_type_id, runtime_id]).rstrip()
    LOGGER.info(udid)

    if self.use_trusted_cert:
      if not os.path.exists(self.wpr_tools_path):
        raise WprToolsNotFoundError(self.wpr_tools_path)

      cert_path = "{}/TrustStore_trust.sqlite3".format(self.wpr_tools_path)
      self.copy_trusted_certificate(cert_path)

    return udid

  def deleteSimulator(self, udid=None):
    """Removes dynamically created simulator devices."""
    if udid:
      LOGGER.info('deleting simulator %s', udid)
      subprocess.call(['xcrun', 'simctl', 'delete', udid])

  def get_launch_command(self, test_filter=None, invert=False, test_shard=None):
    """Returns the command that can be used to launch the test app.

    Args:
      test_filter: List of test cases to filter.
      invert: Whether to invert the filter or not. Inverted, the filter will
        match everything except the given test cases.
      test_shard: How many shards the tests should be divided into.

    Returns:
      A list of strings forming the command to launch the test.
    """
    cmd = [
        self.iossim_path,
        '-d', self.platform,
        '-s', self.version,
    ]

    for env_var in self.env_vars:
      cmd.extend(['-e', env_var])

    for test_arg in self.test_args:
      cmd.extend(['-c', test_arg])

    if self.xctest_path:
      self.sharding_cmd = cmd[:]
      if test_filter:
        # iossim doesn't support inverted filters for XCTests.
        if not invert:
          for test in test_filter:
            cmd.extend(['-t', test])
      elif test_shard:
        for test in test_shard:
          cmd.extend(['-t', test])
      elif not invert:
        for test_case in self.test_cases:
          cmd.extend(['-t', test_case])
    elif test_filter:
        kif_filter = get_kif_test_filter(test_filter, invert=invert)
        gtest_filter = get_gtest_filter(test_filter, invert=invert)
        cmd.extend(['-e', 'GKIF_SCENARIO_FILTER=%s' % kif_filter])
        cmd.extend(['-c', '--gtest_filter=%s' % gtest_filter])

    cmd.append(self.app_path)
    if self.xctest_path:
      cmd.append(self.xctest_path)
    return cmd

  def get_launch_env(self):
    """Returns a dict of environment variables to use to launch the test app.

    Returns:
      A dict of environment variables.
    """
    env = super(SimulatorTestRunner, self).get_launch_env()
    if self.xctest_path:
      env['NSUnbufferedIO'] = 'YES'
    return env

  def copy_trusted_certificate(self, cert_path):
    '''Copies a TrustStore file with a trusted HTTPS certificate into all sims.

      This allows the simulators to access HTTPS webpages served through WprGo.

      Args:
        cert_path: Path to the certificate to copy to all emulators
    '''

    if not os.path.exists(cert_path):
      raise CertPathNotFoundError(cert_path)

    trustStores = glob.glob(
        '{}/Library/Developer/CoreSimulator/Devices/*/data/Library'.
        format(os.path.expanduser('~')))
    for trustStore in trustStores:
      LOGGER.info('Copying TrustStore to %s', trustStore)
      if not os.path.exists(trustStore + "/Keychains/"):
        os.makedirs(trustStore + "/Keychains/")
      shutil.copy(cert_path, trustStore + "/Keychains/TrustStore.sqlite3")


class WprProxySimulatorTestRunner(SimulatorTestRunner):
  """Class for running simulator tests with WPR against saved website replays"""

  def __init__(
      self,
      app_path,
      iossim_path,
      replay_path,
      platform,
      version,
      wpr_tools_path,
      xcode_build_version,
      out_dir,
      env_vars=None,
      mac_toolchain='',
      retries=None,
      shards=None,
      test_args=None,
      test_cases=None,
      xcode_path='',
      xctest=False,
  ):
    """Initializes a new instance of this class.

    Args:
      app_path: Path to the compiled .app or .ipa to run.
      iossim_path: Path to the compiled iossim binary to use.
      replay_path: Path to the folder where WPR replay and recipe files live.
      platform: Name of the platform to simulate. Supported values can be found
        by running "iossim -l". e.g. "iPhone 5s", "iPad Retina".
      version: Version of iOS the platform should be running. Supported values
        can be found by running "iossim -l". e.g. "9.3", "8.2", "7.1".
      wpr_tools_path: Path to pre-installed (from CIPD) WPR-related tools
      xcode_build_version: Xcode build version to install before running tests.
      out_dir: Directory to emit test data into.
      env_vars: List of environment variables to pass to the test itself.
      mac_toolchain: Command to run `mac_toolchain` tool.
      retries: Number of times to retry failed test cases.
      test_args: List of strings to pass as arguments to the test when
        launching.
      test_cases: List of tests to be included in the test run. None or [] to
        include all tests.
      xcode_path: Path to Xcode.app folder where its contents will be installed.
      xctest: Whether or not this is an XCTest.

    Raises:
      AppNotFoundError: If the given app does not exist.
      PlugInsNotFoundError: If the PlugIns directory does not exist for XCTests.
      XcodeVersionNotFoundError: If the given Xcode version does not exist.
      XCTestPlugInNotFoundError: If the .xctest PlugIn does not exist.
    """
    super(WprProxySimulatorTestRunner, self).__init__(
      app_path,
      iossim_path,
      platform,
      version,
      xcode_build_version,
      out_dir,
      env_vars=env_vars,
      mac_toolchain=mac_toolchain,
      retries=retries,
      shards=shards,
      test_args=test_args,
      test_cases=test_cases,
      wpr_tools_path=wpr_tools_path,
      xcode_path=xcode_path,
      xctest=xctest,
    )
    self.use_trusted_cert = True

    replay_path = os.path.abspath(replay_path)
    if not os.path.exists(replay_path):
      raise ReplayPathNotFoundError(replay_path)
    self.replay_path = replay_path

    if not os.path.exists(wpr_tools_path):
      raise WprToolsNotFoundError(wpr_tools_path)

    self.proxy_process = None
    self.wprgo_process = None

  def set_up(self):
    '''Performs setup actions which must occur prior to every test launch.'''
    super(WprProxySimulatorTestRunner, self).set_up()

    self.proxy_start()

  def tear_down(self):
    '''Performs cleanup actions which must occur after every test launch.'''
    super(WprProxySimulatorTestRunner, self).tear_down()
    self.proxy_stop()
    self.wprgo_stop()

  def run_wpr_test(self, udid, recipe_path, replay_path):
    '''Runs a single WPR test.

    Args:
      udid: UDID for the simulator to run the test on
      recipe_path: Path to the recipe file (i.e. ios_costco.test)
      replay_path: Path to the replay file (i.e. ios_costco)

    Returns
      [parser, return code from test] where
      parser: a XCTest or GTestLogParser which has processed all
        the output from the test
    '''

    LOGGER.info('Running test for recipe %s', recipe_path)
    self.wprgo_start(replay_path)

    # TODO(crbug.com/881096): Consider reusing get_launch_command
    #  and adding the autofillautomation flag to it

    # TODO(crbug.com/881096): We only run AutofillAutomationTestCase
    #  as we have other unit tests in the suite which are not related
    #  to testing website recipe/replays. We should consider moving
    #  one or the other to a different suite.

    # For the website replay test suite, we need to pass in a single
    # recipe at a time, with the flag "autofillautomation"
    recipe_cmd = [
      self.iossim_path, '-d', self.platform, '-s',
      self.version, '-t', 'AutofillAutomationTestCase', '-c',
      '--enable-features=AutofillShowTypePredictions {}={}'.format(
        '-autofillautomation', recipe_path),
      '-u', udid,
    ]
    for env_var in self.env_vars:
      recipe_cmd.extend(['-e', env_var])

    for test_arg in self.test_args:
      recipe_cmd.extend(['-c', test_arg])

    recipe_cmd.append(self.app_path)
    if self.xctest_path:
      recipe_cmd.append(self.xctest_path)

    proc = subprocess.Popen(
        recipe_cmd,
        env=self.get_launch_env(),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    old_handler = self.set_sigterm_handler(
      lambda _signum, _frame: self.handle_sigterm(proc))

    if self.xctest_path:
      parser = xctest_utils.XCTestLogParser()
    else:
      parser = gtest_utils.GTestLogParser()

    print_process_output(proc, parser)

    proc.wait()
    self.set_sigterm_handler(old_handler)
    sys.stdout.flush()

    self.wprgo_stop()

    return parser, proc.returncode

  def should_run_wpr_test(self, recipe_name, test_filter, invert):
    '''Returns whether the WPR test should be run, given the filters.

      Args:
        recipe_name: Filename of the recipe to run (i.e. 'ios_costco')
        test_filter: List of tests to run. If recipe_name is found as
          a substring of any of these, then the filter is matched.
        invert: If true, run tests that are not matched by the filter.

      Returns:
        True if the test should be run.
    '''
    # If the matching replay for the recipe doesn't exist, don't run it
    replay_path = '{}/{}'.format(self.replay_path, recipe_name)
    if not os.path.isfile(replay_path):
      LOGGER.error('No matching replay file for recipe %s',
                   recipe_name)
      return False

    # if there is no filter, then run tests
    if test_filter == []:
      return True

    test_matched_filter = False
    for filter_name in test_filter:
      if recipe_name in filter_name:
        test_matched_filter = True

    return test_matched_filter != invert

  def _run(self, cmd, shards=1):
    '''Runs the specified command, parsing GTest output.

    Args:
      cmd: List of strings forming the command to run.
      NOTE: in the case of WprProxySimulatorTestRunner, cmd
        is a dict forming the configuration for the test (including
        filter rules), and not indicative of the actual command
        we build and execute in _run.

    Returns:
      GTestResult instance.
    '''

    result = gtest_utils.GTestResult(cmd)
    completed_without_failure = True
    total_returncode = 0
    if shards > 1:
      # TODO(crbug.com/881096): reimplement sharding in the future
      raise ShardingDisabledError()
    else:
      # TODO(crbug.com/812705): Implement test sharding for unit tests.
      # TODO(crbug.com/812712): Use thread pool for DeviceTestRunner as well.

      # Create a simulator for these tests, and prepare it with the
      # certificate needed for HTTPS proxying.
      udid = self.getSimulator()

      for recipe_path in glob.glob('{}/*.test'.format(self.replay_path)):
        base_name = os.path.basename(recipe_path)
        test_name = os.path.splitext(base_name)[0]
        replay_path = '{}/{}'.format(self.replay_path, test_name)

        if self.should_run_wpr_test(
          test_name, cmd['test_filter'], cmd['invert']):

          parser, returncode = self.run_wpr_test(
            udid, recipe_path, replay_path)

          # If this test fails, immediately rerun it to see if it deflakes.
          # We simply overwrite the first result with the second.
          if parser.FailedTests(include_flaky=True):
            parser, returncode = self.run_wpr_test(
              udid, recipe_path, replay_path)

          for test in parser.FailedTests(include_flaky=True):
            # All test names will be the same since we re-run the same suite;
            # therefore, to differentiate the results, we append the recipe
            # name to the test suite.
            testWithRecipeName = "{}.{}".format(base_name, test)

            # Test cases are named as <test group>.<test case>. If the test case
            # is prefixed w/"FLAKY_", it should be reported as flaked not failed
            if '.' in test and test.split(
                '.', 1)[1].startswith('FLAKY_'):
              result.flaked_tests[
                  testWithRecipeName] = parser.FailureDescription(test)
            else:
              result.failed_tests[
                  testWithRecipeName] = parser.FailureDescription(test)

          for test in parser.PassedTests(include_flaky=True):
            testWithRecipeName = "{}.{}".format(base_name, test)
            result.passed_tests.extend([testWithRecipeName])

          # Check for runtime errors.
          if self.xctest_path and parser.SystemAlertPresent():
            raise SystemAlertPresentError()
          if returncode != 0:
            total_returncode = returncode
          if parser.CompletedWithoutFailure() == False:
            completed_without_failure = False
          LOGGER.info('%s test returned %s\n', recipe_path, returncode)

      self.deleteSimulator(udid)

    # iossim can return 5 if it exits noncleanly even if all tests passed.
    # Therefore we cannot rely on process exit code to determine success.

    # NOTE: total_returncode is 0 OR the last non-zero return code from a test.
    result.finalize(total_returncode, completed_without_failure)
    return result

  def get_launch_command(self, test_filter=[], invert=False):
    '''Returns a config dict for the test, instead of the real launch command.
    Normally this is passed into _run as the command it should use, but since
    the WPR runner builds its own cmd, we use this to configure the function.

    Args:
      test_filter: List of test cases to filter.
      invert: Whether to invert the filter or not. Inverted, the filter will
      match everything except the given test cases.

    Returns:
      A dict forming the configuration for the test.
    '''

    test_config = {}
    test_config['invert'] = invert
    test_config['test_filter'] = test_filter
    return test_config

  def proxy_start(self):
    '''Starts tsproxy and routes the machine's traffic through tsproxy.'''

    # Stops any straggling instances of WPRgo that may hog ports 8080/8081
    subprocess.check_call('lsof -ti:8080 | xargs kill -9', shell=True)
    subprocess.check_call('lsof -ti:8081| xargs kill -9', shell=True)

    # We route all network adapters through the proxy, since it is easier than
    # determining which network adapter is being used currently.
    network_services = subprocess.check_output(
      ['networksetup', '-listallnetworkservices']).strip().split('\n')
    if len(network_services) > 1:
      # We ignore the first line as it is a description of the command's output.
      network_services = network_services[1:]

      for service in network_services:
        subprocess.check_call([
          'networksetup', '-setsocksfirewallproxystate', service,
          'on'
        ])
        subprocess.check_call([
          'networksetup', '-setsocksfirewallproxy', service,
          '127.0.0.1', '1080'
        ])

    self.proxy_process = subprocess.Popen(
        [
          'python', 'tsproxy.py', '--port=1080', '--desthost=127.0.0.1',
          '--mapports=443:8081,*:8080'
        ],
        cwd='{}/tsproxy'.format(self.wpr_tools_path),
        env=self.get_launch_env(),
        stdout=open(os.path.join(self.out_dir, 'stdout_proxy.txt'), 'wb+'),
        stderr=subprocess.STDOUT,
    )

  def proxy_stop(self):
    '''Stops tsproxy and disables the machine's proxy settings.'''
    if self.proxy_process != None:
      os.kill(self.proxy_process.pid, signal.SIGINT)

    network_services = subprocess.check_output(
      ['networksetup', '-listallnetworkservices']).strip().split('\n')
    if len(network_services) > 1:
      # We ignore the first line as it is a description of the command's output.
      network_services = network_services[1:]

      for service in network_services:
        subprocess.check_call([
          'networksetup', '-setsocksfirewallproxystate', service,
          'off'
        ])

  def wprgo_start(self, replay_path):
    '''Starts WprGo serving the specified replay file.

      Args:
        replay_path: Path to the WprGo website replay to use.
    '''
    self.wprgo_process = subprocess.Popen(
        [
          './wpr', 'replay', '--http_port=8080',
          '--https_port=8081', replay_path
        ],
        cwd='{}/web_page_replay_go/'.format(self.wpr_tools_path),
        env=self.get_launch_env(),
        stdout=open(os.path.join(self.out_dir, 'stdout_wprgo.txt'), 'wb+'),
        stderr=subprocess.STDOUT,
    )

  def wprgo_stop(self):
    '''Stops serving website replays using WprGo.'''
    if self.wprgo_process != None:
      os.kill(self.wprgo_process.pid, signal.SIGINT)


class DeviceTestRunner(TestRunner):
  """Class for running tests on devices."""

  def __init__(
    self,
    app_path,
    xcode_build_version,
    out_dir,
    env_vars=None,
    mac_toolchain='',
    restart=False,
    retries=None,
    shards=None,
    test_args=None,
    test_cases=None,
    xctest=False,
    xcode_path='',
  ):
    """Initializes a new instance of this class.

    Args:
      app_path: Path to the compiled .app to run.
      xcode_build_version: Xcode build version to install before running tests.
      out_dir: Directory to emit test data into.
      env_vars: List of environment variables to pass to the test itself.
      mac_toolchain: Command to run `mac_toolchain` tool.
      restart: Whether or not restart device when test app crashes on startup.
      retries: Number of times to retry failed test cases.
      test_args: List of strings to pass as arguments to the test when
        launching.
      test_cases: List of tests to be included in the test run. None or [] to
        include all tests.
      xctest: Whether or not this is an XCTest.
      xcode_path: Path to Xcode.app folder where its contents will be installed.

    Raises:
      AppNotFoundError: If the given app does not exist.
      PlugInsNotFoundError: If the PlugIns directory does not exist for XCTests.
      XcodeVersionNotFoundError: If the given Xcode version does not exist.
      XCTestPlugInNotFoundError: If the .xctest PlugIn does not exist.
    """
    super(DeviceTestRunner, self).__init__(
      app_path,
      xcode_build_version,
      out_dir,
      env_vars=env_vars,
      retries=retries,
      test_args=test_args,
      test_cases=test_cases,
      xctest=xctest,
      mac_toolchain=mac_toolchain,
      xcode_path=xcode_path,
    )

    self.udid = subprocess.check_output(['idevice_id', '--list']).rstrip()
    if len(self.udid.splitlines()) != 1:
      raise DeviceDetectionError(self.udid)
    if xctest:
      self.xctestrun_file = tempfile.mkstemp()[1]
      self.xctestrun_data = {
        'TestTargetName': {
          'IsAppHostedTestBundle': True,
          'TestBundlePath': '%s' % self.xctest_path,
          'TestHostPath': '%s' % self.app_path,
          'TestingEnvironmentVariables': {
            'DYLD_INSERT_LIBRARIES':
              '__PLATFORMS__/iPhoneOS.platform/Developer/usr/lib/'
              'libXCTestBundleInject.dylib',
            'DYLD_LIBRARY_PATH':
              '__PLATFORMS__/iPhoneOS.platform/Developer/Library',
            'DYLD_FRAMEWORK_PATH':
              '__PLATFORMS__/iPhoneOS.platform/Developer/Library/Frameworks',
            'XCInjectBundleInto':'__TESTHOST__/%s' % self.app_name
          }
        }
      }

    self.restart = restart

  def uninstall_apps(self):
    """Uninstalls all apps found on the device."""
    for app in subprocess.check_output(
      ['idevicefs', '--udid', self.udid, 'ls', '@']).splitlines():
      subprocess.check_call(
        ['ideviceinstaller', '--udid', self.udid, '--uninstall', app])

  def install_app(self):
    """Installs the app."""
    subprocess.check_call(
      ['ideviceinstaller', '--udid', self.udid, '--install', self.app_path])

  def set_up(self):
    """Performs setup actions which must occur prior to every test launch."""
    self.uninstall_apps()
    self.wipe_derived_data()
    self.install_app()

  def extract_test_data(self):
    """Extracts data emitted by the test."""
    try:
      subprocess.check_call([
        'idevicefs',
        '--udid', self.udid,
        'pull',
        '@%s/Documents' % self.cfbundleid,
        os.path.join(self.out_dir, 'Documents'),
      ])
    except subprocess.CalledProcessError:
      raise TestDataExtractionError()

  def shutdown_and_restart(self):
    """Restart the device, wait for two minutes."""
    # TODO(crbug.com/760399): swarming bot ios 11 devices turn to be unavailable
    # in a few hours unexpectedly, which is assumed as an ios beta issue. Should
    # remove this method once the bug is fixed.
    if self.restart:
      LOGGER.info('Restarting device, wait for two minutes.')
      try:
        subprocess.check_call(
          ['idevicediagnostics', 'restart', '--udid', self.udid])
      except subprocess.CalledProcessError:
        raise DeviceRestartError()
      time.sleep(120)

  def retrieve_crash_reports(self):
    """Retrieves crash reports produced by the test."""
    logs_dir = os.path.join(self.out_dir, 'Logs')
    os.mkdir(logs_dir)
    try:
      subprocess.check_call([
        'idevicecrashreport',
        '--extract',
        '--udid', self.udid,
        logs_dir,
      ])
    except subprocess.CalledProcessError:
      # TODO(crbug.com/828951): Raise the exception when the bug is fixed.
      LOGGER.warning('Failed to retrieve crash reports from device.')

  def tear_down(self):
    """Performs cleanup actions which must occur after every test launch."""
    self.screenshot_desktop()
    self.retrieve_derived_data()
    self.extract_test_data()
    self.retrieve_crash_reports()
    self.uninstall_apps()

  def set_xctest_filters(self, test_filter=None, invert=False):
    """Sets the tests be included in the test run."""
    if self.test_cases:
      filter = self.test_cases
      if test_filter:
        # If inverted, the filter should match tests in test_cases except the
        # ones in test_filter. Otherwise, the filter should be tests both in
        # test_cases and test_filter. test_filter is used for test retries, it
        # should be a subset of test_cases. If the intersection of test_cases
        # and test_filter fails, use test_filter.
        filter = (sorted(set(filter) - set(test_filter)) if invert
                  else sorted(set(filter) & set(test_filter)) or test_filter)
      self.xctestrun_data['TestTargetName'].update(
        {'OnlyTestIdentifiers': filter})
    elif test_filter:
      if invert:
        self.xctestrun_data['TestTargetName'].update(
          {'SkipTestIdentifiers': test_filter})
      else:
        self.xctestrun_data['TestTargetName'].update(
          {'OnlyTestIdentifiers': test_filter})

  def get_launch_command(self, test_filter=None, invert=False):
    """Returns the command that can be used to launch the test app.

    Args:
      test_filter: List of test cases to filter.
      invert: Whether to invert the filter or not. Inverted, the filter will
        match everything except the given test cases.

    Returns:
      A list of strings forming the command to launch the test.
    """
    if self.xctest_path:
      self.set_xctest_filters(test_filter, invert)
      if self.env_vars:
        self.xctestrun_data['TestTargetName'].update(
          {'EnvironmentVariables': self.env_vars})
      if self.test_args:
        self.xctestrun_data['TestTargetName'].update(
          {'CommandLineArguments': self.test_args})
      plistlib.writePlist(self.xctestrun_data, self.xctestrun_file)
      return [
        'xcodebuild',
        'test-without-building',
        '-xctestrun', self.xctestrun_file,
        '-destination', 'id=%s' % self.udid,
      ]

    cmd = [
      'idevice-app-runner',
      '--udid', self.udid,
      '--start', self.cfbundleid,
    ]
    args = []

    if test_filter:
      kif_filter = get_kif_test_filter(test_filter, invert=invert)
      gtest_filter = get_gtest_filter(test_filter, invert=invert)
      cmd.extend(['-D', 'GKIF_SCENARIO_FILTER=%s' % kif_filter])
      args.append('--gtest_filter=%s' % gtest_filter)

    for env_var in self.env_vars:
      cmd.extend(['-D', env_var])

    if args or self.test_args:
      cmd.append('--args')
      cmd.extend(self.test_args)
      cmd.extend(args)

    return cmd

  def get_launch_env(self):
    """Returns a dict of environment variables to use to launch the test app.

    Returns:
      A dict of environment variables.
    """
    env = super(DeviceTestRunner, self).get_launch_env()
    if self.xctest_path:
      env['NSUnbufferedIO'] = 'YES'
      # e.g. ios_web_shell_egtests
      env['APP_TARGET_NAME'] = os.path.splitext(
          os.path.basename(self.app_path))[0]
      # e.g. ios_web_shell_egtests_module
      env['TEST_TARGET_NAME'] = env['APP_TARGET_NAME'] + '_module'
    return env
