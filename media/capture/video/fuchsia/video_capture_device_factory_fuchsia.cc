// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/fuchsia/video_capture_device_factory_fuchsia.h"

#include "base/check_op.h"
#include "base/notreached.h"

namespace media {

VideoCaptureDeviceFactoryFuchsia::VideoCaptureDeviceFactoryFuchsia() = default;
VideoCaptureDeviceFactoryFuchsia::~VideoCaptureDeviceFactoryFuchsia() = default;

std::unique_ptr<VideoCaptureDevice>
VideoCaptureDeviceFactoryFuchsia::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return nullptr;
}

void VideoCaptureDeviceFactoryFuchsia::GetDeviceDescriptors(
    VideoCaptureDeviceDescriptors* device_descriptors) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  device_descriptors->clear();
}

void VideoCaptureDeviceFactoryFuchsia::GetSupportedFormats(
    const VideoCaptureDeviceDescriptor& device,
    VideoCaptureFormats* capture_formats) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  capture_formats->clear();
}

}  // namespace media
