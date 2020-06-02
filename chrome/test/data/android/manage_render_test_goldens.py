#!/usr/bin/env python
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Simple script to automatically download all current golden images for Android
# render tests or upload any newly generated ones.

import argparse
import os
from upload_download_utils import download
from upload_download_utils import upload

STORAGE_BUCKET = 'chromium-android-render-test-goldens'
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..', '..'))
GOLDEN_DIRECTORIES = [
  os.path.join(THIS_DIR, 'render_tests'),
  os.path.join(
      CHROMIUM_SRC, 'components', 'test', 'data', 'js_dialogs', 'render_tests'),
  os.path.join(
      CHROMIUM_SRC, 'components', 'test', 'data', 'payments', 'render_tests'),
  os.path.join(
      CHROMIUM_SRC, 'components', 'test', 'data', 'permission_dialogs',
      'render_tests'),
  os.path.join(
      CHROMIUM_SRC, 'components', 'test', 'data', 'vr_browser_ui',
      'render_tests'),
  os.path.join(
      CHROMIUM_SRC, 'components', 'test', 'data', 'vr_browser_video',
      'render_tests'),
]

# This is to prevent accidentally uploading random, non-golden images that
# might be in the directory.
ALLOWED_DEVICE_SDK_COMBINATIONS = [
  # From RenderTestRule.java
  'Nexus_5-19',
  'Nexus_5X-23',
  # For VR tests.
  'Pixel_XL-25',
  'Pixel_XL-26',
]


def _is_file_of_interest(f):
  """Filter through png files with right device sdk combo in the names."""
  if not f.endswith('.png'):
    return False
  for combo in ALLOWED_DEVICE_SDK_COMBINATIONS:
    if combo in f:
      return True
  return False


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('action', choices=['download', 'upload'],
                      help='Which action to perform')
  parser.add_argument('--dry_run', action='store_true',
                      default=False, help='Dry run for uploading')
  args = parser.parse_args()

  if args.action == 'download':
    for d in GOLDEN_DIRECTORIES:
      download(d, _is_file_of_interest,
               'RenderTest Goldens', STORAGE_BUCKET)
  else:
    for d in GOLDEN_DIRECTORIES:
      upload(d, _is_file_of_interest,
             'RenderTest Goldens', STORAGE_BUCKET, args.dry_run)


if __name__ == '__main__':
  main()
