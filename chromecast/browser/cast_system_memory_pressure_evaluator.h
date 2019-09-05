// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_
#define CHROMECAST_BROWSER_CAST_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/util/memory_pressure/system_memory_pressure_evaluator.h"

namespace chromecast {

// Memory pressure evaluator for Cast: polls for current memory
// usage periodically and sends memory pressure notifications.
class CastSystemMemoryPressureEvaluator
    : public util::SystemMemoryPressureEvaluator {
 public:
  explicit CastSystemMemoryPressureEvaluator(
      std::unique_ptr<util::MemoryPressureVoter> voter);
  ~CastSystemMemoryPressureEvaluator() override;

 private:
  void PollPressureLevel();
  void UpdateMemoryPressureLevel(
      base::MemoryPressureListener::MemoryPressureLevel new_level);

  const float critical_memory_fraction_;
  const float moderate_memory_fraction_;

  const int system_reserved_kb_;
  base::WeakPtrFactory<CastSystemMemoryPressureEvaluator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastSystemMemoryPressureEvaluator);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_
