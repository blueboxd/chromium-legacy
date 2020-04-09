# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import posixpath
import stat

import py_utils
from py_utils import cloud_storage
from py_utils import tempfile_ext

CS_BUCKET = cloud_storage.PUBLIC_BUCKET
CS_FOLDER = 'perfetto_binaries'
LATEST_FILENAME = 'latest'
LOCAL_STORAGE_FOLDER = os.path.join(os.path.dirname(__file__), 'bin')
CONFIG_PATH = os.path.join(os.path.dirname(__file__), 'binary_deps.json')


def _GetHostPlatform():
  os_name = py_utils.GetHostOsName()
  # If we're running directly on a Chrome OS device, fetch the binaries for
  # linux instead, which should be compatible with CrOS.
  if os_name == 'chromeos':
    os_name = 'linux'
  return os_name


def _CalculateHash(remote_path):
  with tempfile_ext.NamedTemporaryFile() as f:
    f.close()
    cloud_storage.Get(CS_BUCKET, remote_path, f.name)
    return cloud_storage.CalculateHash(f.name)


def _SetLatestPathForBinary(binary_name, platform, latest_path):
  with tempfile_ext.NamedTemporaryFile() as latest_file:
    latest_file.write(latest_path)
    latest_file.close()
    remote_latest_file = posixpath.join(CS_FOLDER, binary_name, platform,
                                        LATEST_FILENAME)
    cloud_storage.Insert(
        CS_BUCKET, remote_latest_file, latest_file.name, publicly_readable=True)


def UploadHostBinary(binary_name, binary_path, version):
  """Upload the binary to the cloud.

  This function uploads the host binary (e.g. trace_processor_shell) to the
  cloud and updates the 'latest' file for the host platform to point to the
  newly uploaded file. Note that it doesn't modify the config and so doesn't
  affect which binaries will be downloaded by FetchHostBinary.
  """
  filename = os.path.basename(binary_path)
  platform = _GetHostPlatform()
  remote_path = posixpath.join(CS_FOLDER, binary_name, platform, version,
                               filename)
  if not cloud_storage.Exists(CS_BUCKET, remote_path):
    cloud_storage.Insert(
        CS_BUCKET, remote_path, binary_path, publicly_readable=True)
  _SetLatestPathForBinary(binary_name, platform, remote_path)


def GetLatestPath(binary_name, platform):
  with tempfile_ext.NamedTemporaryFile() as latest_file:
    latest_file.close()
    remote_path = posixpath.join(CS_FOLDER, binary_name, platform,
                                 LATEST_FILENAME)
    cloud_storage.Get(CS_BUCKET, remote_path, latest_file.name)
    with open(latest_file.name) as latest:
      return latest.read()


def GetCurrentPath(binary_name, platform):
  with open(CONFIG_PATH) as f:
    config = json.load(f)
  return config[binary_name][platform]['remote_path']


def SwitchBinaryToNewPath(binary_name, platform, new_path):
  """Switch the binary version in use to the latest one.

  This function updates the config file to contain the path to the latest
  available binary version. This will make FetchHostBinary download the latest
  file.
  """
  with open(CONFIG_PATH) as f:
    config = json.load(f)
  config[binary_name][platform]['remote_path'] = new_path
  config[binary_name][platform]['hash'] = _CalculateHash(new_path)
  with open(CONFIG_PATH, 'w') as f:
    json.dump(config, f, indent=4, separators=(',', ': '))


def FetchHostBinary(binary_name):
  """Download the binary from the cloud.

  This function fetches the binary for the host platform from the cloud.
  The cloud path is read from the config.
  """
  with open(CONFIG_PATH) as f:
    config = json.load(f)
  platform = _GetHostPlatform()
  remote_path = config[binary_name][platform]['remote_path']
  expected_hash = config[binary_name][platform]['hash']
  filename = posixpath.basename(remote_path)
  local_path = os.path.join(LOCAL_STORAGE_FOLDER, filename)
  cloud_storage.Get(CS_BUCKET, remote_path, local_path)
  if cloud_storage.CalculateHash(local_path) != expected_hash:
    raise RuntimeError('The downloaded binary has wrong hash.')
  mode = os.stat(local_path).st_mode
  os.chmod(local_path, mode | stat.S_IXUSR)
  return local_path
