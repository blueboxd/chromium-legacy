// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UTIL_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_
#define BASE_UTIL_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_

#include "base/util/memory_pressure/memory_pressure_voter.h"
#include "base/util/memory_pressure/multi_source_memory_pressure_monitor.h"

namespace util {

// Base class for the platform SystemMemoryPressureEvaluators, which use
// MemoryPressureVoters to cast their vote on the overall MemoryPressureLevel.
class SystemMemoryPressureEvaluator {
 public:
  // Used by the MemoryPressureMonitor to create the correct Evaluator for the
  // platform in use.
  static std::unique_ptr<SystemMemoryPressureEvaluator>
  CreateDefaultSystemEvaluator(MultiSourceMemoryPressureMonitor* monitor);

  virtual ~SystemMemoryPressureEvaluator() = default;

 protected:
  SystemMemoryPressureEvaluator();
};

}  // namespace util

#endif  // BASE_UTIL_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_
