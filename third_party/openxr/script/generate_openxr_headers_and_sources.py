# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates necessary files for building and using openxr loader
# This script should be run when we update DEPS file openxr entry or any other
# time that we get a new snapshot of the OpenXR loader library.

# Running this script requires python3, (You can download them from
# https://www.python.org/downloads/) because OpenXR loader project uses
# python3 exclusive libraries to generate some source files.

# Using this script:
# 1. run 'gclient sync' so it will pull openxrsdk from the source.
# 2. cd into third_party/openxr/script/ and run 'python
# generate_openxr_header_and_sources.py'.

import sys, os, subprocess, shutil
from distutils import dir_util

def cleanup(error_message=""):
  print(error_message);
  if os.path.exists("../tmp"):
    shutil.rmtree("../tmp")
  sys.exit()


def genxr(target):
  try:
    subprocess.check_call("python3 ../tmp/genxr.py -registry ../tmp/xr.xml -o "
      "../include/openxr " + target, shell=True)
  except exc:
    raise exc


def src_genxr(target):
  try:
    subprocess.check_call("python3 ../tmp/src_genxr.py -registry "
      "../tmp/xr.xml -o ../gen/ " + target, shell=True)
  except exc:
    raise exc


def copy_necessary_files_to_tmp():
  if not os.path.exists("../tmp"):
    os.makedirs("../tmp")

  src_paths = ["../src/specification/scripts", "../src/src/scripts/"]
  for src_path in src_paths:
    for name in os.listdir(src_path):
      path = os.path.join(src_path, name)
      if os.path.isdir(path):
        # skip directories
        continue
      shutil.copy(path, "../tmp")

  shutil.copy("../src/specification/registry/xr.xml", "../tmp")


def generate_openxr_headers():

  if not os.path.exists("../include/openxr"):
      os.makedirs("../include/openxr")

  shutil.copy("../src/include/openxr/openxr_platform_defines.h",
    "../include/openxr")
  shutil.copy("../src/src/common/loader_interfaces.h", "../include/openxr")

  genxr_targets = [ "openxr_platform.h", "openxr.h"]

  for target in genxr_targets:
    genxr(target)


def generate_openxr_loader_sources():
  src_genxr_targets = [
    "xr_generated_loader.hpp",
    "xr_generated_loader.cpp",
    "xr_generated_dispatch_table.h",
    "xr_generated_dispatch_table.c",
    "xr_generated_utilities.h",
    "xr_generated_utilities.c",
  ]

  if not os.path.exists("../gen"):
    os.makedirs("../gen")

  for target in src_genxr_targets:
    src_genxr(target)


if __name__ == '__main__':

    try:
      copy_necessary_files_to_tmp()
      generate_openxr_headers()
      generate_openxr_loader_sources()
      subprocess.check_call("git cl format ..", shell=True)
      cleanup("All Done")

    except subprocess.CalledProcessError,exc:
      cleanup (exc.cmd + str(exc.output))
