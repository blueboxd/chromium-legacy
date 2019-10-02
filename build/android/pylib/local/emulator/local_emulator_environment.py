# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from pylib.constants import host_paths
from pylib.local.device import local_device_environment

AVD_DIR_PATH = os.path.join(host_paths.DIR_SOURCE_ROOT, 'tools', 'android',
                            'avd')
with host_paths.SysPath(AVD_DIR_PATH):
  import avd  # pylint: disable=import-error


class LocalEmulatorEnvironment(local_device_environment.LocalDeviceEnvironment):

  def __init__(self, args, output_manager, error_func):
    super(LocalEmulatorEnvironment, self).__init__(args, output_manager,
                                                   error_func)
    self._avd_config = avd.AvdConfig(args.avd_config)
    self._emulator_instance = None

  #override
  def SetUp(self):
    self._avd_config.Install()
    self._emulator_instance = self._avd_config.StartInstance()
    self._device_serials = [self._emulator_instance.serial]
    super(LocalEmulatorEnvironment, self).SetUp()

  #override
  def TearDown(self):
    try:
      super(LocalEmulatorEnvironment, self).TearDown()
    finally:
      if self._emulator_instance:
        self._emulator_instance.Stop()
