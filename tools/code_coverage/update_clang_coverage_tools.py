#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Download clang coverage tools (llvm-cov + llvm-profdata)"""

import os
import sys
import urllib2

sys.path.append(
    os.path.join(
        os.path.dirname(__file__), os.path.pardir, os.path.pardir,
        'third_party'))
import coverage_utils

sys.path.append(
    os.path.join(
        os.path.dirname(__file__), os.path.pardir, os.path.pardir, 'tools',
        'clang', 'scripts'))
import update as clang_update


# TODO(crbug.com/759794): remove this function once tools get included to
# Clang bundle:
# https://chromium-review.googlesource.com/c/chromium/src/+/688221
def DownloadCoverageToolsIfNeeded():
  """Temporary solution to download llvm-profdata and llvm-cov tools."""

  def _GetRevisionFromStampFile(stamp_file_path):
    """Returns revision by reading the build stamp file.

    Args:
      stamp_file_path: A path the build stamp file created by
                       tools/clang/scripts/update.py.
    Returns:
      A string represeting the revision of the tool, such as 361212-67510fac-2.
    """
    if not os.path.exists(stamp_file_path):
      return ''

    with open(stamp_file_path) as stamp_file:
      stamp_file_line = stamp_file.readline()
      if ',' in stamp_file_line:
        package_version = stamp_file_line.rstrip().split(',')[0]
      else:
        package_version = stamp_file_line.rstrip()

      return package_version

  cov_path = os.path.join(clang_update.LLVM_BUILD_DIR, 'bin', 'llvm-cov')
  profdata_path = os.path.join(
      clang_update.LLVM_BUILD_DIR, 'bin', 'llvm-profdata')

  host_platform = coverage_utils.GetHostPlatform()
  clang_revision = _GetRevisionFromStampFile(clang_update.STAMP_FILE)
  coverage_revision_stamp_file = os.path.join(
      os.path.dirname(clang_update.STAMP_FILE), 'cr_coverage_revision')
  coverage_revision = _GetRevisionFromStampFile(coverage_revision_stamp_file)
  has_coverage_tools = (
      os.path.exists(cov_path) and os.path.exists(profdata_path))

  if (has_coverage_tools and clang_revision == coverage_revision):
    # LLVM coverage tools are up to date, bail out.
    return

  package_version = clang_revision
  coverage_tools_file = 'llvm-code-coverage-%s.tgz' % package_version

  # The code below follows the code from tools/clang/scripts/update.py.
  if host_platform == 'mac':
    coverage_tools_url = clang_update.CDS_URL + '/Mac/' + coverage_tools_file
  elif host_platform == 'linux':
    coverage_tools_url = (
        clang_update.CDS_URL + '/Linux_x64/' + coverage_tools_file)
  else:
    assert host_platform == 'win'
    coverage_tools_url = (clang_update.CDS_URL + '/Win/' + coverage_tools_file)

  try:
    clang_update.DownloadAndUnpack(coverage_tools_url,
                                   clang_update.LLVM_BUILD_DIR)
    with open(coverage_revision_stamp_file, 'w') as file_handle:
      file_handle.write('%s,%s' % (package_version, host_platform))
      file_handle.write('\n')
  except urllib2.URLError:
    raise Exception(
        'Failed to download coverage tools: %s.' % coverage_tools_url)


if __name__ == '__main__':
  DownloadCoverageToolsIfNeeded()
  sys.exit(0)
