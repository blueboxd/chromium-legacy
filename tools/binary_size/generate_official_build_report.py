#!/usr/bin/python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for generating Supersize HTML Reports for official builds."""

import argparse
import json
import logging
import os
import re
import subprocess
import tempfile

_REPORTS_BASE_URL = 'gs://chrome-supersize/official_builds'
_REPORTS_JSON_GS_URL = os.path.join(_REPORTS_BASE_URL, 'canary_reports.json')
_REPORTS_GS_URL = os.path.join(_REPORTS_BASE_URL, 'reports')


def _WriteReportsJson(out):
  output = subprocess.check_output(['gsutil.py', 'ls', '-R', _REPORTS_GS_URL])

  reports = []
  report_re = re.compile(
      re.escape(_REPORTS_GS_URL) +
      r'/(?P<version>.+?)/(?P<apk>.+?)/(?P<cpu>.+?)\.size')
  for line in output.splitlines():
    m = report_re.search(line)
    if m:
      reports.append({
          'cpu': m.group('cpu'),
          'version': m.group('version'),
          'apk': m.group('apk'),
      })

  json.dump({'pushed': reports}, out)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--version',
      required=True,
      help='Official build version to generate report for (ex. "72.0.3626.7").')
  parser.add_argument(
      '--size-path',
      required=True,
      help='Path to .size file for the given version.')
  parser.add_argument('--gs-size-url', help='Unused')
  parser.add_argument('--gs-size-path', help='Unused')
  parser.add_argument(
      '--arch', required=True, help='Compiler architecture of build.')
  parser.add_argument(
      '--platform',
      required=True,
      help='OS corresponding to those used by omahaproxy.',
      choices=['android', 'webview'])

  args = parser.parse_args()

  report_basename = os.path.splitext(os.path.basename(args.size_path))[0]
  # Maintain name through transition to bundles.
  report_basename = report_basename.replace('.minimal.apks', '.apk')
  dst_url = os.path.join(_REPORTS_GS_URL, args.version, args.arch,
                         report_basename + '.size')

  cmd = ['gsutil.py', 'cp', '-a', 'public-read', args.size_path, dst_url]
  logging.warning(' '.join(cmd))
  subprocess.check_call(cmd)

  with tempfile.NamedTemporaryFile() as f:
    _WriteReportsJson(f)
    cmd = [
        'gsutil.py', '--', '-h', 'Cache-Control:no-cache', 'cp', '-a',
        'public-read', f.name, _REPORTS_JSON_GS_URL
    ]
    logging.warning(' '.join(cmd))
    subprocess.check_call(cmd)


if __name__ == '__main__':
  main()
