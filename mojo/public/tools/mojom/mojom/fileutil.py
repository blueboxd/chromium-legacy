# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import errno
import imp
import os.path
import sys


def _GetDirAbove(dirname):
  """Returns the directory "above" this file containing |dirname| (which must
  also be "above" this file)."""
  path = os.path.abspath(__file__)
  while True:
    path, tail = os.path.split(path)
    assert tail
    if tail == dirname:
      return path


def EnsureDirectoryExists(path, always_try_to_create=False):
  """A wrapper for os.makedirs that does not error if the directory already
  exists. A different process could be racing to create this directory."""

  if not os.path.exists(path) or always_try_to_create:
    try:
      os.makedirs(path)
    except OSError as e:
      # There may have been a race to create this directory.
      if e.errno != errno.EEXIST:
        raise


def EnsureModuleAvailable(module_name):
  """Helper function which attempts to find the Python module named
  |module_name| using the usual module search. If that fails, this assumes it's
  being called within the Chromium tree, or an equivalent tree where this
  library lives somewhere under a "mojo" directory which has a "third_party"
  sibling."""
  try:
    imp.find_module(module_name)
  except ImportError:
    sys.path.append(os.path.join(_GetDirAbove("mojo"), "third_party"))
