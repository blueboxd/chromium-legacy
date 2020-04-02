// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_DECIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_DECIDER_H_

#include <vector>

#include "base/callback_forward.h"
#include "components/optimization_guide/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace optimization_guide {

// Represents the decision made by the optimization guide.
// Keep in sync with OptimizationGuideOptimizationGuideDecision in enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.optimization_guide
enum class OptimizationGuideDecision {
  // The necessary information to make the decision is not yet available.
  kUnknown,
  // The necessary information to make the decision is available and the
  // decision is affirmative.
  kTrue,
  // The necessary information to make the decision is available and the
  // decision is negative.
  kFalse,

  // New values above this line.
  kMaxValue = kFalse,
};

using OptimizationGuideDecisionCallback =
    base::OnceCallback<void(optimization_guide::OptimizationGuideDecision,
                            const optimization_guide::OptimizationMetadata&)>;

class OptimizationGuideDecider {
 public:
  // Registers the optimization types and targets that intend to be queried
  // during the session. It is expected for this to be called after the browser
  // has been initialized.
  virtual void RegisterOptimizationTypesAndTargets(
      const std::vector<proto::OptimizationType>& optimization_types,
      const std::vector<proto::OptimizationTarget>& optimization_targets) = 0;

  // Returns whether the current conditions match |optimization_target|. This
  // should only be called for main frame navigations.
  virtual OptimizationGuideDecision ShouldTargetNavigation(
      content::NavigationHandle* navigation_handle,
      proto::OptimizationTarget optimization_target) = 0;

  // Returns whether |optimization_type| can be applied for the URL associated
  // with |navigation_handle|. This should only be called for main frame
  // navigations.
  virtual OptimizationGuideDecision CanApplyOptimization(
      content::NavigationHandle* navigation_handle,
      proto::OptimizationType optimization_type,
      OptimizationMetadata* optimization_metadata) = 0;

  // Invokes |callback| with the decision for the URL contained in
  // |navigation_handle| and |optimization_type|, when sufficient information
  // has been collected to make the decision. This should only be called for
  // main frame navigations.
  virtual void CanApplyOptimizationAsync(
      content::NavigationHandle* navigation_handle,
      proto::OptimizationType optimization_type,
      OptimizationGuideDecisionCallback callback) = 0;

 protected:
  OptimizationGuideDecider() {}
  virtual ~OptimizationGuideDecider() {}
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_DECIDER_H_
