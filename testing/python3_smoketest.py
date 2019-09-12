#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import sys
import time

# Simple test to make sure that Python 3 can be used on swarming and that
# test_env.py remains compatible with Python 3. Can be removed once another
# test is moved over to Python 3.

results = {
  'tests': {
    'python3_smoketest': {
      'expected': 'PASS',
    },
  },
  'interrupted': False,
  'path_delimiter': '.',
  'version': 3,
  'seconds_since_epoch': time.time(),
  'num_failures_by_type': {},
}

parser = argparse.ArgumentParser()
parser.add_argument('--write-full-results-to', required=True)
args = parser.parse_args()

rc = 0
if sys.version_info[0] == 3:
  results['tests']['python3_smoketest']['actual'] = 'PASS'
  results['num_failures_by_type']['PASS'] = 1
else:
  rc = 1
  results['tests']['python3_smoketest']['actual'] = 'FAIL'
  results['num_failures_by_type']['FAIL'] = 1

with open(args.write_full_results_to, 'w') as f:
  json.dump(results, f)

sys.exit(rc)
