// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MEMORY_SWAP_CONFIGURATION_H_
#define CHROMEOS_MEMORY_SWAP_CONFIGURATION_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// Controls the ChromeOS /proc/sys/vm/min_filelist_kb swap tunable, if the
// feature is enabled it will use the value (in MB) from the feature param.
extern const base::Feature kCrOSTuneMinFilelist;
extern const base::FeatureParam<int> kCrOSTuneMinFilelistMb;

// Configure swap will configure any swap related experiments that this user may
// be opted into.
CHROMEOS_EXPORT void ConfigureSwap();

}  // namespace chromeos

#endif  // CHROMEOS_MEMORY_SWAP_CONFIGURATION_H_
