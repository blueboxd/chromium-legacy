// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/marking_scheduling_oracle.h"

namespace blink {

constexpr double MarkingSchedulingOracle::kEstimatedMarkingTimeMs;
constexpr base::TimeDelta
    MarkingSchedulingOracle::kDefaultIncrementalMarkingStepDuration;
constexpr size_t MarkingSchedulingOracle::kMinimumMarkedBytesInStep;
constexpr base::TimeDelta
    MarkingSchedulingOracle::kMaximumIncrementalMarkingStepDuration;

MarkingSchedulingOracle::MarkingSchedulingOracle()
    : incremental_marking_start_time_(base::TimeTicks::Now()) {}

void MarkingSchedulingOracle::UpdateIncrementalMarkingStats(
    size_t overall_marked_bytes,
    base::TimeDelta overall_marking_time) {
  incrementally_marked_bytes_ = overall_marked_bytes;
  incremental_marking_time_so_far_ = overall_marking_time;
}

void MarkingSchedulingOracle::AddConcurrentlyMarkedBytes(size_t marked_bytes) {
  base::AutoLock lock(concurrently_marked_bytes_lock_);
  concurrently_marked_bytes_ += marked_bytes;
}

size_t MarkingSchedulingOracle::GetOverallMarkedBytes() {
  base::AutoLock lock(concurrently_marked_bytes_lock_);
  return incrementally_marked_bytes_ + concurrently_marked_bytes_;
}

double MarkingSchedulingOracle::GetElapsedTimeInMs(base::TimeTicks start_time) {
  if (elapsed_time_for_testing_ != kNoSetElapsedTimeForTesting) {
    double elapsed_time = elapsed_time_for_testing_;
    elapsed_time_for_testing_ = kNoSetElapsedTimeForTesting;
    return elapsed_time;
  }
  return (base::TimeTicks::Now() - start_time).InMillisecondsF();
}

base::TimeDelta MarkingSchedulingOracle::GetMinimumStepDuration() {
  DCHECK_LT(0u, incrementally_marked_bytes_);
  DCHECK(!incremental_marking_time_so_far_.is_zero());
  base::TimeDelta minimum_duration = incremental_marking_time_so_far_ *
                                     kMinimumMarkedBytesInStep /
                                     incrementally_marked_bytes_;
  return minimum_duration;
}

base::TimeDelta MarkingSchedulingOracle::GetNextIncrementalStepDurationForTask(
    size_t estimated_live_bytes) {
  if ((incrementally_marked_bytes_ == 0) ||
      incremental_marking_time_so_far_.is_zero()) {
    // Impossible to estimate marking speed. Fallback to default duration.
    return kDefaultIncrementalMarkingStepDuration;
  }
  double elapsed_time_in_ms =
      GetElapsedTimeInMs(incremental_marking_start_time_);
  size_t actual_marked_bytes = GetOverallMarkedBytes();
  double expected_marked_bytes =
      estimated_live_bytes * elapsed_time_in_ms / kEstimatedMarkingTimeMs;
  base::TimeDelta minimum_duration = GetMinimumStepDuration();
  if (expected_marked_bytes < actual_marked_bytes) {
    // Marking is ahead of schedule, incremental marking doesn't need to
    // do anything.
    return std::min(minimum_duration, kMaximumIncrementalMarkingStepDuration);
  }
  // Assuming marking will take |kEstimatedMarkingTime|, overall there will
  // be |estimated_live_bytes| live bytes to mark, and that marking speed is
  // constant, after |elapsed_time| the number of marked_bytes should be
  // |estimated_live_bytes| * (|elapsed_time| / |kEstimatedMarkingTime|),
  // denoted as |expected_marked_bytes|.  If |actual_marked_bytes| is less,
  // i.e. marking is behind schedule, incremental marking should help "catch
  // up" by marking (|expected_marked_bytes| - |actual_marked_bytes|).
  // Assuming constant marking speed, duration of the next incremental step
  // should be as follows:
  double marking_time_to_catch_up_in_ms =
      (expected_marked_bytes - actual_marked_bytes) *
      incremental_marking_time_so_far_.InMillisecondsF() /
      incrementally_marked_bytes_;
  return std::min(
      kMaximumIncrementalMarkingStepDuration,
      std::max(minimum_duration, base::TimeDelta::FromMillisecondsD(
                                     marking_time_to_catch_up_in_ms)));
}

}  // namespace blink
