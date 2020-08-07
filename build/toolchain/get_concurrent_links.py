#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script computs the number of concurrent links we want to run in the build
# as a function of machine spec. It's based on GetDefaultConcurrentLinks in GYP.

from __future__ import print_function

import argparse
import multiprocessing
import os
import re
import subprocess
import sys

sys.path.insert(1, os.path.join(os.path.dirname(__file__), '..'))
import gn_helpers


def _GetTotalMemoryInBytes():
  if sys.platform in ('win32', 'cygwin'):
    import ctypes

    class MEMORYSTATUSEX(ctypes.Structure):
      _fields_ = [
          ("dwLength", ctypes.c_ulong),
          ("dwMemoryLoad", ctypes.c_ulong),
          ("ullTotalPhys", ctypes.c_ulonglong),
          ("ullAvailPhys", ctypes.c_ulonglong),
          ("ullTotalPageFile", ctypes.c_ulonglong),
          ("ullAvailPageFile", ctypes.c_ulonglong),
          ("ullTotalVirtual", ctypes.c_ulonglong),
          ("ullAvailVirtual", ctypes.c_ulonglong),
          ("sullAvailExtendedVirtual", ctypes.c_ulonglong),
      ]

    stat = MEMORYSTATUSEX(dwLength=ctypes.sizeof(MEMORYSTATUSEX))
    ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(stat))
    return stat.ullTotalPhys
  elif sys.platform.startswith('linux'):
    if os.path.exists("/proc/meminfo"):
      with open("/proc/meminfo") as meminfo:
        memtotal_re = re.compile(r'^MemTotal:\s*(\d*)\s*kB')
        for line in meminfo:
          match = memtotal_re.match(line)
          if not match:
            continue
          return float(match.group(1)) * 2**10
  elif sys.platform == 'darwin':
    try:
      return int(subprocess.check_output(['sysctl', '-n', 'hw.memsize']))
    except Exception:
      return 0
  # TODO(scottmg): Implement this for other platforms.
  return 0


def _GetDefaultConcurrentLinks(mem_per_link_gb, reserve_mem_gb):
  explanation = []
  explanation.append('mem_per_link_gb={} reserve_mem_gb={}'.format(
      mem_per_link_gb, reserve_mem_gb))
  # Inherit the legacy environment variable for people that have set it in GYP.
  num_links = int(os.getenv('GYP_LINK_CONCURRENCY', 0))
  if num_links:
    reason = 'GYP_LINK_CONCURRENCY'
  else:
    mem_total_bytes = _GetTotalMemoryInBytes()
    mem_total_bytes = max(0, mem_total_bytes - reserve_mem_gb * 2**30)
    mem_cap = int(max(1, mem_total_bytes / mem_per_link_gb / 2**30))
    hard_cap = max(1, int(os.getenv('GYP_LINK_CONCURRENCY_MAX', 2**32)))

    try:
      cpu_cap = multiprocessing.cpu_count()
    except:
      cpu_cap = 1

    explanation.append('cpu_count={} mem_total_bytes={:.1f}GiB'.format(
        cpu_cap, mem_total_bytes / 2**30))

    num_links = min(mem_cap, hard_cap, cpu_cap)
    if num_links == cpu_cap:
      reason = 'cpu_count'
    elif num_links == hard_cap:
      reason = 'GYP_LINK_CONCURRENCY_MAX'
    else:
      reason = 'RAM'

  explanation.append('concurrent_links={}  (reason: {})'.format(
      num_links, reason))
  return num_links, explanation


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--mem_per_link_gb', type=int, default=8)
  parser.add_argument('--reserve_mem_gb', type=int, default=0)
  options = parser.parse_args()

  num_links, explanation = _GetDefaultConcurrentLinks(options.mem_per_link_gb,
                                                      options.reserve_mem_gb)
  sys.stdout.write(
      gn_helpers.ToGNString({
          'concurrent_links': num_links,
          'explanation': explanation,
      }))
  return 0


if __name__ == '__main__':
  sys.exit(main())
