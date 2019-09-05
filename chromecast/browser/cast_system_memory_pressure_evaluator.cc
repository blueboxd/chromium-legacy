// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_system_memory_pressure_evaluator.h"

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"

namespace chromecast {
namespace {

// Memory thresholds (as fraction of total memory) for memory pressure levels.
// See more detailed description of pressure heuristic in PollPressureLevel.
// TODO(halliwell): tune thresholds based on data.
constexpr float kCriticalMemoryFraction = 0.25f;
constexpr float kModerateMemoryFraction = 0.4f;

// Memory thresholds in MB for the simple heuristic based on 'free' memory.
constexpr int kCriticalFreeMemoryKB = 20 * 1024;
constexpr int kModerateFreeMemoryKB = 30 * 1024;

constexpr int kPollingIntervalMS = 5000;

int GetSystemReservedKb() {
  int rtn_kb_ = 0;
  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  base::StringToInt(
      command_line->GetSwitchValueASCII(switches::kMemPressureSystemReservedKb),
      &rtn_kb_);
  DCHECK(rtn_kb_ >= 0);
  return std::max(rtn_kb_, 0);
}

}  // namespace

CastSystemMemoryPressureEvaluator::CastSystemMemoryPressureEvaluator(
    std::unique_ptr<util::MemoryPressureVoter> voter)
    : util::SystemMemoryPressureEvaluator(std::move(voter)),
      critical_memory_fraction_(
          GetSwitchValueDouble(switches::kCastMemoryPressureCriticalFraction,
                               kCriticalMemoryFraction)),
      moderate_memory_fraction_(
          GetSwitchValueDouble(switches::kCastMemoryPressureModerateFraction,
                               kModerateMemoryFraction)),
      system_reserved_kb_(GetSystemReservedKb()),
      weak_ptr_factory_(this) {
  PollPressureLevel();
}

CastSystemMemoryPressureEvaluator::~CastSystemMemoryPressureEvaluator() =
    default;

void CastSystemMemoryPressureEvaluator::PollPressureLevel() {
  base::MemoryPressureListener::MemoryPressureLevel level =
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;

  base::SystemMemoryInfoKB info;
  if (!base::GetSystemMemoryInfo(&info)) {
    LOG(ERROR) << "GetSystemMemoryInfo failed";
  } else if (system_reserved_kb_ != 0 || info.available != 0) {
    // Preferred memory pressure heuristic:
    // 1. Use /proc/meminfo's MemAvailable if possible, fall back to estimate
    // of free + buffers + cached otherwise.
    const int total_available = (info.available != 0)
                                    ? info.available
                                    : (info.free + info.buffers + info.cached);

    // 2. Allow some memory to be 'reserved' on command line.
    const int available = total_available - system_reserved_kb_;
    const int total = info.total - system_reserved_kb_;
    DCHECK_GT(total, 0);
    const float ratio = available / static_cast<float>(total);

    if (ratio < critical_memory_fraction_)
      level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
    else if (ratio < moderate_memory_fraction_)
      level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  } else {
    // Backup method purely using 'free' memory.  It may generate more
    // pressure events than necessary, since more memory may actually be free.
    if (info.free < kCriticalFreeMemoryKB)
      level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
    else if (info.free < kModerateFreeMemoryKB)
      level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  }

  UpdateMemoryPressureLevel(level);

  UMA_HISTOGRAM_PERCENTAGE("Platform.MeminfoMemFree",
                           (info.free * 100.0) / info.total);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Platform.Cast.MeminfoMemFreeDerived2",
                              (info.free + info.buffers + info.cached) / 1024,
                              1, 2000, 100);
  if (info.available != 0) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Platform.Cast.MeminfoMemAvailable2",
                                info.available / 1024, 1, 2000, 100);
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CastSystemMemoryPressureEvaluator::PollPressureLevel,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kPollingIntervalMS));
}

void CastSystemMemoryPressureEvaluator::UpdateMemoryPressureLevel(
    base::MemoryPressureListener::MemoryPressureLevel new_level) {
  auto old_vote = current_vote();
  SetCurrentVote(new_level);

  SendCurrentVote(/* notify = */ current_vote() !=
                  base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);

  if (old_vote == current_vote())
    return;

  metrics::CastMetricsHelper::GetInstance()->RecordApplicationEventWithValue(
      "Memory.Pressure.LevelChange", new_level);
}

}  // namespace chromecast
