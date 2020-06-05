// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PERFORMANCE_MANAGER_V8_PER_FRAME_MEMORY_REPORTER_IMPL_H_
#define CONTENT_RENDERER_PERFORMANCE_MANAGER_V8_PER_FRAME_MEMORY_REPORTER_IMPL_H_

#include "content/public/common/performance_manager/v8_per_frame_memory.mojom.h"

namespace performance_manager {

// Exposes V8 per-frame associated memory metrics to the browser.
class V8PerFrameMemoryReporterImpl : public mojom::V8PerFrameMemoryReporter {
 public:
  static void Create(
      mojo::PendingReceiver<mojom::V8PerFrameMemoryReporter> receiver);

  void GetPerFrameV8MemoryUsageData(
      GetPerFrameV8MemoryUsageDataCallback callback) override;
};

}  // namespace performance_manager

#endif  // CONTENT_RENDERER_PERFORMANCE_MANAGER_V8_PER_FRAME_MEMORY_REPORTER_IMPL_H_
