// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/system/core_scheduling.h"

#include <base/logging.h>
#include <errno.h>
#include <sys/prctl.h>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

#ifndef PR_SET_CORE_SCHED
#define PR_SET_CORE_SCHED 57
#endif

namespace chromeos {
namespace system {

namespace {
const base::Feature kCoreScheduling{"CoreSchedulingEnabled",
                                    base::FEATURE_ENABLED_BY_DEFAULT};
}

void EnableCoreSchedulingIfAvailable() {
  if (!base::FeatureList::IsEnabled(kCoreScheduling)) {
    return;
  }

  if (prctl(PR_SET_CORE_SCHED, 1) == -1) {
    // prctl(2) will return EINVAL for unknown functions. We're tolerant to this
    // and will log an error message for non EINVAL errnos.
    PLOG_IF(WARNING, errno != EINVAL) << "Unable to set core scheduling";
  }
}

}  // namespace system
}  // namespace chromeos
