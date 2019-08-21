// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"

OptimizationGuideNavigationData::OptimizationGuideNavigationData(
    int64_t navigation_id)
    : navigation_id_(navigation_id) {}

OptimizationGuideNavigationData::~OptimizationGuideNavigationData() = default;

OptimizationGuideNavigationData::OptimizationGuideNavigationData(
    const OptimizationGuideNavigationData& other) = default;
