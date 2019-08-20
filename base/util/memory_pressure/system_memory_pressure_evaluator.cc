// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/memory_pressure/system_memory_pressure_evaluator.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/util/memory_pressure/system_memory_pressure_evaluator_win.h"
#endif

namespace util {

// static
std::unique_ptr<SystemMemoryPressureEvaluator>
SystemMemoryPressureEvaluator::CreateDefaultSystemEvaluator(
    MultiSourceMemoryPressureMonitor* monitor) {
#if defined(OS_WIN)
  return std::make_unique<util::win::SystemMemoryPressureEvaluator>(
      monitor->CreateVoter());
#endif
  return nullptr;
}

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator() = default;

}  // namespace util
