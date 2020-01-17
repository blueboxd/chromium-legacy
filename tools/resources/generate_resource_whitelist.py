#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

__doc__ = """generate_resource_whitelist.py [-o OUTPUT] INPUTS...

INPUTS are paths to unstripped binaries or PDBs containing references to
resources in their debug info.

This script generates a resource whitelist by reading debug info from
INPUTS and writes it to OUTPUT.
"""

# Whitelisted resources are identified by searching the input file for
# instantiations of the special function ui::WhitelistedResource (see
# ui/base/resource/whitelist.h).

import argparse
import os
import subprocess
import sys

llvm_bindir = os.path.join(
    os.path.dirname(sys.argv[0]), '..', '..', 'third_party', 'llvm-build',
    'Release+Asserts', 'bin')


def GetResourceWhitelistELF(path):
  # Produce a resource whitelist by searching for debug info referring to
  # WhitelistedResource. It's sufficient to look for strings in .debug_str
  # rather than trying to parse all of the debug info.
  readelf = subprocess.Popen(['readelf', '-p', '.debug_str', path],
                             stdout=subprocess.PIPE)
  resource_ids = set()
  for line in readelf.stdout:
    # Read a line of the form "  [   123]  WhitelistedResource<456>". We're
    # only interested in the string, not the offset. We're also not interested
    # in header lines.
    split = line.split(']', 1)
    if len(split) < 2:
      continue
    s = split[1][2:]
    if s.startswith('WhitelistedResource<'):
      try:
        resource_ids.add(int(s[len('WhitelistedResource<'):-len('>')-1]))
      except ValueError:
        continue
  exit_code = readelf.wait()
  if exit_code != 0:
    raise Exception('readelf exited with exit code %d' % exit_code)
  return resource_ids


def GetResourceWhitelistPDB(path):
  # Produce a resource whitelist by using llvm-pdbutil to read a PDB file's
  # publics stream, which is essentially a symbol table, and searching for
  # instantiations of WhitelistedResource. Any such instantiations are demangled
  # to extract the resource identifier.
  pdbutil = subprocess.Popen(
      [os.path.join(llvm_bindir, 'llvm-pdbutil'), 'dump', '-publics', path],
      stdout=subprocess.PIPE)
  names = ''
  for line in pdbutil.stdout:
    # Read a line of the form
    # "733352 | S_PUB32 [size = 56] `??$WhitelistedResource@$0BFGM@@ui@@YAXXZ`".
    if '`' not in line:
      continue
    sym_name = line[line.find('`') + 1:line.rfind('`')]
    # Under certain conditions such as the GN arg `use_clang_coverage = true` it
    # is possible for the compiler to emit additional symbols that do not match
    # the standard mangled-name format.
    # Example: __profd_??$WhitelistedResource@$0BGPH@@ui@@YAXXZ
    # C++ mangled names are supposed to begin with `?`, so check for that.
    if 'WhitelistedResource' in sym_name and sym_name.startswith('?'):
      names += sym_name + '\n'
  exit_code = pdbutil.wait()
  if exit_code != 0:
    raise Exception('llvm-pdbutil exited with exit code %d' % exit_code)

  undname = subprocess.Popen([os.path.join(llvm_bindir, 'llvm-undname')],
                             stdin=subprocess.PIPE,
                             stdout=subprocess.PIPE)
  stdout, _ = undname.communicate(names)
  resource_ids = set()
  for line in stdout.split('\n'):
    # Read a line of the form
    # "void __cdecl ui::WhitelistedResource<5484>(void)".
    prefix = ' ui::WhitelistedResource<'
    pos = line.find(prefix)
    if pos == -1:
      continue
    try:
      resource_ids.add(int(line[pos + len(prefix):line.rfind('>')]))
    except ValueError:
      continue
  exit_code = undname.wait()
  if exit_code != 0:
    raise Exception('llvm-undname exited with exit code %d' % exit_code)
  return resource_ids


def WriteResourceWhitelist(args):
  resource_ids = set()
  for input in args.inputs:
    with open(input, 'r') as f:
      magic = f.read(4)
    if magic == '\x7fELF':
      resource_ids = resource_ids.union(GetResourceWhitelistELF(input))
    elif magic == 'Micr':
      resource_ids = resource_ids.union(GetResourceWhitelistPDB(input))
    else:
      raise Exception('unknown file format')
  if len(resource_ids) == 0:
    raise Exception('No debug info was dumped. Ensure GN arg "symbol_level" '
                    '!= 0 and that the file is not stripped.')
  for id in sorted(resource_ids):
    args.output.write(str(id) + '\n')


def main():
  parser = argparse.ArgumentParser(usage=__doc__)
  parser.add_argument('inputs', nargs='+', help='An unstripped binary or PDB.')
  parser.add_argument(
      '-o', dest='output', type=argparse.FileType('w'), default=sys.stdout,
      help='The resource list path to write (default stdout)')

  args = parser.parse_args()
  WriteResourceWhitelist(args)

if __name__ == '__main__':
  main()
