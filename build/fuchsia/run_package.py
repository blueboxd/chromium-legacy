# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Contains a helper function for deploying and executing a packaged
executable on a Target."""

from __future__ import print_function

import common
import hashlib
import logging
import multiprocessing
import os
import re
import select
import subprocess
import sys
import time
import threading
import uuid

from symbolizer import SymbolizerFilter

FAR = os.path.join(common.SDK_ROOT, 'tools', 'far')

# Amount of time to wait for the termination of the system log output thread.
_JOIN_TIMEOUT_SECS = 5


def _GetComponentUri(package_name):
  return 'fuchsia-pkg://fuchsia.com/%s#meta/%s.cmx' % (package_name,
                                                       package_name)


class RunPackageArgs:
  """RunPackage() configuration arguments structure.

  install_only: If set, skips the package execution step.
  symbolizer_config: A newline delimited list of source files contained
      in the package. Omitting this parameter will disable symbolization.
  system_logging: If set, connects a system log reader to the target.
  target_staging_path: Path to which package FARs will be staged, during
      installation. Defaults to staging into '/data'.
  """
  def __init__(self):
    self.install_only = False
    self.symbolizer_config = None
    self.system_logging = False
    self.target_staging_path = '/data'

  @staticmethod
  def FromCommonArgs(args):
    run_package_args = RunPackageArgs()
    run_package_args.install_only = args.install_only
    run_package_args.target_staging_path = args.target_staging_path
    return run_package_args


def RunPackage(output_dir, target, package_path, package_name,
               package_deps, package_args, args):
  """Installs the Fuchsia package at |package_path| on the target,
  executes it with |package_args|, and symbolizes its output.

  output_dir: The path containing the build output files.
  target: The deployment Target object that will run the package.
  package_path: The path to the .far package file.
  package_name: The name of app specified by package metadata.
  package_args: The arguments which will be passed to the Fuchsia process.
  args: Structure of arguments to configure how the package will be run.

  Returns the exit code of the remote package process."""
  target.InstallPackage(package_path, package_name, package_deps)

  if args.install_only:
    logging.info('Installation complete.')
    return

  logging.info('Running application.')
  command = ['run', _GetComponentUri(package_name)] + package_args
  process = target.RunCommandPiped(command,
                                   stdin=open(os.devnull, 'r'),
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT)

  # Run the log data through the symbolizer process.
  build_ids_paths = map(
      lambda package_path: os.path.join(
          os.path.dirname(package_path), 'ids.txt'),
      [package_path] + package_deps)
  output_stream = SymbolizerFilter(process.stdout, build_ids_paths)

  for next_line in output_stream:
    print(next_line.rstrip())

  process.wait()
  if process.returncode == 0:
    logging.info('Process exited normally with status code 0.')
  else:
    # The test runner returns an error status code if *any* tests fail,
    # so we should proceed anyway.
    logging.warning('Process exited with status code %d.' %
                    process.returncode)

  return process.returncode
