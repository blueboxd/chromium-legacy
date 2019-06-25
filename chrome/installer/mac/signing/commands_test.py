# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import shutil
import unittest

from . import commands


class TestCommands(unittest.TestCase):

    def setUp(self):
        self.tempdir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tempdir)

    def test_file_exists(self):
        path = os.path.join(self.tempdir, 'exists.txt')

        self.assertFalse(commands.file_exists(path))

        open(path, 'w').close()
        self.assertTrue(commands.file_exists(path))

        os.unlink(path)
        self.assertFalse(commands.file_exists(path))

    def test_move_file(self):
        orig_path = os.path.join(self.tempdir, 'file.txt')
        new_path = os.path.join(self.tempdir, 'renamed.txt')

        commands.write_file(orig_path, 'moo')

        self.assertTrue(commands.file_exists(orig_path))
        self.assertFalse(commands.file_exists(new_path))

        commands.move_file(orig_path, new_path)

        self.assertFalse(commands.file_exists(orig_path))
        self.assertTrue(commands.file_exists(new_path))

    def test_write_file(self):
        path = os.path.join(self.tempdir, 'file.txt')

        data1 = 'hello world this is a test'
        commands.write_file(path, data1)

        with open(path, 'r') as f:
            self.assertEqual(data1, f.read())

        data2 = 'moo'
        commands.write_file(path, data2)

        with open(path, 'r') as f:
            self.assertEqual(data2, f.read())

    def test_run_command(self):
        path = os.path.join(self.tempdir, 'touch.txt')
        self.assertFalse(commands.file_exists(path))

        commands.run_command(['touch', path])

        self.assertTrue(commands.file_exists(path))

    def test_run_command_output(self):
        output = commands.run_command_output(['echo', 'hello world'])
        self.assertEqual(b'hello world\n', output)
