#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Web Platform Tests (WPT) on Android browsers.

This script supports running tests on the Chromium Waterfall by mapping isolated
script flags to WPT flags.

It is also useful for local reproduction by performing APK installation and
configuring the browser to resolve test hosts.  Be sure to invoke this
executable directly rather than using python run_android_wpt.py so that
WPT dependencies in Chromium vpython are found.

If you need more advanced test control, please use the runner located at
//third_party/blink/web_tests/external/wpt/wpt.

Here's the mapping [isolate script flag] : [wpt flag]
--isolated-script-test-output : --log-chromium
--total-shards : --total-chunks
--shard-index : -- this-chunk
"""

# TODO(aluo): Combine or factor out commons parts with run_wpt_tests.py script.

import contextlib
import json
import os
import sys

import common

SRC_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))

BUILD_ANDROID = os.path.join(SRC_DIR, 'build', 'android')
PLATFORM_TOOLS = os.path.join(SRC_DIR, 'third_party', 'android_sdk', 'public',
                              'platform-tools')

if BUILD_ANDROID not in sys.path:
  sys.path.append(BUILD_ANDROID)

from pylib.constants import host_paths
if host_paths.DEVIL_PATH not in sys.path:
  sys.path.append(host_paths.DEVIL_PATH)

from devil.android import apk_helper
from devil.android import device_utils
from devil.android import flag_changer
from devil.android.tools import system_app
from devil.android.tools import webview_app

DEFAULT_WEBDRIVER = os.path.join(SRC_DIR, 'chrome', 'test', 'chromedriver',
                                 'cipd', 'linux', 'chromedriver')
DEFAULT_WPT = os.path.join(SRC_DIR, 'third_party', 'blink', 'web_tests',
                           'external', 'wpt', 'wpt')

SYSTEM_WEBVIEW_SHELL_PKG = 'org.chromium.webview_shell'

# This avoids having to update the hosts file on device.
HOST_RESOLVER_ARGS = ['--host-resolver-rules=MAP nonexistent.*.test ~NOTFOUND,'
                      ' MAP *.test 127.0.0.1']

# Browsers on debug and eng devices read command-line-flags from special files
# during startup.
FLAGS_FILE_MAP = {'android_webview': 'webview-command-line',
                  'chrome_android': 'chrome-command-line'}

class WPTAndroidAdapter(common.BaseIsolatedScriptArgsAdapter):
  def generate_test_output_args(self, output):
    return ['--log-chromium', output]

  def generate_sharding_args(self, total_shards, shard_index):
    return ['--total-chunks=%d' % total_shards,
        # shard_index is 0-based but WPT's this-chunk to be 1-based
        '--this-chunk=%d' % (shard_index + 1)]

  @property
  def rest_args(self):
    rest_args = super(WPTAndroidAdapter, self).rest_args

    # Here we add all of the arguments required to run WPT tests on Android.
    rest_args.extend([self.options.wpt_path])

    # vpython has packages needed by wpt, so force it to skip the setup
    rest_args.extend(["--venv=../../", "--skip-venv-setup"])

    rest_args.extend(["run",
      "--test-type=" + self.options.test_type,
      self.options.product,
      "--webdriver-binary",
      self.options.webdriver_binary,
      "--headless",
      "--no-pause-after-test",
      "--no-capture-stdio",
      "--no-manifest-download",
      "--no-fail-on-unexpected",
      #TODO(aluo): Tune this as tests are stabilized
      "--timeout-multiplier",
      "0.25",
    ])

    # Default to the apk's package name for chrome_android
    if not self.options.package_name:
      if self.options.product == 'chrome_android':
        if self.options.apk:
          pkg = apk_helper.GetPackageName(self.options.apk)
          print("Defaulting --package-name to that of the apk: {}.".format(pkg))
          rest_args.extend(['--package-name', pkg])
        else:
          raise Exception('chrome_android requires --package-name or --apk.')
    else:
      rest_args.extend(['--package-name', self.options.package_name])

    if self.options.include:
      for i in self.options.include:
        rest_args.extend(['--include', i])

    if self.options.list_tests:
      rest_args.extend(['--list-tests'])

    return rest_args

  def add_extra_arguments(self, parser):
    parser.add_argument('--webdriver-binary', default=DEFAULT_WEBDRIVER,
                        help='Path of the webdriver binary.  It needs to have'
                        ' the same major version as the apk.  Defaults to cipd'
                        ' archived version (near ToT).')
    parser.add_argument('--wpt-path', default=DEFAULT_WPT,
                        help='Controls the path of the WPT runner to use'
                        ' (therefore tests).  Defaults the revision rolled into'
                        ' Chromium.')
    parser.add_argument('--test-type', default='testharness',
                        help='Specify to experiment with other test types.'
                        ' Currently only the default is expected to work.')
    parser.add_argument('--product', choices = FLAGS_FILE_MAP.keys(),
                        required=True)
    parser.add_argument('--apk', help='Apk to install during test.  Defaults to'
                        ' the on-device WebView provider or Chrome.')
    parser.add_argument('--system-webview-shell', help='System'
                        ' WebView Shell apk to install during test.  Defaults'
                        ' to the on-device WebView Shell apk.')
    parser.add_argument('--package-name', help='The package name of Chrome'
                        ' to test, defaults to that of the --apk.')
    parser.add_argument('--binary-arg', action='append', default=[],
                        help='Additional command line flags to set during'
                        ' test execution.')
    parser.add_argument('--include', action='append', default=[],
                        help='Test(s) to run, defaults to run all tests.')
    parser.add_argument('--list-tests', action='store_true',
                        help="Don't run any tests, just print out a list of"
                        ' tests that would be run.')


def run_android_webview(device, adapter):
  if adapter.options.package_name:
    print('WARNING: --package-name has no effect for android_webview, provider'
          'will be set to the --apk if it is provided.')

  if adapter.options.system_webview_shell:
    shell_pkg = apk_helper.GetPackageName(adapter.options.system_webview_shell)
    if shell_pkg != SYSTEM_WEBVIEW_SHELL_PKG:
      raise Exception('{} has incorrect package name: {}, expected {}.'.format(
          '--system-webview-shell apk', shell_pkg, SYSTEM_WEBVIEW_SHELL_PKG))
    install_shell_as_needed = system_app.ReplaceSystemApp(device, shell_pkg,
        adapter.options.system_webview_shell)
  else:
    install_shell_as_needed = no_op()

  if adapter.options.apk:
    install_webview_as_needed = webview_app.UseWebViewProvider(device,
        adapter.options.apk)
  else:
    install_webview_as_needed = no_op()

  with install_shell_as_needed, install_webview_as_needed:
    return adapter.run_test()


def run_chrome_android(device, adapter):
  if adapter.options.apk:
    with app_installed(device, adapter.options.apk):
      return adapter.run_test()
  else:
    return adapter.run_test()


@contextlib.contextmanager
def app_installed(device, apk):
  pkg = apk_helper.GetPackageName(apk)
  device.Install(apk)
  try:
    yield
  finally:
    device.Uninstall(pkg)


# Dummy contextmanager to simplify multiple optional managers.
@contextlib.contextmanager
def no_op():
  yield


# This is not really a "script test" so does not need to manually add
# any additional compile targets.
def main_compile_targets(args):
    json.dump([], args.output)


def main():
  adapter = WPTAndroidAdapter()
  adapter.parse_args()

  # Only 1 device is supported for Android locally, this will work well with
  # sharding support via swarming infra.
  device = device_utils.DeviceUtils.HealthyDevices()[0]

  flags_file = FLAGS_FILE_MAP[adapter.options.product]
  all_flags = HOST_RESOLVER_ARGS + adapter.options.binary_arg
  flags = flag_changer.CustomCommandLineFlags(device, flags_file, all_flags)

  # WPT setup for chrome and webview requires that PATH contains adb.
  os.environ['PATH'] = ':'.join([PLATFORM_TOOLS] +
                                os.environ['PATH'].split(':'))

  with flags:
    if adapter.options.product == 'android_webview':
      run_android_webview(device, adapter)
    elif adapter.options.product == 'chrome_android':
      run_chrome_android(device, adapter)


if __name__ == '__main__':
    # Conform minimally to the protocol defined by ScriptTest.
    if 'compile_targets' in sys.argv:
        funcs = {
            'run': None,
            'compile_targets': main_compile_targets,
        }
        sys.exit(common.run_script(sys.argv[1:], funcs))
    sys.exit(main())
