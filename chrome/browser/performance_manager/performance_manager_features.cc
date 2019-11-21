// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/performance_manager_features.h"

namespace features {

// Enables freezing pages directly from PerformanceManager rather than via
// TabManager.
const base::Feature kPageFreezingFromPerformanceManager{
    "PageFreezingFromPerformanceManager", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
