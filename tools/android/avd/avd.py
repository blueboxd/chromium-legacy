# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import logging
import os
import socket
import stat
import sys
import threading

from google.protobuf import text_format
from proto import avd_pb2

_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(
    os.path.join(_SRC_ROOT, 'third_party', 'catapult', 'common', 'py_utils'))
from py_utils import tempfile_ext

sys.path.append(
    os.path.join(_SRC_ROOT, 'third_party', 'catapult', 'devil'))
from devil.android.sdk import adb_wrapper
from devil.utils import cmd_helper
from devil.utils import timeout_retry


class AvdException(Exception):
  """Raised when this module has a problem interacting with an AVD."""

  def __init__(self, summary, command=None, stdout=None, stderr=None):
    message_parts = [summary]
    if command:
      message_parts.append('  command: %s' % ' '.join(command))
    if stdout:
      message_parts.append('  stdout:')
      message_parts.extend(
          '    %s' % line for line in stdout.splitlines())
    if stderr:
      message_parts.append('  stderr:')
      message_parts.extend(
          '    %s' % line for line in stderr.splitlines())

    super(AvdException, self).__init__('\n'.join(message_parts))


def _Load(avd_proto_path):
  """Loads an Avd proto from a textpb file at the given path.

  Should not be called outside of this module.

  Args:
    avd_proto_path: path to a textpb file containing an Avd message.
  """
  with open(avd_proto_path) as avd_proto_file:
    return text_format.Merge(avd_proto_file.read(), avd_pb2.Avd())


class AvdConfig(object):
  """Represents a particular AVD configuration.

  This class supports creation, installation, and execution of an AVD
  from a given Avd proto message, as defined in
  //tools/android/avd/proto/avd.proto.
  """

  def __init__(self, avd_proto_path):
    """Create an AvdConfig object.

    Args:
      avd_proto_path: path to a textpb file containing an Avd message.
    """
    self._config = _Load(avd_proto_path)

    self._emulator_home = os.path.join(
        _SRC_ROOT, self._config.avd_package.dest_path)
    self._emulator_sdk_root = os.path.join(
        _SRC_ROOT, self._config.emulator_package.dest_path)
    self._emulator_path = os.path.join(
        self._emulator_sdk_root, 'emulator', 'emulator')

    self._initialized = False
    self._initializer_lock = threading.Lock()

  def Install(self):
    """Installs the requested CIPD packages.

    Returns: None
    Raises: AvdException on failure to install.
    """
    pkgs_by_dir = {}
    for pkg in (self._config.emulator_package,
                self._config.system_image_package,
                self._config.avd_package):
      if not pkg.dest_path in pkgs_by_dir:
        pkgs_by_dir[pkg.dest_path] = []
      pkgs_by_dir[pkg.dest_path].append(pkg)

    for pkg_dir, pkgs in pkgs_by_dir.iteritems():
      logging.info('Installing packages in %s', pkg_dir)
      cipd_root = os.path.join(_SRC_ROOT, pkg_dir)
      if not os.path.exists(cipd_root):
        os.makedirs(cipd_root)
      ensure_path = os.path.join(cipd_root, '.ensure')
      with open(ensure_path, 'w') as ensure_file:
        # Make CIPD ensure that all files are present, even if
        # it thinks the package is installed.
        ensure_file.write('$ParanoidMode CheckPresence\n\n')
        for pkg in pkgs:
          ensure_file.write('%s %s\n' % (pkg.package_name, pkg.version))
          logging.info('  %s %s', pkg.package_name, pkg.version)
      ensure_cmd = [
          'cipd', 'ensure', '-ensure-file', ensure_path, '-root', cipd_root,
      ]
      status, output, error = cmd_helper.GetCmdStatusOutputAndError(ensure_cmd)
      if status:
        raise AvdException(
            'Failed to install CIPD package %s' % pkg.package_name,
            command=ensure_cmd,
            stdout=output,
            stderr=error)

    # The emulator requires that some files are writable.
    for dirname, _, filenames in os.walk(self._emulator_home):
      for f in filenames:
        path = os.path.join(dirname, f)
        if (os.lstat(path).st_mode &
            (stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO) == stat.S_IRUSR):
          os.chmod(path, stat.S_IRUSR | stat.S_IWUSR)

  def _Initialize(self):
    if self._initialized:
      return

    with self._initializer_lock:
      if self._initialized:
        return

      # Emulator start-up looks for the adb daemon. Make sure it's running.
      adb_wrapper.AdbWrapper.StartServer()

      # Emulator start-up tries to check for the SDK root by looking for
      # platforms/ and platform-tools/. Ensure they exist.
      # See http://bit.ly/2YAkyFE for context.
      required_dirs = [
          os.path.join(self._emulator_sdk_root, 'platforms'),
          os.path.join(self._emulator_sdk_root, 'platform-tools'),
      ]
      for d in required_dirs:
        if not os.path.exists(d):
          os.makedirs(d)

  def StartInstance(self):
    """Starts an AVD instance.

    Returns:
      An _AvdInstance.
    """
    self._Initialize()
    instance = _AvdInstance(
        self._emulator_path, self._config.avd_name, self._emulator_home)
    instance.Start()
    return instance


class _AvdInstance(object):
  """Represents a single running instance of an AVD.

  This class should only be created directly by AvdConfig.StartInstance,
  but its other methods can be freely called.
  """

  def __init__(self, emulator_path, avd_name, emulator_home):
    """Create an _AvdInstance object.

    Args:
      emulator_path: path to the emulator binary.
      avd_name: name of the AVD to run.
      emulator_home: path to the emulator home directory.
    """
    self._avd_name = avd_name
    self._emulator_home = emulator_home
    self._emulator_path = emulator_path
    self._emulator_proc = None
    self._emulator_serial = None
    self._sink = None

  def Start(self):
    """Starts the emulator running an instance of the given AVD."""
    with tempfile_ext.TemporaryFileName() as socket_path, (contextlib.closing(
        socket.socket(socket.AF_UNIX))) as sock:
      sock.bind(socket_path)
      emulator_cmd = [
          self._emulator_path,
          '-avd',
          self._avd_name,
          '-report-console',
          'unix:%s' % socket_path,
          '-read-only',
          '-no-window'
      ]
      emulator_env = {}
      if self._emulator_home:
        emulator_env['ANDROID_EMULATOR_HOME'] = self._emulator_home
      sock.listen(1)

      logging.info('Starting emulator.')

      # TODO(jbudorick): Add support for logging emulator stdout & stderr at
      # higher logging levels.
      self._sink = open('/dev/null', 'w')
      self._emulator_proc = cmd_helper.Popen(
          emulator_cmd,
          stdout=self._sink,
          stderr=self._sink,
          env=emulator_env)

      # Waits for the emulator to report its serial as requested via
      # -report-console. See http://bit.ly/2lK3L18 for more.
      def listen_for_serial(s):
        logging.info('Waiting for connection from emulator.')
        with contextlib.closing(s.accept()[0]) as conn:
          val = conn.recv(1024)
          return 'emulator-%d' % int(val)

      try:
        self._emulator_serial = timeout_retry.Run(
            listen_for_serial, timeout=30, retries=0, args=[sock])
        logging.info('%s started', self._emulator_serial)
      except Exception:
        self.Stop()
        raise

  def Stop(self):
    """Stops the emulator process."""
    if self._emulator_proc:
      if self._emulator_proc.poll() is None:
        self._emulator_proc.terminate()
        self._emulator_proc.wait()
      self._emulator_proc = None
    if self._sink:
      self._sink.close()
      self._sink = None

  @property
  def serial(self):
    return self._emulator_serial
