# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import mock

def _ParseANGLEGpuVendorString(device_string):
  if not device_string:
    return None
  # ANGLE's device (renderer) string is of the form:
  # "ANGLE (vendor_string, renderer_string, gl_version profile)"
  # This function will be used to get the first value in the tuple
  match = re.search(r'ANGLE \((.*), .*, .*\)', device_string)
  if match:
    return match.group(1)
  else:
    return None


def _GetANGLEGpuDeviceId(device_string):
  if not device_string:
    return None
  # ANGLE's device (renderer) string is of the form:
  # "ANGLE (vendor_string, renderer_string, gl_version profile)"
  # This function will be used to get the second value in the tuple
  match = re.search(r'ANGLE \(.*, (.*), .*\)', device_string)
  if match:
    return match.group(1)
  else:
    return None


def GetGpuVendorString(gpu_info):
  if gpu_info:
    primary_gpu = gpu_info.devices[0]
    if primary_gpu:
      vendor_string = primary_gpu.vendor_string
      angle_vendor_string = _ParseANGLEGpuVendorString(
        primary_gpu.device_string)
      vendor_id = primary_gpu.vendor_id
      if vendor_id == 0x10DE:
        return 'nvidia'
      elif vendor_id == 0x1002:
        return 'amd'
      elif vendor_id == 0x8086:
        return 'intel'
      elif angle_vendor_string:
        return angle_vendor_string.lower()
      elif vendor_string:
        return vendor_string.split(' ')[0].lower()
  return 'unknown_gpu'


def GetGpuDeviceId(gpu_info):
  if gpu_info:
    primary_gpu = gpu_info.devices[0]
    if primary_gpu:
      return (
          primary_gpu.device_id
          or _GetANGLEGpuDeviceId(
              primary_gpu.device_string)
          or primary_gpu.device_string)
  return 0


def GetGpuDriverVendor(gpu_info):
  if gpu_info:
    primary_gpu = gpu_info.devices[0]
    if primary_gpu:
      return primary_gpu.driver_vendor
  return None


def GetGpuDriverVersion(gpu_info):
  if gpu_info:
    primary_gpu = gpu_info.devices[0]
    if primary_gpu:
      return primary_gpu.driver_version
  return None


def GetANGLERenderer(gpu_info):
  if gpu_info and gpu_info.aux_attributes:
    gl_renderer = gpu_info.aux_attributes.get('gl_renderer')
    if gl_renderer and 'ANGLE' in gl_renderer:
      if 'Direct3D11' in gl_renderer:
        return 'd3d11'
      elif 'Direct3D9' in gl_renderer:
        return 'd3d9'
      elif 'OpenGL ES' in gl_renderer:
        return 'opengles'
      elif 'OpenGL' in gl_renderer:
        return 'opengl'
      elif 'Vulkan' in gl_renderer:
        return 'vulkan'
  return 'no_angle'


def GetCommandDecoder(gpu_info):
  if gpu_info and gpu_info.aux_attributes and \
      gpu_info.aux_attributes.get('passthrough_cmd_decoder', False):
    return 'passthrough'
  return 'no_passthrough'

# Used to parse additional options sent to the browser instance via
# '--extra-browser-args', looking for '--enable-features=UseSkiaRenderer' which
# may be merged with additional feature flags.
def GetSkiaRenderer(extra_browser_args):
  if extra_browser_args:
    for o in extra_browser_args:
      if "UseSkiaRenderer" in o:
        return 'skia-renderer'
      if "--disable-vulkan-fallback-to-gl-for-testing" in o:
        return 'skia-renderer'
  return 'no-skia-renderer'

# Used to parse additional options sent to the browser instance via
# '--extra-browser-args', looking for '--use-vulkan='.
def GetVulkan(extra_browser_args):
  if extra_browser_args:
    for o in extra_browser_args:
      if "--use-vulkan=" in o:
        return 'use-vulkan'
  return 'no-use-vulkan'

# used by unittests to create a mock arguments object
def GetMockArgs(is_asan=False, webgl_version='1.0.0'):
  args = mock.MagicMock()
  args.is_asan = is_asan
  args.webgl_conformance_version = webgl_version
  args.webgl2_only = False
  args.url = 'https://www.google.com'
  args.duration = 10
  args.delay = 10
  args.resolution = 100
  args.fullscreen = False
  args.underlay = False
  args.logdir = '/tmp'
  args.repeat = 1
  args.outliers = 0
  args.bypass_ipg = False
  args.expected_vendor_id = 0
  args.expected_device_id = 0
  args.browser_options = []
  return args
