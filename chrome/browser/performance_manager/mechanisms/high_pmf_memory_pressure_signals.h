// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_HIGH_PMF_MEMORY_PRESSURE_SIGNALS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_HIGH_PMF_MEMORY_PRESSURE_SIGNALS_H_

#include "base/memory/memory_pressure_listener.h"
#include "base/sequence_checker.h"

namespace performance_manager {
namespace mechanism {

class MemoryPressureVoterOnUIThread;

// HighPMFMemoryPressureSignals is meant to be used by a
// HighPMFMemoryPressurePolicy to notify the global MemoryPressureMonitor when
// the total PMF of Chrome exceeds a threshold.
class HighPMFMemoryPressureSignals {
 public:
  using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;

  HighPMFMemoryPressureSignals();
  ~HighPMFMemoryPressureSignals();
  HighPMFMemoryPressureSignals(const HighPMFMemoryPressureSignals& other) =
      delete;
  HighPMFMemoryPressureSignals& operator=(const HighPMFMemoryPressureSignals&) =
      delete;

  // Should be called each time the memory pressure level computed by
  // HighPMFMemoryPressurePolicy changes.
  void SetPressureLevel(MemoryPressureLevel level);

 private:
  MemoryPressureLevel pressure_level_ =
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_NONE;

  std::unique_ptr<MemoryPressureVoterOnUIThread> ui_thread_voter_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace mechanism
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_HIGH_PMF_MEMORY_PRESSURE_SIGNALS_H_
