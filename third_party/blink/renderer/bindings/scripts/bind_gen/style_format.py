# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import subprocess

import gclient_paths

_clang_format_command_path = None
_gn_command_path = None


def init():
    global _clang_format_command_path
    global _gn_command_path

    # //buildtools/<platform>/clang-format
    command_name = "clang-format{}".format(gclient_paths.GetExeSuffix())
    command_path = os.path.abspath(
        os.path.join(gclient_paths.GetBuildtoolsPlatformBinaryPath(),
                     command_name))
    _clang_format_command_path = command_path

    # //buildtools/<platform>/gn
    command_name = "gn{}".format(gclient_paths.GetExeSuffix())
    command_path = os.path.abspath(
        os.path.join(gclient_paths.GetBuildtoolsPlatformBinaryPath(),
                     command_name))
    _gn_command_path = command_path


def auto_format(contents, filename):
    assert isinstance(filename, str)

    _, ext = os.path.splitext(filename)
    if ext in (".gn", ".gni"):
        return gn_format(contents, filename)

    return clang_format(contents, filename)


def clang_format(contents, filename=None):
    command_line = [_clang_format_command_path]
    if filename is not None:
        command_line.append('-assume-filename={}'.format(filename))

    return _invoke_format_command(command_line, filename, contents)


def gn_format(contents, filename=None):
    command_line = [_gn_command_path, "format", "--stdin"]
    if filename is not None:
        command_line.append('-assume-filename={}'.format(filename))

    return _invoke_format_command(command_line, filename, contents)


def _invoke_format_command(command_line, filename, contents):
    proc = subprocess.Popen(
        command_line, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    stdout_output, stderr_output = proc.communicate(input=contents)
    exit_code = proc.wait()

    return StyleFormatResult(
        stdout_output=stdout_output,
        stderr_output=stderr_output,
        exit_code=exit_code,
        filename=filename)


class StyleFormatResult(object):
    def __init__(self, stdout_output, stderr_output, exit_code, filename):
        self._stdout_output = stdout_output
        self._stderr_output = stderr_output
        self._exit_code = exit_code
        self._filename = filename

    @property
    def did_succeed(self):
        return self._exit_code == 0

    @property
    def contents(self):
        assert self.did_succeed
        return self._stdout_output

    @property
    def error_message(self):
        return self._stderr_output

    @property
    def filename(self):
        return self._filename
