// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/android/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace page_info {

const base::Feature kPageInfoV2{"PageInfoV2",
                                base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace page_info