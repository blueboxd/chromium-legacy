// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_DEVICE_PERF_INFO_H_
#define GPU_CONFIG_DEVICE_PERF_INFO_H_

#include <string>
#include <vector>

#include "gpu/gpu_export.h"

namespace gpu {

struct GPU_EXPORT DevicePerfInfo {
  uint32_t score = 0;
};

}  // namespace gpu

#endif  // GPU_CONFIG_DEVICE_PERF_INFO_H_
