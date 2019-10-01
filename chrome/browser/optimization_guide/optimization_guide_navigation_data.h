// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/optional.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/optimization_guide_enums.h"
#include "components/optimization_guide/proto/hints.pb.h"

// A representation of optimization guide information related to a navigation.
// This also includes methods for recording metrics based on this data.
class OptimizationGuideNavigationData {
 public:
  explicit OptimizationGuideNavigationData(int64_t navigation_id);
  ~OptimizationGuideNavigationData();

  OptimizationGuideNavigationData(const OptimizationGuideNavigationData& other);

  // Records metrics based on data currently held in |this|. |has_committed|
  // indicates whether commit-time metrics should be recorded.
  void RecordMetrics(bool has_committed) const;

  // The navigation ID of the navigation handle that this data is associated
  // with.
  int64_t navigation_id() const { return navigation_id_; }

  // The serialized hints version for the hint that applied to the navigation.
  base::Optional<std::string> serialized_hint_version_string() const {
    return serialized_hint_version_string_;
  }
  void set_serialized_hint_version_string(
      const std::string& serialized_hint_version_string) {
    serialized_hint_version_string_ = serialized_hint_version_string;
  }

  // Returns the latest decision made for |optimization_type|.
  base::Optional<optimization_guide::OptimizationTypeDecision>
  GetDecisionForOptimizationType(
      optimization_guide::proto::OptimizationType optimization_type) const;
  // Sets the |decision| for |optimization_type|.
  void SetDecisionForOptimizationType(
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationTypeDecision decision);

  // Returns the latest decision made for |optimmization_target|.
  base::Optional<optimization_guide::OptimizationTargetDecision>
  GetDecisionForOptimizationTarget(
      optimization_guide::OptimizationTarget optimization_target) const;
  // Sets the |decision| for |optimization_target|.
  void SetDecisionForOptimizationTarget(
      optimization_guide::OptimizationTarget optimization_target,
      optimization_guide::OptimizationTargetDecision decision);

  // Whether the hint cache had a hint for the navigation before commit.
  base::Optional<bool> has_hint_before_commit() const {
    return has_hint_before_commit_;
  }
  void set_has_hint_before_commit(bool has_hint_before_commit) {
    has_hint_before_commit_ = has_hint_before_commit;
  }

  // Whether the hint cache had a hint after commit.
  base::Optional<bool> has_hint_after_commit() const {
    return has_hint_after_commit_;
  }
  void set_has_hint_after_commit(bool has_hint_after_commit) {
    has_hint_after_commit_ = has_hint_after_commit;
  }

  // The page hint applicable for the navigation.
  bool has_page_hint_value() const { return !!page_hint_; }
  const optimization_guide::proto::PageHint* page_hint() const {
    return page_hint_.value().get();
  }
  void set_page_hint(
      std::unique_ptr<optimization_guide::proto::PageHint> page_hint) {
    page_hint_ = std::move(page_hint);
  }

  // Whether the host was covered by a hints fetch at the start of navigation.
  base::Optional<bool> was_host_covered_by_fetch_at_navigation_start() const {
    return was_host_covered_by_fetch_at_navigation_start_;
  }
  void set_was_host_covered_by_fetch_at_navigation_start(
      bool was_host_covered_by_fetch_at_navigation_start) {
    was_host_covered_by_fetch_at_navigation_start_ =
        was_host_covered_by_fetch_at_navigation_start;
  }

 private:
  // Records hint cache histograms based on data currently held in |this|.
  void RecordHintCacheMatch(bool has_committed) const;

  // Records histograms for the decisions made for each optimization target and
  // type that was queried for the navigation based on data currently held in
  // |this|.
  void RecordOptimizationTypeAndTargetDecisions() const;

  // Records the OptimizationGuide UKM event based on data currently held in
  // |this|.
  void RecordOptimizationGuideUKM() const;

  // The navigation ID of the navigation handle that this data is associated
  // with.
  const int64_t navigation_id_;

  // The serialized hints version for the hint that applied to the navigation.
  base::Optional<std::string> serialized_hint_version_string_;

  // The map from optimization type to the last decision made for that type.
  std::unordered_map<optimization_guide::proto::OptimizationType,
                     optimization_guide::OptimizationTypeDecision>
      optimization_type_decisions_;

  // The map from optimization target to the last decision made for that target.
  std::unordered_map<optimization_guide::OptimizationTarget,
                     optimization_guide::OptimizationTargetDecision>
      optimization_target_decisions_;

  // Whether the hint cache had a hint for the navigation before commit.
  base::Optional<bool> has_hint_before_commit_;

  // Whether the hint cache had a hint for the navigation after commit.
  base::Optional<bool> has_hint_after_commit_;

  // The page hint for the navigation.
  base::Optional<std::unique_ptr<optimization_guide::proto::PageHint>>
      page_hint_;

  // Whether the host was covered by a hints fetch at the start of
  // navigation.
  base::Optional<bool> was_host_covered_by_fetch_at_navigation_start_;

  DISALLOW_ASSIGN(OptimizationGuideNavigationData);
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_
