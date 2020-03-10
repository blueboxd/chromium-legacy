#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import re
import sys


def CheckCppTypemapConfig(target_name, config_filename, out_filename):
  _SUPPORTED_KEYS = set([
      'mojom', 'cpp', 'public_headers', 'copyable_pass_by_value',
      'force_serialize', 'hashable', 'move_only', 'nullable_is_same_type'
  ])
  with open(config_filename, 'r') as f:
    config = json.load(f)
    for entry in config['types']:
      for key in entry.iterkeys():
        if key not in _SUPPORTED_KEYS:
          raise IOError('Invalid type property "%s" when processing %s' %
                        (key, target_name))

  with open(out_filename, 'w') as f:
    f.truncate(0)
    json.dump(config, f)


def main():
  parser = argparse.ArgumentParser()
  _, args = parser.parse_known_args()
  if len(args) != 3:
    print('Usage: check_typemap_config.py target_name config_filename '
          'validated_config_filename')
    sys.exit(1)

  CheckCppTypemapConfig(args[0], args[1], args[2])


if __name__ == '__main__':
  main()
