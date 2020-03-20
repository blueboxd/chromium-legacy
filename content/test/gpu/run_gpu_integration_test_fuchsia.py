#!/usr/bin/env vpython
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper for running gpu integration tests on Fuchsia devices."""

import argparse
import logging
import os
import subprocess
import sys
import time
from gpu_tests import path_util

sys.path.insert(0, os.path.join(path_util.GetChromiumSrcDir(),
    'build', 'fuchsia'))
from common_args import AddCommonArgs, ConfigureLogging, GetDeploymentTargetForArgs

def main():
  parser = argparse.ArgumentParser()
  AddCommonArgs(parser)
  args, gpu_test_args = parser.parse_known_args()
  ConfigureLogging(args)

  # If output directory is not set, assume the script is being launched
  # from the output directory.
  if not args.output_directory:
    args.output_directory = os.getcwd()

  gpu_script = [os.path.join(path_util.GetChromiumSrcDir(), 'content',
                'test', 'gpu', 'run_gpu_integration_test.py')]

  # Pass all other arguments to the gpu integration tests.
  gpu_script.extend(gpu_test_args)
  with GetDeploymentTargetForArgs(args) as target:
    target.Start()
    _, fuchsia_ssh_port = target._GetEndpoint()
    gpu_script.extend(['--fuchsia-ssh-config-dir', args.output_directory])
    gpu_script.extend(['--fuchsia-ssh-port', str(fuchsia_ssh_port)])

    web_engine_dir = os.path.join(args.output_directory, 'gen',
        'fuchsia', 'engine')

    # Install necessary packages on the device.
    target.InstallPackage([
        os.path.join(web_engine_dir, 'web_engine', 'web_engine.far'),
        os.path.join(web_engine_dir, 'web_engine_shell',
            'web_engine_shell.far')
    ])
    return subprocess.call(gpu_script)

if __name__ == '__main__':
  sys.exit(main())
