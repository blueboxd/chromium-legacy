// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_TEST_OPTIMIZATION_GUIDE_DECIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_TEST_OPTIMIZATION_GUIDE_DECIDER_H_

#include "components/optimization_guide/optimization_guide_decider.h"

namespace optimization_guide {

// An implementation of |OptimizationGuideDecider| that can be selectively
// mocked out for unit testing features that rely on the Optimization Guide in
// //components/...
class TestOptimizationGuideDecider : public OptimizationGuideDecider {
 public:
  TestOptimizationGuideDecider();
  ~TestOptimizationGuideDecider() override;
  TestOptimizationGuideDecider(const TestOptimizationGuideDecider&) = delete;
  TestOptimizationGuideDecider& operator=(const TestOptimizationGuideDecider&) =
      delete;

  // OptimizationGuideDecider implementation:
  void RegisterOptimizationTypesAndTargets(
      const std::vector<proto::OptimizationType>& optimization_types,
      const std::vector<proto::OptimizationTarget>& optimization_targets)
      override;
  OptimizationGuideDecision ShouldTargetNavigation(
      content::NavigationHandle* navigation_handle,
      proto::OptimizationTarget optimization_target) override;
  OptimizationGuideDecision CanApplyOptimization(
      content::NavigationHandle* navigation_handle,
      proto::OptimizationType optimization_type,
      OptimizationMetadata* optimization_metadata) override;
  void CanApplyOptimizationAsync(
      content::NavigationHandle* navigation_handle,
      proto::OptimizationType optimization_type,
      OptimizationGuideDecisionCallback callback) override;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_TEST_OPTIMIZATION_GUIDE_DECIDER_H_
