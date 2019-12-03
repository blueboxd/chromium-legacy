#!/usr/bin/env python

# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Checks the consistency of a11y component extension style.

Checks that conventions followed in the accessibility component extensions
(e.g. braces for all conditional statement blocks) are respected.
'''

import os
import sys
import re
from multiprocessing import pool


def CheckMatch(pattern, path, should_match):
  '''Checks if the path matches the given pattern.
  Args:
    pattern The regex pattern to check.
    path The filepath being checked.
    should_match Whether the file is expected to contain a match or not
  Returns:
    True or false, depending on whether the path matches only when expected.
  '''
  for line in open(path).readlines():
    did_match = re.search(pattern, line)
  return did_match == should_match


_IF_NO_BRACES_PATTERN = '^ *if \(.*[;)] *$'
_ELSE_IF_NO_BRACES_PATTERN = '^[ }]*else if.*[;)] *$'
_ELSE_NO_BRACES_PATTERN = '^[ }]*else[^i{:]*$'


def CheckA11yStyle(changed_files=None):
  if changed_files is None:
    return (True, '')
  changed_files_set = frozenset(
      (os.path.relpath(path) for path in changed_files))
  if len(changed_files_set) == 0:
    return (True, '')

  ret_success = True
  ret_output = ''
  worker_pool = pool.Pool(3 * len(changed_files_set))
  try:
    results = []
    for path in changed_files_set:
      results.append([
          path,
          worker_pool.apply_async(
              CheckMatch, args=[_IF_NO_BRACES_PATTERN, path, False]), 'if'
      ])
      results.append([
          path,
          worker_pool.apply_async(
              CheckMatch, args=[_ELSE_IF_NO_BRACES_PATTERN, path, False]),
          'else if'
      ])
      results.append([
          path,
          worker_pool.apply_async(
              CheckMatch, args=[_ELSE_NO_BRACES_PATTERN, path, False]), 'else'
      ])

    for result in results:
      path = result[0]
      success = result[1].get()
      conditional = result[2]
      if not success:
        ret_output += ('Missing braces for `' + conditional +
                       '` block in file: ' + path + '\n')
        ret_success = False

    worker_pool.close()
  except:
    worker_pool.terminate()
    raise
  finally:
    worker_pool.join()

  return (ret_success, ret_output)


if __name__ == '__main__':
  paths = sys.argv[1:]
  print CheckA11yStyle(paths)
