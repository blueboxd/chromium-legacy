#!/usr/bin/env python
# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Library and tool to expand command lines that mention thin archives
# into command lines that mention the contained object files.

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import argparse
import errno
import io
import os
import re
import sys

COMPILER_RE = re.compile('clang')
LINKER_RE = re.compile('l(?:ld|ink)')
LIB_RE = re.compile('.*\\.(?:a|lib)', re.IGNORECASE)
OBJ_RE = re.compile(b'(.*)\\.(o(?:bj)?)', re.IGNORECASE)
THIN_AR_LFN_RE = re.compile('/([0-9]+)')


def ensure_dir(path):
  """
  Creates path as a directory if it does not already exist.
  """
  try:
    os.makedirs(path)
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise


def thin_archive(path):
  """
  Returns True if path refers to a thin archive (ar file), False if not.
  """
  with open(path, 'rb') as f:
    return f.read(8) == b'!<thin>\n'


def write_rsp(path, params):
  """
  Writes params to a newly created response file at path.
  """
  ensure_dir(os.path.basename(path))
  with open(path, 'wb') as f:
    f.write(b'\n'.join(params))


def names_in_archive(path):
  """
  Yields the member names in the archive file at path.
  """
  with open(path, 'rb') as f:
    long_names = None
    f.seek(8, io.SEEK_CUR)
    while True:
      file_id = f.read(16)
      if len(file_id) == 0:
        break
      f.seek(32, io.SEEK_CUR)
      m = THIN_AR_LFN_RE.match(file_id)
      if long_names and m:
        name_pos = long(m.group(1))
        name_end = long_names.find('/\n', name_pos)
        name = long_names[name_pos:name_end]
      else:
        name = file_id
      try:
        size = long(f.read(10))
      except:
        sys.stderr.write('While parsing %r, pos %r\n' % (path, f.tell()))
        raise
      # Two entries are special: '/' and '//'. The former is
      # the symbol table, which we skip. The latter is the long
      # file name table, which we read.
      # Anything else is a filename entry which we yield.
      # Every file record ends with two terminating characters
      # which we skip.
      seek_distance = 2
      if file_id == '/               ':
        # Skip symbol table.
        seek_distance += size + (size & 1)
      elif file_id == '//              ':
        # Read long name table.
        f.seek(2, io.SEEK_CUR)
        long_names = f.read(size)
        seek_distance = size & 1
      else:
        yield name
      f.seek(seek_distance, io.SEEK_CUR)


def expand_args(args, linker_prefix=''):
  """
  Yields the parameters in args, with thin archives replaced by a sequence of
  '-start-lib', the member names, and '-end-lib'. This is used to get a command
  line where members of thin archives are mentioned explicitly.
  """
  for arg in args:
    if len(arg) > 1 and arg[0] == '@':
      for x in expand_rsp(arg[1:], linker_prefix):
        yield x
    elif LIB_RE.match(arg) and os.path.exists(arg) and thin_archive(arg):
      yield(linker_prefix + '-start-lib')
      for name in names_in_archive(arg):
        yield(os.path.dirname(arg) + '/' + name)
      yield(linker_prefix + '-end-lib')
    else:
      yield(arg)


def expand_rsp(rspname, linker_prefix=''):
  """
  Yields the parameters found in the response file at rspname, with thin
  archives replaced by a sequence of '-start-lib', the member names,
  and '-end-lib'. This is used to get a command line where members of
  thin archives are mentioned explicitly.
  """
  with open(rspname) as f:
    for x in expand_args(f.read().split(), linker_prefix):
      yield x


def main(argv):
  ap = argparse.ArgumentParser(
      description=('Expand command lines that mention thin archives into'
                   ' command lines that mention the contained object files.'),
      usage='%(prog)s [options] -- command line')
  ap.add_argument('-o', '--output',
                  help=('Write new command line to named file'
                        ' instead of standard output.'))
  ap.add_argument('-p', '--linker-prefix',
                  help='String to prefix linker flags with.',
                  default='')
  ap.add_argument('cmdline',
                  nargs=argparse.REMAINDER,
                  help='Command line to expand. Should be preceeded by \'--\'.')
  args = ap.parse_args(argv[1:])
  if not args.cmdline:
    ap.print_help(sys.stderr)
    return 1

  cmdline = args.cmdline
  if cmdline[0] == '--':
    cmdline = cmdline[1:]
  linker_prefix = args.linker_prefix

  if args.output:
    output = open(args.output, 'w')
  else:
    output = sys.stdout
  for arg in expand_args(cmdline, linker_prefix=linker_prefix):
    output.write('%s\n' % (arg,))
  if args.output:
    output.close()
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
