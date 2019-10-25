#!/usr/bin/env python
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Allots libraries to modules to be packaged into.

All libraries that are depended on by a single module will be allotted to this
module. All other libraries will be allotted to base.
"""

import argparse
import collections
import json
import sys

from util import build_utils


def _ModuleLibrariesPair(arg):
  pos = arg.find(',')
  assert pos > 0
  return (arg[:pos], arg[pos + 1:])


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--libraries',
      action='append',
      type=_ModuleLibrariesPair,
      required=True,
      help='A pair of module name and GN list of libraries a module depends '
      'on. Can be specified multiple times. Must be set at least once for '
      'base.')
  parser.add_argument(
      '--output',
      required=True,
      help='A JSON file with a key for each module mapping to a list of '
      'libraries, which should be packaged into this module.')
  options = parser.parse_args(build_utils.ExpandFileArgs(args))
  options.libraries = [(m, build_utils.ParseGnList(l))
                       for m, l in options.libraries]

  # Parse input to map and count how many modules depend on each library.
  libraries_map = {}
  libraries_counter = collections.Counter()  # Modules count for each library.
  for module, libraries in options.libraries:
    if module not in libraries_map:
      libraries_map[module] = set()
    libraries_map[module].update(libraries)
    libraries_counter.update(libraries)

  assert 'base' in libraries_map

  # Allot libraries depended on by multiple modules to base.
  multidep_libraries = {l for l, c in libraries_counter.items() if c > 1}
  libraries_map = {m: l - multidep_libraries for m, l in libraries_map.items()}
  libraries_map['base'] |= multidep_libraries

  with open(options.output, 'w') as f:
    # Write native libraries config and ensure the output is deterministic.
    json.dump({m: sorted(l)
               for m, l in libraries_map.items()},
              f,
              sort_keys=True,
              indent=2)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
