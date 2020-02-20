// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_DEVICE_PERF_INFO_H_
#define GPU_CONFIG_DEVICE_PERF_INFO_H_

#include <string>
#include <vector>

#include "build/build_config.h"
#include "gpu/gpu_export.h"

#if defined(OS_WIN)
#include <d3dcommon.h>
#endif

namespace gpu {

enum class IntelGpuGeneration {
  kNonIntel = -1,
  kUnknownIntel = 0,  // Intel GPU, but not one of the following generations.
  kGen1 = 1,
  kGen2 = 2,
  kGen3 = 3,
  kGen4 = 4,
  kGen5 = 5,
  kGen6 = 6,
  kGen7 = 7,
  kGen8 = 8,
  kGen9 = 9,
  kGen10 = 10,
  kGen11 = 11,
  kGen12 = 12,
  kMaxValue = kGen12,
};

struct GPU_EXPORT DevicePerfInfo {
  uint32_t total_physical_memory_mb = 0u;
  uint32_t total_disk_space_mb = 0u;
  uint32_t hardware_concurrency = 0u;
#if defined(OS_WIN)
  // system commit limit (n pages) x page size.
  uint32_t system_commit_limit_mb = 0u;
  // If multiple GPUs are detected, this holds the highest feature level.
  D3D_FEATURE_LEVEL d3d11_feature_level = D3D_FEATURE_LEVEL_1_0_CORE;
  bool has_discrete_gpu = false;
#endif

  // The following fields are only filled on the browser side. They don't
  // need to be part of mojom.
  IntelGpuGeneration intel_gpu_generation = IntelGpuGeneration::kNonIntel;
  bool software_rendering = false;
};

}  // namespace gpu

#endif  // GPU_CONFIG_DEVICE_PERF_INFO_H_
