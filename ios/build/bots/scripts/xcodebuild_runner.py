# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test runner for running tests using xcodebuild."""

import sys

import collections
import distutils.version
import logging
import multiprocessing
import os
import plistlib
import subprocess
import threading
import time

import test_runner
import xcode_log_parser

LOGGER = logging.getLogger(__name__)


class LaunchCommandCreationError(test_runner.TestRunnerError):
  """One of launch command parameters was not set properly."""

  def __init__(self, message):
    super(LaunchCommandCreationError, self).__init__(message)


class LaunchCommandPoolCreationError(test_runner.TestRunnerError):
  """Failed to create a pool of launch commands."""

  def __init__(self, message):
    super(LaunchCommandPoolCreationError, self).__init__(message)


def terminate_process(proc):
  """Terminates the process.

  If an error occurs ignore it, just print out a message.

  Args:
    proc: A subprocess.
  """
  try:
    proc.terminate()
  except OSError as ex:
    LOGGER.info('Error while killing a process: %s' % ex)


class EgtestsApp(object):
  """Egtests to run.

  Stores data about egtests:
    egtests_app: full path to egtests app.
    project_path: root project folder.
    module_name: egtests module name.
    filtered_tests: List of tests to include or exclude, depending on `invert`.
    invert: type of filter(True - inclusive, False - exclusive).
  """

  def __init__(self, egtests_app, filtered_tests=None, invert=False,
               test_args=None, env_vars=None, host_app_path=None):
    """Initialize Egtests.

    Args:
      egtests_app: (str) full path to egtests app.
      filtered_tests: (list) Specific tests to run
        (it can inclusive/exclusive based on invert parameter).
         E.g.
          [ 'TestCaseClass1/testMethod1', 'TestCaseClass2/testMethod2']
      invert: type of filter(True - inclusive, False - exclusive).
      test_args: List of strings to pass as arguments to the test when
        launching.
      env_vars: List of environment variables to pass to the test itself.
      host_app_path: (str) full path to host app.

    Raises:
      AppNotFoundError: If the given app does not exist
    """
    if not os.path.exists(egtests_app):
      raise test_runner.AppNotFoundError(egtests_app)
    self.egtests_path = egtests_app
    self.project_path = os.path.dirname(self.egtests_path)
    self.module_name = os.path.splitext(os.path.basename(egtests_app))[0]
    self.filter = filtered_tests
    self.invert = invert
    self.test_args = test_args
    self.env_vars = env_vars
    self.host_app_path = host_app_path

  def _xctest_path(self):
    """Gets xctest-file from egtests/PlugIns folder.

    Returns:
      A path for xctest in the format of /PlugIns/file.xctest

    Raises:
      PlugInsNotFoundError: If no PlugIns folder found in egtests.app.
      XCTestPlugInNotFoundError: If no xctest-file found in PlugIns.
    """
    plugins_dir = os.path.join(self.egtests_path, 'PlugIns')
    if not os.path.exists(plugins_dir):
      raise test_runner.PlugInsNotFoundError(plugins_dir)
    plugin_xctest = None
    if os.path.exists(plugins_dir):
      for plugin in os.listdir(plugins_dir):
        if plugin.endswith('.xctest'):
          plugin_xctest = os.path.join(plugins_dir, plugin)
    if not plugin_xctest:
      raise test_runner.XCTestPlugInNotFoundError(plugin_xctest)
    return plugin_xctest.replace(self.egtests_path, '')

  def xctestrun_node(self):
    """Fills only required nodes for egtests in xctestrun file.

    Returns:
      A node with filled required fields about egtests.
    """
    module = self.module_name + '_module'
    module_data = {
            'TestBundlePath': '__TESTHOST__%s' % self._xctest_path(),
            'TestHostPath': '%s' % self.egtests_path,
            'TestingEnvironmentVariables': {
                'DYLD_INSERT_LIBRARIES': (
                    '__PLATFORMS__/iPhoneSimulator.platform/Developer/'
                    'usr/lib/libXCTestBundleInject.dylib'),
                'DYLD_LIBRARY_PATH': self.project_path,
                'DYLD_FRAMEWORK_PATH': self.project_path + ':',
                'XCInjectBundleInto': '__TESTHOST__/%s' % self.module_name
            }
        }
    # Add module data specific to EG2 or EG1 tests
    # EG2 tests
    if self.host_app_path:
      module_data['IsUITestBundle'] = True
      module_data['IsXCTRunnerHostedTestBundle'] = True
      module_data['UITargetAppPath'] = '%s' % self.host_app_path
      # Special handling for Xcode10.2
      dependent_products = [
        module_data['UITargetAppPath'],
        module_data['TestBundlePath'],
        module_data['TestHostPath']
      ]
      module_data['DependentProductPaths'] = dependent_products
    # EG1 tests
    else:
      module_data['IsAppHostedTestBundle'] = True

    xctestrun_data = {
        module: module_data
    }
    if self.filter:
      if self.invert:
        xctestrun_data[module].update(
            {'SkipTestIdentifiers': self.filter})
      else:
        xctestrun_data[module].update(
            {'OnlyTestIdentifiers': self.filter})
    if self.env_vars:
      xctestrun_data[module].update(
          {'EnvironmentVariables': self.env_vars})
    if self.test_args:
      xctestrun_data[module].update(
          {'CommandLineArguments': self.test_args})
    return xctestrun_data


class LaunchCommand(object):
  """Stores xcodebuild test launching command."""

  def __init__(self, egtests_app, destination,
               shards,
               retries,
               out_dir=os.path.basename(os.getcwd()),
               env=None):
    """Initialize launch command.

    Args:
      egtests_app: (EgtestsApp) An egtests_app to run.
      destination: (str) A destination.
      shards: (int) A number of shards.
      retries: (int) A number of retries.
      out_dir: (str) A folder in which xcodebuild will generate test output.
        By default it is a current directory.
      env: (dict) Environment variables.

    Raises:
      LaunchCommandCreationError: if one of parameters was not set properly.
    """
    if not isinstance(egtests_app, EgtestsApp):
      raise test_runner.AppNotFoundError(
          'Parameter `egtests_app` is not EgtestsApp: %s' % egtests_app)
    self.egtests_app = egtests_app
    self.destination = destination
    self.shards = shards
    self.retries = retries
    self.out_dir = out_dir
    self.logs = collections.OrderedDict()
    self.test_results = collections.OrderedDict()
    self.env = env
    if distutils.version.LooseVersion('11.0') <= distutils.version.LooseVersion(
        test_runner.get_current_xcode_info()['version']):
      self._log_parser = xcode_log_parser.Xcode11LogParser()
    else:
      self._log_parser = xcode_log_parser.XcodeLogParser()

  def _make_cmd_list_for_failed_tests(self, failed_results, out_dir,
                                      test_args=None, env_vars=None):
    """Makes cmd list based on failure results.

    Args:
      failed_results: Map of failed tests, where key is name of egtests_app and
        value is a list of failed_test_case/test_methods:
          {
              'failed_test_case/test_methods': ['StackTrace']
          }
      out_dir: (str) An output path.
      test_args: List of strings to pass as arguments to the test when
        launching.
      env_vars: List of environment variables to pass to the test itself.

    Returns:
      List of Launch commands to re-run failed tests.
      Every destination will run on separate clone of a stimulator.
    """
    eg_app = EgtestsApp(
        egtests_app=self.egtests_app.egtests_path,
        filtered_tests=[test.replace(' ', '/') for test in failed_results],
        test_args=test_args,
        env_vars=env_vars,
        host_app_path=self.egtests_app.host_app_path)
    # Regenerates xctest run and gets a command.
    return self.command(eg_app, out_dir, self.destination, shards=1)

  def summary_log(self):
    """Calculates test summary - how many passed, failed and error tests.

    Returns:
      Dictionary with number of passed and failed tests.
      Failed tests will be calculated from the last test attempt.
      Passed tests calculated for each test attempt.
    """
    test_statuses = ['passed', 'failed']
    for status in test_statuses:
      self.logs[status] = 0

    for index, test_attempt_results in enumerate(self.test_results['attempts']):
      for test_status in test_statuses:
        if test_status not in test_attempt_results:
          continue
        if (test_status == 'passed'
            # Number of failed tests is taken only from last run.
            or (test_status == 'failed'
                and index == len(self.test_results['attempts']) - 1)):
          self.logs[test_status] += len(test_attempt_results[test_status])

  def launch_attempt(self, cmd, out_dir):
    """Launch a process and do logging simultaneously.

    Args:
      cmd: (list[str]) A command to run.
      out_dir: (str) Output directory given to the command. Used in tests only.

    Returns:
      returncode - return code of command run.
    """
    LOGGER.info('Launching %s with env %s' % (cmd, self.env))
    proc = subprocess.Popen(
        cmd,
        env=self.env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

    while True:
      # It seems that subprocess.stdout.readline() is stuck from time to time
      # and tests fail because of TIMEOUT.
      # Try to fix the issue by adding timer-thread for 3 mins
      # that will kill `frozen` running process if no new line is read
      # and will finish test attempt.
      # If new line appears in 3 mins, just cancel timer.
      timer = threading.Timer(test_runner.READLINE_TIMEOUT,
                              terminate_process, [proc])
      timer.start()
      line = proc.stdout.readline()
      timer.cancel()
      if not line:
        break
      line = line.rstrip()
      LOGGER.info(line)
      sys.stdout.flush()

    proc.wait()
    LOGGER.info('Command %s finished with %d' % (cmd, proc.returncode))
    return proc.returncode

  def launch(self):
    """Launches tests using xcodebuild."""
    cmd_list = []
    self.test_results['attempts'] = []

    # total number of attempts is self.retries+1
    for attempt in range(self.retries + 1):
      outdir_attempt = os.path.join(self.out_dir, 'attempt_%d' % attempt)
      # Create a command for the 1st run or if tests did not start,
      # re-run the same command but with different output folder.
      # (http://crbug.com/916620) If tests did not start, repeat the command.
      if (not self.test_results['attempts'] or
          {'TESTS_DID_NOT_START', 'BUILD_INTERRUPTED'}.intersection(
              self.test_results['attempts'][-1]['failed'].keys())):
        cmd_list = self.command(self.egtests_app,
                                outdir_attempt,
                                self.destination,
                                self.shards)
      # Re-init the command based on list of failed tests.
      else:
        cmd_list = self._make_cmd_list_for_failed_tests(
            self.test_results['attempts'][-1]['failed'],
            outdir_attempt,
            test_args=self.egtests_app.test_args,
            env_vars=self.egtests_app.env_vars)

      # TODO(crbug.com/914878): add heartbeat logging to xcodebuild_runner.
      LOGGER.info('Start test attempt #%d for command [%s]' % (
          attempt, ' '.join(cmd_list)))
      self.launch_attempt(cmd_list, outdir_attempt)
      self.test_results['attempts'].append(
          self._log_parser.collect_test_results(outdir_attempt))
      if self.retries == attempt or not self.test_results[
          'attempts'][-1]['failed']:
        break
      self._log_parser.copy_screenshots(outdir_attempt)

    self.test_results['end_run'] = int(time.time())
    self.summary_log()

    return {
        'test_results': self.test_results,
        'logs': self.logs
    }

  def fill_xctest_run(self, egtests_app):
    """Fills xctestrun file by egtests.

    Args:
      egtests_app: (EgtestsApp) An Egetsts_app to run.

    Returns:
      A path to xctestrun file.

    Raises:
      AppNotFoundError if egtests is empty.
    """
    if not egtests_app:
      raise test_runner.AppNotFoundError('Egtests is not found!')
    xctestrun = os.path.join(
        os.path.abspath(os.path.join(self.out_dir, os.pardir)),
        'run_%d.xctestrun' % int(time.time()))
    if not os.path.exists(xctestrun):
      with open(xctestrun, 'w'):
        pass
    # Creates a dict with data about egtests to run - fill all required fields:
    # egtests_module, egtest_app_path, egtests_xctest_path and
    # filtered tests if filter is specified.
    # Write data in temp xctest run file.
    plistlib.writePlist(egtests_app.xctestrun_node(), xctestrun)
    return xctestrun

  def command(self, egtests_app, out_dir, destination, shards):
    """Returns the command that launches tests using xcodebuild.

    Format of command:
    xcodebuild test-without-building -xctestrun file.xctestrun \
      -parallel-testing-enabled YES -parallel-testing-worker-count %d% \
      [-destination "destination"]  -resultBundlePath %output_path%

    Args:
      egtests_app: (EgtestsApp) An egetsts_app to run.
      out_dir: (str) An output directory.
      destination: (str) A destination of running simulator.
      shards: (int) A number of shards.

    Returns:
      A list of strings forming the command to launch the test.
    """
    cmd = ['xcodebuild', 'test-without-building',
           '-xctestrun', self.fill_xctest_run(egtests_app),
           '-destination', destination,
           '-resultBundlePath', out_dir]
    if self.shards > 1:
      cmd += ['-parallel-testing-enabled', 'YES',
              '-parallel-testing-worker-count', str(shards)]
    return cmd


class SimulatorParallelTestRunner(test_runner.SimulatorTestRunner):
  """Class for running simulator tests using xCode."""

  def __init__(
      self,
      app_path,
      host_app_path,
      iossim_path,
      xcode_build_version,
      version,
      platform,
      out_dir,
      mac_toolchain=None,
      retries=1,
      shards=1,
      xcode_path=None,
      test_cases=None,
      test_args=None,
      env_vars=None
  ):
    """Initializes a new instance of SimulatorParallelTestRunner class.

    Args:
      app_path: (str) A path to egtests_app.
      host_app_path: (str) A path to the host app for EG2.
      iossim_path: Path to the compiled iossim binary to use.
                   Not used, but is required by the base class.
      xcode_build_version: (str) Xcode build version for running tests.
      version: (str) iOS version to run simulator on.
      platform: (str) Name of device.
      out_dir: (str) A directory to emit test data into.
      mac_toolchain: (str) A command to run `mac_toolchain` tool.
      retries: (int) A number to retry test run, will re-run only failed tests.
      shards: (int) A number of shards. Default is 1.
      xcode_path: (str) A path to Xcode.app folder.
      test_cases: (list) List of tests to be included in the test run.
                  None or [] to include all tests.
      test_args: List of strings to pass as arguments to the test when
        launching.
      env_vars: List of environment variables to pass to the test itself.

    Raises:
      AppNotFoundError: If the given app does not exist.
      PlugInsNotFoundError: If the PlugIns directory does not exist for XCTests.
      XcodeVersionNotFoundError: If the given Xcode version does not exist.
      XCTestPlugInNotFoundError: If the .xctest PlugIn does not exist.
    """
    super(SimulatorParallelTestRunner, self).__init__(
        app_path,
        iossim_path,
        platform,
        version,
        xcode_build_version,
        out_dir,
        env_vars=env_vars,
        mac_toolchain=mac_toolchain,
        retries=retries or 1,
        shards=shards or 1,
        test_args=test_args,
        test_cases=test_cases,
        xcode_path=xcode_path,
        xctest=False
    )
    self.set_up()
    self.erase_all_simulators()
    self.host_app_path = None
    if host_app_path != 'NO_PATH':
      self.host_app_path = os.path.abspath(host_app_path)
    self._init_sharding_data()
    self.logs = collections.OrderedDict()
    self.test_results = collections.OrderedDict()
    self.test_results['start_run'] = int(time.time())
    self.test_results['end_run'] = None

  def _init_sharding_data(self):
    """Initialize sharding data.

    For common case info about sharding tests will be a list of dictionaries:
    [
        {
            'app':paths to egtests_app,
            'destination': 'platform=iOS Simulator,OS=<os>,Name=<simulator>'
            'shards': N
        }
    ]
    """
    self.sharding_data = [
        {
            'app': self.app_path,
            'host': self.host_app_path,
            # Destination is required to run tests via xcodebuild and it
            # looks like
            # 'platform=iOS Simulator,OS=<os_version>,Name=<simulator-name>'
            # By default all tests runs on 'platform=iOS Simulator'.
            'destination': 'platform=iOS Simulator,OS=%s,name=%s' % (
                self.version, self.platform),
            'shards': self.shards,
            'test_cases': self.test_cases
        }
    ]

  def get_launch_env(self):
    """Returns a dict of environment variables to use to launch the test app.

    Returns:
      A dict of environment variables.
    """
    env = super(test_runner.SimulatorTestRunner, self).get_launch_env()
    env['NSUnbufferedIO'] = 'YES'
    return env

  def launch(self):
    """Launches tests using xcodebuild."""
    destinaion_folder = lambda dest: dest.replace(
        'platform=iOS Simulator,', '').replace(',name=', ' ').replace('OS=', '')
    launch_commands = []
    for params in self.sharding_data:
      launch_commands.append(LaunchCommand(
          EgtestsApp(params['app'], filtered_tests=params['test_cases'],
                     env_vars=self.env_vars, test_args=self.test_args,
                     host_app_path=params['host']),
          params['destination'],
          shards=params['shards'],
          retries=self.retries,
          out_dir=os.path.join(self.out_dir,
                               destinaion_folder(params['destination'])),
          env=self.get_launch_env()))

    pool = multiprocessing.pool.ThreadPool(len(launch_commands))
    attempts_results = []
    for result in pool.imap_unordered(LaunchCommand.launch, launch_commands):
      attempts_results.append(result['test_results']['attempts'])
    self.test_results['end_run'] = int(time.time())

    # Gets passed tests
    self.logs['passed tests'] = []
    for shard_attempts in attempts_results:
      for attempt in shard_attempts:
        self.logs['passed tests'].extend(attempt['passed'])

    # If the last attempt does not have failures, mark failed as empty
    self.logs['failed tests'] = []
    for shard_attempts in attempts_results:
      if shard_attempts[-1]['failed']:
        self.logs['failed tests'].extend(shard_attempts[-1]['failed'].keys())

    # Gets all failures/flakes and lists them in bot summary
    all_failures = set()
    for shard_attempts in attempts_results:
      for attempt, attempt_results in enumerate(shard_attempts):
        for failure in attempt_results['failed']:
          if failure not in self.logs:
            self.logs[failure] = []
          self.logs[failure].append('%s: attempt # %d' % (failure, attempt))
          self.logs[failure].extend(attempt_results['failed'][failure])
          all_failures.add(failure)

    # Gets only flaky(not failed) tests.
    self.logs['flaked tests'] = list(
        all_failures - set(self.logs['failed tests']))

    # Test is failed if there are failures for the last run.
    return not self.logs['failed tests']

  def erase_all_simulators(self):
    """Erases all simulator devices.

    Fix for DVTCoreSimulatorAdditionsErrorDomain error.
    """
    LOGGER.info('Erasing all simulators.')
    subprocess.call(['xcrun', 'simctl', 'erase', 'all'])
