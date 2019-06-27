#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Clobbers all builder caches for a specific builder.

Note that this currently does not support windows.
"""

from __future__ import print_function

import argparse
import hashlib
import os
import subprocess
import sys

_SRC_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
_SWARMING_CLIENT = os.path.join(_SRC_ROOT, 'tools', 'swarming_client',
                                'swarming.py')
_SWARMING_SERVER = 'chromium-swarm.appspot.com'


def _get_bots(pool, cache):
  cmd = [
      sys.executable,
      _SWARMING_CLIENT,
      'bots',
      '-b',
      '-S',
      _SWARMING_SERVER,
      '-d',
      'caches',
      cache,
      '-d',
      'pool',
      pool,
  ]
  return subprocess.check_output(cmd).splitlines()


def _trigger_clobber(pool, cache, bot, dry_run):
  cmd = [
      sys.executable,
      _SWARMING_CLIENT,
      'trigger',
      '-S',
      _SWARMING_SERVER,
      '-d',
      'pool',
      pool,
      '-d',
      'id',
      bot,
      '--named-cache',
      cache,
      'cache/builder',
      '--priority=10',
      '--raw-cmd',
      '--',
      # TODO(jbudorick): Generalize this for windows.
      '/bin/rm',
      '-rf',
      'cache/builder',
  ]
  if dry_run:
    print('Would run `%s`' % ' '.join(cmd))
  else:
    subprocess.check_call(cmd)


def main(raw_args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--builder', required=True)
  parser.add_argument('--pool', required=True)
  parser.add_argument(
      '--project', default='chromium', choices=['chromium', 'infra'])
  parser.add_argument('-n', '--dry-run', action='store_true')
  args = parser.parse_args(raw_args)

  # Matches http://bit.ly/2WZO33P
  h = hashlib.sha256('%s/%s/%s' % (args.project, args.pool, args.builder))
  cache_name = 'builder_%s_v2' % (h.hexdigest())
  swarming_pool = 'luci.%s.%s' % (args.project, args.pool)

  bots = _get_bots(swarming_pool, cache_name)

  print('The following bots will be clobbered:')
  print()
  for bot in bots:
    print('  %s' % bot)
  print()
  val = raw_input('Proceed? [Y/n] ')
  if val and not val[0] in ('Y', 'y'):
    print('Cancelled.')
    return 1

  for bot in bots:
    _trigger_clobber(swarming_pool, cache_name, bot, args.dry_run)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
