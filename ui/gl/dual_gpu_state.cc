// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dual_gpu_state.h"

#include "base/trace_event/trace_event.h"

namespace gl {

DualGPUState::DualGPUState() : num_high_performance_contexts_(0) {}

void DualGPUState::RegisterHighPerformanceContext() {
  CancelDelayedSwitchToLowPowerGPU();
  num_high_performance_contexts_++;
  SwitchToHighPerformanceGPUIfNeeded();
}

void DualGPUState::RemoveHighPerformanceContext() {
  DCHECK(num_high_performance_contexts_ > 0);

  num_high_performance_contexts_--;
  if (num_high_performance_contexts_ == 0)
    AttemptSwitchToLowPowerGPUWithDelay();
}

}  // namespace gl
