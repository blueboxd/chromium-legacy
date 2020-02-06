// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/device_perf_info_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<gpu::mojom::DevicePerfInfoDataView, gpu::DevicePerfInfo>::
    Read(gpu::mojom::DevicePerfInfoDataView data, gpu::DevicePerfInfo* out) {
  out->score = data.score();
  return true;
}

}  // namespace mojo
