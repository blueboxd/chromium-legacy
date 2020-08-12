// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/android/cpu_affinity_experiments.h"

#include "base/cpu_affinity_posix.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_handle.h"
#include "chrome/common/chrome_features.h"

namespace chrome {

void InitializeCpuAffinityExperiments() {
  if (!base::FeatureList::IsEnabled(
          features::kCpuAffinityRestrictToLittleCores)) {
    return;
  }

  // Restrict affinity of all existing threads of the current process. The
  // affinity is inherited by any subsequently created thread. While
  // InitializeThreadAffinityExperiments() is called early during startup, other
  // threads (e.g. Java threads like the RenderThread) may already exist, so
  // setting the affinity only for the current thread is not enough here.
  bool success = base::SetProcessCpuAffinityMode(
      base::GetCurrentProcessHandle(), base::CpuAffinityMode::kLittleCoresOnly);

  base::UmaHistogramBoolean(
      "Power.CpuAffinityExperiments.ProcessAffinityUpdateSuccess", success);
}

}  // namespace chrome
