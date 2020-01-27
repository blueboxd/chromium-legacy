# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import subprocess

import test_runner

LOGGER = logging.getLogger(__name__)


def get_simulator_list():
  """Gets list of available simulator as a dictionary."""
  return json.loads(subprocess.check_output(['xcrun', 'simctl', 'list', '-j']))


def get_simulator(platform, version):
  """Gets a simulator or creates a new one if not exist by platform and version

    Returns:
      A udid of a simulator device.
  """
  udids = get_simulator_udids_by_platform_and_sdk(platform, version)
  if udids:
    return udids[0]
  simctl_list = get_simulator_list()
  return create_device_by_platform_and_version(
      get_simulator_runtime_by_platform(simctl_list, platform), version)


def get_simulator_runtime_by_platform(simulators, platform):
  """Gets runtime identifier for platform.

  Args:
    simulators: (dict) A list of available simulators.
    platform: (str) A platform name, e.g. "iPhone 11 Pro"
  """
  for device in simulators['devicetypes']:
    if device['name'] == platform:
      return device['identifier']
  raise test_runner.SimulatorNotFoundError(
      'Not found device "%s" in devicetypes %s' % (platform,
                                                   simulators['devicetypes']))


def get_simulator_runtime_by_version(simulators, version):
  """Gets runtime based on iOS version.

  Args:
    simulators: (dict) A list of available simulators.
    version: (str) A version name, e.g. "13.2.2"
  """
  for runtime in simulators['runtimes']:
    if runtime['version'] == version and 'iOS' in runtime['name']:
      return runtime['identifier']
  raise test_runner.SimulatorNotFoundError(
      'Not found "%s" SDK in runtimes %s' % (version, simulators['runtimes']))


def get_simulator_runtime_by_device_udid(simulator_udid):
  """Gets simulator runtime based on simulator UDID.

  Args:
    simulator_udid: (str) UDID of a simulator.
  """
  simulator_list = get_simulator_list()['devices']
  for runtime, simulators in simulator_list.items():
    for device in simulators:
      if simulator_udid == device['udid']:
        return runtime
  raise test_runner.SimulatorNotFoundError(
      'Not found simulator with "%s" UDID in devices %s' % (simulator_udid,
                                                            simulator_list))


def get_simulator_udids_by_platform_and_sdk(platform, version):
  """Gets list of simulators UDID based on platform name and iOS version.

    Args:
      platform: (str) A platform name, e.g. "iPhone 11"
      version: (str) A version name, e.g. "13.2.2"
  """
  simulators = get_simulator_list()
  devices = simulators['devices']
  sdk_id = get_simulator_runtime_by_version(simulators, version)
  results = []
  for device in devices.get(sdk_id, []):
    if device['name'] == platform:
      results.append(device['udid'])
  return results


def create_device_by_platform_and_version(platform, version):
  """Creates a simulator and returns UDID of it.

    Args:
      platform: (str) A platform name, e.g. "iPhone 11"
      version: (str) A version name, e.g. "13.2.2"
  """
  name = '%s test' % platform
  LOGGER.info('Creating simulator %s', name)
  udid = subprocess.check_output([
      'xcrun', 'simctl', 'create', name, platform,
      get_simulator_runtime_by_version(get_simulator_list(), version)
  ]).rstrip()
  LOGGER.info('Created simulator with UDID: %s', udid)
  return udid


def delete_simulator_by_udid(udid):
  """Deletes simulator by its udid.

  Args:
    udid: (str) UDID of simulator.
  """
  LOGGER.info('Deleting simulator %s', udid)
  subprocess.call(['xcrun', 'simctl', 'delete', udid])


def wipe_simulator_by_udid(udid):
  """Wipes simulators by its udid.

  Args:
    udid: (str) UDID of simulator.
  """
  for _, devices in get_simulator_list()['devices'].items():
    for device in devices:
      if device['udid'] != udid:
        continue
      try:
        LOGGER.info('Shutdown simulator %s ', device)
        if device['state'] != 'Shutdown':
          subprocess.check_call(['xcrun', 'simctl', 'shutdown', device['udid']])
      except subprocess.CalledProcessError as ex:
        LOGGER.info('Shutdown failed %s ', ex)
      subprocess.check_call(['xcrun', 'simctl', 'erase', device['udid']])


def get_home_directory(platform, version):
  """Gets directory where simulators are stored.

  Args:
    platform: (str) A platform name, e.g. "iPhone 11"
    version: (str) A version name, e.g. "13.2.2"
  """
  return subprocess.check_output(
      ['xcrun', 'simctl', 'getenv',
       get_simulator(platform, version), 'HOME']).rstrip()


def is_device_with_udid_simulator(device_udid):
  """Checks whether a device with udid is simulator or not.

  Args:
    device_udid: (str) UDID of a device.
  """
  simulator_list = get_simulator_list()['devices']
  for _, simulators in simulator_list.items():
    for device in simulators:
      if device_udid == device['udid']:
        return True
  return False
