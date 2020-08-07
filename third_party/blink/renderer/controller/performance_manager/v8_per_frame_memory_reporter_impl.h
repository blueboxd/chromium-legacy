// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_PERFORMANCE_MANAGER_V8_PER_FRAME_MEMORY_REPORTER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_PERFORMANCE_MANAGER_V8_PER_FRAME_MEMORY_REPORTER_IMPL_H_

#include "third_party/blink/public/mojom/performance_manager/v8_per_frame_memory.mojom-blink.h"
#include "third_party/blink/renderer/controller/controller_export.h"

namespace blink {

// Exposes V8 per-frame associated memory metrics to the browser.
class CONTROLLER_EXPORT V8PerFrameMemoryReporterImpl
    : public mojom::blink::V8PerFrameMemoryReporter {
 public:
  static void Create(
      mojo::PendingReceiver<mojom::blink::V8PerFrameMemoryReporter> receiver);

  void GetPerFrameV8MemoryUsageData(
      Mode mode,
      GetPerFrameV8MemoryUsageDataCallback callback) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_CONTROLLER_PERFORMANCE_MANAGER_V8_PER_FRAME_MEMORY_REPORTER_IMPL_H_
