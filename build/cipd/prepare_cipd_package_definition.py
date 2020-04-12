#!/usr/bin/env python
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Prepares a directory and a corresponding package definition which can be
used to create a CIPD package."""

import argparse
import errno
import os
import sys
import yaml


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--pkg-name',
                      type=str,
                      required=True,
                      help='Name of the CIPD package.')
  parser.add_argument('--description',
                      type=str,
                      required=True,
                      help='Description of the CIPD package.')
  parser.add_argument('--pkg-root',
                      type=str,
                      required=True,
                      help='Path to the package root.')
  parser.add_argument('--install-mode',
                      type=str,
                      choices=['copy', 'symlink'],
                      required=True,
                      help='CIPD install mode.')
  parser.add_argument('--pkg-def',
                      type=str,
                      required=True,
                      help='Path to the output package definition.')
  parser.add_argument('--depfile',
                      type=str,
                      required=True,
                      help='Path to the depfile.')
  parser.add_argument('--copy-files',
                      nargs='+',
                      help='Files to be copied into --pkg-root and included '
                      'in the package definition.')
  args = parser.parse_args(args)

  pkg_def = {
      'package': args.pkg_name,
      'description': args.description,
      'root': '${outdir}/%s' % os.path.join(args.pkg_root),
      'install_mode': args.install_mode,
      'data': [],
  }

  deps = set()
  # Copy files into the root.
  for filepath in args.copy_files:
    basename = os.path.basename(filepath)
    dest = os.path.join(args.pkg_root, basename)
    try:
      os.link(filepath, dest)
    except OSError as e:
      if e.errno != errno.EEXIST:
        raise
    pkg_def['data'].append({'file': basename})
    deps.add(dest)

  with open(args.pkg_def, 'w') as f:
    yaml.dump(pkg_def, f)
  with open(args.depfile, 'w') as f:
    f.writelines('%s: %s\n' % (args.pkg_def, ' '.join(sorted(deps))))

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
