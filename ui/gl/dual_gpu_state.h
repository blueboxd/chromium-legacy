// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DUAL_GPU_STATE_H_
#define UI_GL_DUAL_GPU_STATE_H_

#include "base/macros.h"
#include "ui/gl/gl_export.h"

namespace gl {

class GL_EXPORT DualGPUState {
 public:
  void RegisterHighPerformanceContext();
  void RemoveHighPerformanceContext();

 protected:
  DualGPUState();

 private:
  virtual void SwitchToHighPerformanceGPUIfNeeded() = 0;
  virtual void SwitchToLowPowerGPU() = 0;
  virtual void AttemptSwitchToLowPowerGPUWithDelay() = 0;
  virtual void CancelDelayedSwitchToLowPowerGPU() = 0;

  unsigned int num_high_performance_contexts_;

  DISALLOW_COPY_AND_ASSIGN(DualGPUState);
};

}  // namespace gl

#endif  // UI_GL_DUAL_GPU_STATE_H_
