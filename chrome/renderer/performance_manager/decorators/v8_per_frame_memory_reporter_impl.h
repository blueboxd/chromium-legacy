// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PERFORMANCE_MANAGER_DECORATORS_V8_PER_FRAME_MEMORY_REPORTER_IMPL_H_
#define CHROME_RENDERER_PERFORMANCE_MANAGER_DECORATORS_V8_PER_FRAME_MEMORY_REPORTER_IMPL_H_

#include "chrome/common/performance_manager/mojom/v8_per_frame_memory.mojom.h"

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

#endif  // CHROME_RENDERER_PERFORMANCE_MANAGER_DECORATORS_V8_PER_FRAME_MEMORY_REPORTER_IMPL_H_
