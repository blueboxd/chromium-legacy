# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The commands module wraps operations that have side-effects.
"""

import os
import plistlib
import shutil
import subprocess
import tempfile


def file_exists(path):
    return os.path.exists(path)


def copy_files(source, dest):
    assert source[-1] != '/'
    subprocess.check_call(['rsync', '-a', '--delete', source, dest])


def move_file(source, dest):
    shutil.move(source, dest)


def make_dir(at):
    os.mkdir(at)


def write_file(path, contents):
    with open(path, 'w') as f:
        f.write(contents)


def run_command(args, **kwargs):
    print('Running command: {}'.format(args))
    subprocess.check_call(args, **kwargs)


def run_command_output(args, **kwargs):
    print('Running command: {}'.format(args))
    return subprocess.check_output(args, **kwargs)


class PlistContext(object):
    """
    PlistContext is a context manager that reads a plist on entry, providing
    the contents as a dictionary. If |rewrite| is True, then the same dictionary
    is re-serialized on exit.
    """

    def __init__(self, plist_path, rewrite=False):
        self._path = plist_path
        self._rewrite = rewrite

    def __enter__(self):
        self._plist = plistlib.readPlist(self._path)
        return self._plist

    def __exit__(self, exc_type, exc_value, exc_tb):
        if self._rewrite and not exc_type:
            plistlib.writePlist(self._plist, self._path)
        self._plist = None


class WorkDirectory(object):
    """
    WorkDirectory creates a temporary directory on entry, storing the path as
    the |model.Paths.work| path. On exit, the directory is destroyed.
    """

    def __init__(self, paths):
        self._workdir = tempfile.mkdtemp(prefix='chromesign_')
        self._paths = paths

    def __enter__(self):
        return self._paths.replace_work(self._workdir)

    def __exit__(self, exc_type, value, traceback):
        shutil.rmtree(self._workdir)
