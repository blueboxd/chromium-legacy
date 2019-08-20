#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Emits a formatted, optionally filtered view of the list of flags.
"""

import argparse
import os
import sys

ROOT_PATH = os.path.join(os.path.dirname(__file__), '..', '..')
PYJSON5_PATH = os.path.join(ROOT_PATH, 'third_party', 'pyjson5', 'src')

sys.path.append(PYJSON5_PATH)

import json5


def load_metadata():
  flags_path = os.path.join(ROOT_PATH, 'chrome', 'browser',
                            'flag-metadata.json')
  return json5.load(open(flags_path))


def keep_expired_by(flags, mstone):
  """Filter flags to contain only flags that expire by mstone.

  Only flags that either never expire or have an expiration milestone <= mstone
  are in the returned list.

  >>> keep_expired_by([{'expiry_milestone': 3}], 2)
  []
  >>> keep_expired_by([{'expiry_milestone': 3}], 3)
  [{'expiry_milestone': 3}]
  >>> keep_expired_by([{'expiry_milestone': -1}], 3)
  []
  """
  return [f for f in flags if -1 != f['expiry_milestone'] <= mstone]


def keep_never_expires(flags):
  """Filter flags to contain only flags that never expire.

  >>> keep_never_expires([{'expiry_milestone': -1}, {'expiry_milestone': 2}])
  [{'expiry_milestone': -1}]
  """
  return [f for f in flags if f['expiry_milestone'] == -1]


def print_flags(flags, verbose):
  """Prints the supplied list of flags.

  In verbose mode, prints name, expiry, and owner list; in non-verbose mode,
  prints just the name.

  >>> f1 = {'name': 'foo', 'expiry_milestone': 73, 'owners': ['bar', 'baz']}
  >>> f2 = {'name': 'bar', 'expiry_milestone': 74, 'owners': ['//quxx/OWNERS']}
  >>> print_flags([f1], False)
  foo
  >>> print_flags([f1], True)
  foo expires 73 owners bar, baz
  >>> print_flags([f2], False)
  bar
  >>> print_flags([f2], True)
  bar expires 74 owners //quxx/OWNERS
  """
  for f in flags:
    if verbose:
      print '%s expires %d owners %s' % (f['name'], f['expiry_milestone'],
        ', '.join(f['owners']))
    else:
      print f['name']


def main():
  import doctest
  doctest.testmod()

  parser = argparse.ArgumentParser(description=__doc__)
  group = parser.add_mutually_exclusive_group()
  group.add_argument('-n', '--never-expires', action='store_true')
  group.add_argument('-e', '--expired-by', type=int)
  parser.add_argument('-v', '--verbose', action='store_true')
  args = parser.parse_args()

  flags = load_metadata()
  if args.expired_by:
    flags = keep_expired_by(flags, args.expired_by)
  if args.never_expires:
    flags = keep_never_expires(flags)
  print_flags(flags, args.verbose)


if __name__ == '__main__':
  main()
