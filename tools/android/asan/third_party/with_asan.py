#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import argparse
import contextlib
import os
import subprocess
import sys

_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))

sys.path.append(os.path.join(_SRC_ROOT, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils
from devil.android.sdk import version_codes

sys.path.append(os.path.join(_SRC_ROOT, 'build', 'android'))
import devil_chromium

_SCRIPT_PATH = os.path.abspath(
    os.path.join(
        os.path.dirname(__file__),
        'asan_device_setup.sh'))


@contextlib.contextmanager
def Asan(args):
  env = os.environ.copy()
  env['ADB'] = args.adb

  device = device_utils.DeviceUtils.HealthyDevices(
      device_arg=args.device)[0]
  disable_verity = device.build_version_sdk >= version_codes.MARSHMALLOW
  try:
    if disable_verity:
      device.EnableRoot()
      device.adb.DisableVerity()
      device.Reboot()
    # Call EnableRoot prior to asan_device_setup.sh to ensure it doesn't
    # get tripped up by the root timeout.
    device.EnableRoot()
    setup_cmd = [_SCRIPT_PATH, '--lib', args.lib]
    if args.device:
      setup_cmd += ['--device', args.device]
    subprocess.check_call(setup_cmd, env=env)
    yield
  finally:
    teardown_cmd = [_SCRIPT_PATH, '--revert']
    if args.device:
      teardown_cmd += ['--device', args.device]
    subprocess.check_call(teardown_cmd, env=env)
    if disable_verity:
      device.adb.EnableVerity()
      device.Reboot()


def main(raw_args):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--adb', type=os.path.realpath, required=True,
      help='Path to adb binary.')
  parser.add_argument(
      '--device',
      help='Device serial.')
  parser.add_argument(
      '--lib', type=os.path.realpath, required=True,
      help='Path to asan library.')
  parser.add_argument(
      'command', nargs='*',
      help='Command to run with ASAN installed.')
  args = parser.parse_args()

  devil_chromium.Initialize(adb_path=args.adb)

  with Asan(args):
    if args.command:
      return subprocess.call(args.command)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
