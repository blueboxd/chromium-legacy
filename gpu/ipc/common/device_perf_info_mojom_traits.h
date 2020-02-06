// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_DEVICE_PERF_INFO_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_DEVICE_PERF_INFO_MOJOM_TRAITS_H_

#include "gpu/ipc/common/device_perf_info.mojom.h"

namespace mojo {

template <>
struct StructTraits<gpu::mojom::DevicePerfInfoDataView, gpu::DevicePerfInfo> {
  static bool Read(gpu::mojom::DevicePerfInfoDataView data,
                   gpu::DevicePerfInfo* out);

  static uint32_t score(const gpu::DevicePerfInfo& info) { return info.score; }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_DEVICE_PERF_INFO_MOJOM_TRAITS_H_
