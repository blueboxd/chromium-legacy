#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
If should_use_hermetic_xcode.py emits "1", and the current toolchain is out of
date:
  * Downloads the hermetic mac toolchain
    * Requires CIPD authentication. Run `cipd auth-login`, use Google account.
  * Accepts the license.
    * If xcode-select and xcodebuild are not passwordless in sudoers, requires
      user interaction.
  * Downloads standalone binaries from [a possibly different version of Xcode].

The toolchain version can be overridden by setting MAC_TOOLCHAIN_REVISION with
the full revision, e.g. 9A235.
"""

from __future__ import print_function

import os
import pkg_resources
import platform
import plistlib
import shutil
import subprocess
import sys


# This can be changed after running:
#    mac_toolchain upload -xcode-path path/to/Xcode.app
# The hermetic install of Xcode is used:
#  1) For sizes support
#  2) To build clang
#  3) For code-coverage support.
# These should eventually be phased out to use the new deployment of
# xcode_binaries, see InstallXcodeBinaries. https://crbug.com/984746
MAC_TOOLCHAIN_VERSION = '9E501'

# This contains binaries from Xcode 9.3.1, along with the 10.13 SDK.
# To build this package, see comments in build/xcode_binaries.yaml
MAC_BINARIES_LABEL = 'infra_internal/ios/xcode/xcode_binaries/mac-amd64'
MAC_BINARIES_TAG = 'oXeIQqw4E0pXIStoixHwKg_1TJZTJGhfhmn2fq8LPOAC'

# The toolchain will not be downloaded if the minimum OS version is not met.
# 17 is the major version number for macOS 10.13.
# 9E145 (Xcode 9.3) only runs on 10.13.2 and newer.
MAC_MINIMUM_OS_VERSION = 17

MAC_TOOLCHAIN_INSTALLER = 'mac_toolchain'

# Absolute path to src/ directory.
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Absolute path to a file with gclient solutions.
GCLIENT_CONFIG = os.path.join(os.path.dirname(REPO_ROOT), '.gclient')

BASE_DIR = os.path.abspath(os.path.dirname(__file__))
TOOLCHAIN_ROOT = os.path.join(BASE_DIR, 'mac_files')
TOOLCHAIN_BUILD_DIR = os.path.join(TOOLCHAIN_ROOT, 'Xcode.app')
STAMP_FILE = os.path.join(TOOLCHAIN_ROOT, 'toolchain_build_revision')


def PlatformMeetsHermeticXcodeRequirements():
  major_version = int(platform.release().split('.')[0])
  return major_version >= MAC_MINIMUM_OS_VERSION


def _UseHermeticToolchain():
  current_dir = os.path.dirname(os.path.realpath(__file__))
  script_path = os.path.join(current_dir, 'mac/should_use_hermetic_xcode.py')
  proc = subprocess.Popen([script_path, 'mac'], stdout=subprocess.PIPE)
  return '1' in proc.stdout.readline()


def RequestCipdAuthentication():
  """Requests that the user authenticate to access Xcode CIPD packages."""

  print('Access to Xcode CIPD package requires authentication.')
  print('-----------------------------------------------------------------')
  print()
  print('You appear to be a Googler.')
  print()
  print('I\'m sorry for the hassle, but you may need to do a one-time manual')
  print('authentication. Please run:')
  print()
  print('    cipd auth-login')
  print()
  print('and follow the instructions.')
  print()
  print('NOTE: Use your google.com credentials, not chromium.org.')
  print()
  print('-----------------------------------------------------------------')
  print()
  sys.stdout.flush()


def PrintError(message):
  # Flush buffers to ensure correct output ordering.
  sys.stdout.flush()
  sys.stderr.write(message + '\n')
  sys.stderr.flush()


def InstallXcode(xcode_build_version, installer_cmd, xcode_app_path):
  """Installs the requested Xcode build version.

  Args:
    xcode_build_version: (string) Xcode build version to install.
    installer_cmd: (string) Path to mac_toolchain command to install Xcode.
      See https://chromium.googlesource.com/infra/infra/+/master/go/src/infra/cmd/mac_toolchain/
    xcode_app_path: (string) Path to install the contents of Xcode.app.

  Returns:
    True if installation was successful. False otherwise.
  """
  args = [
      installer_cmd, 'install',
      '-kind', 'mac',
      '-xcode-version', xcode_build_version.lower(),
      '-output-dir', xcode_app_path,
  ]

  # Buildbot slaves need to use explicit credentials. LUCI bots should NOT set
  # this variable.
  creds = os.environ.get('MAC_TOOLCHAIN_CREDS')
  if creds:
    args.extend(['--service-account-json', creds])

  try:
    subprocess.check_call(args)
  except subprocess.CalledProcessError as e:
    PrintError('Xcode build version %s failed to install: %s\n' % (
        xcode_build_version, e))
    RequestCipdAuthentication()
    return False
  except OSError as e:
    PrintError(('Xcode installer "%s" failed to execute'
                ' (not on PATH or not installed).') % installer_cmd)
    return False

  return True


def InstallXcodeBinaries():
  """Installs the Xcode binaries needed to build Chrome and accepts the license.

  This is the replacement for InstallXcode that installs a trimmed down version
  of Xcode that is OS-version agnostic.
  """
  # First make sure the directory exists. It will serve as the cipd root. This
  # also ensures that there will be no conflicts of cipd root.
  binaries_root = os.path.join(TOOLCHAIN_ROOT, 'xcode_binaries')
  if not os.path.exists(binaries_root):
    os.mkdir(binaries_root)

  # 'cipd ensure' is idempotent.
  args = [
      'cipd', 'ensure', '-root', binaries_root, '-ensure-file', '-'
  ]
  p = subprocess.Popen(args, stdin=subprocess.PIPE)
  p.communicate(input=MAC_BINARIES_LABEL + ' ' + MAC_BINARIES_TAG)

  # Accept the license for this version of Xcode if it's newer than the
  # currently accepted version.
  cipd_xcode_version_plist_path = os.path.join(
      binaries_root, 'Contents/version.plist')
  cipd_xcode_version_plist = plistlib.readPlist(cipd_xcode_version_plist_path)
  cipd_xcode_version = cipd_xcode_version_plist['CFBundleShortVersionString']

  cipd_license_path = os.path.join(
      binaries_root, 'Contents/Resources/LicenseInfo.plist')
  cipd_license_plist = plistlib.readPlist(cipd_license_path)
  cipd_license_version = cipd_license_plist['licenseID']

  should_overwrite_license = True
  current_license_path = '/Library/Preferences/com.apple.dt.Xcode.plist'
  if os.path.exists(current_license_path):
    current_license_plist = plistlib.readPlist(current_license_path)
    xcode_version = current_license_plist['IDEXcodeVersionForAgreedToGMLicense']
    if (pkg_resources.parse_version(xcode_version) >=
        pkg_resources.parse_version(cipd_xcode_version)):
      should_overwrite_license = False

  if not should_overwrite_license:
    return

  # Use puppet's sudoers script to accept the license if its available.
  license_accept_script = '/usr/local/bin/xcode_accept_license.py'
  if os.path.exists(license_accept_script):
    args = ['sudo', license_accept_script, '--xcode_version',
            cipd_xcode_version, '--license-version', cipd_license_version]
    subprocess.call(args)
    return

  # Otherwise manually accept the license. This will prompt for sudo.
  print('Accepting new Xcode license.')
  args = ['sudo', 'defaults', 'write', current_license_path,
          'IDEXcodeVersionForAgreedToGMLicense', cipd_xcode_version]
  subprocess.call(args)
  args = ['sudo', 'defaults', 'write', current_license_path,
          'IDELastGMLicenseAgreedTo', cipd_license_version]
  subprocess.call(args)
  args = ['sudo', 'plutil', '-convert', 'xml1', current_license_path]
  subprocess.call(args)


def main():
  if sys.platform != 'darwin':
    return 0

  if not _UseHermeticToolchain():
    print('Skipping Mac toolchain installation for mac')
    return 0

  if not PlatformMeetsHermeticXcodeRequirements():
    print('OS version does not support toolchain.')
    return 0

  toolchain_version = os.environ.get('MAC_TOOLCHAIN_REVISION',
                                      MAC_TOOLCHAIN_VERSION)

  # On developer machines, mac_toolchain tool is provided by
  # depot_tools. On the bots, the recipe is responsible for installing
  # it and providing the path to the executable.
  installer_cmd = os.environ.get('MAC_TOOLCHAIN_INSTALLER',
                                 MAC_TOOLCHAIN_INSTALLER)

  toolchain_root = TOOLCHAIN_ROOT
  xcode_app_path = TOOLCHAIN_BUILD_DIR
  stamp_file = STAMP_FILE

  # Delete the old "hermetic" installation if detected.
  # TODO(crbug.com/797051): remove this once the old "hermetic" solution is no
  # longer in use.
  if os.path.exists(stamp_file):
    print(
        'Detected old hermetic installation at %s. Deleting.' % toolchain_root)
    shutil.rmtree(toolchain_root)

  success = InstallXcode(toolchain_version, installer_cmd, xcode_app_path)
  if not success:
    return 1

  InstallXcodeBinaries()

  return 0


if __name__ == '__main__':
  sys.exit(main())
