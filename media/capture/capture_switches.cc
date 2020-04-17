// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/capture_switches.h"

namespace switches {

// Enables GpuMemoryBuffer-based buffer pool.
const char kVideoCaptureUseGpuMemoryBuffer[] =
    "video-capture-use-gpu-memory-buffer";

// This is for the same feature controlled by kVideoCaptureUseGpuMemoryBuffer.
// kVideoCaptureUseGpuMemoryBuffer is settled by chromeos overlays. This flag is
// necessary to overwrite the settings via chrome:// flag. The behavior of
// chrome://flag#zero-copy-video-capture is as follows;
// Default  : Respect chromeos overlays settings.
// Enabled  : Force to enable kVideoCaptureUseGpuMemoryBuffer.
// Disabled : Force to disable kVideoCaptureUseGpuMemoryBuffer.
const char kDisableVideoCaptureUseGpuMemoryBuffer[] =
    "disable-video-capture-use-gpu-memory-buffer";

}  // namespace switches
