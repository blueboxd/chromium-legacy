// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/optimization_guide_enums.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"

// A representation of optimization guide information related to a navigation.
// Metrics will be recorded upon this object's destruction.
class OptimizationGuideNavigationData {
 public:
  explicit OptimizationGuideNavigationData(int64_t navigation_id);
  ~OptimizationGuideNavigationData();

  OptimizationGuideNavigationData(
      const OptimizationGuideNavigationData& other) = delete;
  OptimizationGuideNavigationData& operator=(
      const OptimizationGuideNavigationData&) = delete;

  // Returns the OptimizationGuideNavigationData for |navigation_handle|. Will
  // return nullptr if one cannot be created for it for any reason.
  static OptimizationGuideNavigationData* GetFromNavigationHandle(
      content::NavigationHandle* navigation_handle);

  base::WeakPtr<OptimizationGuideNavigationData> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

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
      optimization_guide::proto::OptimizationTarget optimization_target) const;
  // Sets the |decision| for |optimization_target|.
  void SetDecisionForOptimizationTarget(
      optimization_guide::proto::OptimizationTarget optimization_target,
      optimization_guide::OptimizationTargetDecision decision);

  // Returns the version of the model evaluated for |optimization_target|.
  base::Optional<int64_t> GetModelVersionForOptimizationTarget(
      optimization_guide::proto::OptimizationTarget optimization_target) const;
  // Sets the |model_version| for |optimization_target|.
  void SetModelVersionForOptimizationTarget(
      optimization_guide::proto::OptimizationTarget optimization_target,
      int64_t model_version);

  // Returns the prediction score of the model evaluated for
  // |optimization_target|.
  base::Optional<double> GetModelPredictionScoreForOptimizationTarget(
      optimization_guide::proto::OptimizationTarget optimization_target) const;
  // Sets the |model_prediction_score| for |optimization_target|.
  void SetModelPredictionScoreForOptimizationTarget(
      optimization_guide::proto::OptimizationTarget optimization_target,
      double model_prediction_score);

  // Returns the value of the model feature if it has been provided.
  base::Optional<float> GetValueForModelFeatureForTesting(
      optimization_guide::proto::ClientModelFeature model_feature);
  // Sets the value provided to the model for a particular model feature.
  void SetValueForModelFeature(
      optimization_guide::proto::ClientModelFeature model_feature,
      float value);

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

  // Whether the host was covered by a hints fetch at commit.
  base::Optional<bool> was_host_covered_by_fetch_at_commit() const {
    return was_host_covered_by_fetch_at_commit_;
  }
  void set_was_host_covered_by_fetch_at_commit(
      bool was_host_covered_by_fetch_at_commit) {
    was_host_covered_by_fetch_at_commit_ = was_host_covered_by_fetch_at_commit;
  }

  // Whether a hint was attempted to be fetched from the remote Optimization
  // Guide Service at some point during the navigation.
  base::Optional<bool> was_hint_for_host_attempted_to_be_fetched() const {
    return was_hint_for_host_attempted_to_be_fetched_;
  }
  void set_was_hint_for_host_attempted_to_be_fetched(
      bool was_hint_for_host_attempted_to_be_fetched) {
    was_hint_for_host_attempted_to_be_fetched_ =
        was_hint_for_host_attempted_to_be_fetched;
  }

  // Whether the initiation of the navigation was from a same origin URL or not.
  bool is_same_origin_navigation() const { return is_same_origin_navigation_; }
  void set_is_same_origin_navigation(bool is_same_origin_navigation) {
    is_same_origin_navigation_ = is_same_origin_navigation;
  }

  // The duration between the fetch for a hint for the navigation going out to
  // when it was received by the client if a fetch was initiated for the
  // navigation.
  base::Optional<base::TimeDelta> hints_fetch_latency() const;
  void set_hints_fetch_start(base::TimeTicks hints_fetch_start) {
    hints_fetch_start_ = hints_fetch_start;
  }
  void set_hints_fetch_end(base::TimeTicks hints_fetch_end) {
    hints_fetch_end_ = hints_fetch_end;
  }

 private:
  // Records metrics based on data currently held in |this|.
  void RecordMetrics() const;

  // Records histograms for the decisions made for each optimization target and
  // type that was queried for the navigation based on data currently held in
  // |this|.
  void RecordOptimizationTypeAndTargetDecisions() const;

  // Records the OptimizationGuide UKM event based on data currently held in
  // |this|.
  void RecordOptimizationGuideUKM() const;

  // Returns whether the host was covered by a hint or a fetch based on data
  // currently held in |this| at navigation start.
  bool WasHostCoveredByHintOrFetchAtNavigationStart() const;

  // Returns whether the host was covered by a hint or a fetch based on data
  // currently held in |this| at commit.
  bool WasHostCoveredByHintOrFetchAtCommit() const;

  // The navigation ID of the navigation handle that this data is associated
  // with.
  const int64_t navigation_id_;

  // The serialized hints version for the hint that applied to the navigation.
  base::Optional<std::string> serialized_hint_version_string_;

  // The map from optimization type to the last decision made for that type.
  base::flat_map<optimization_guide::proto::OptimizationType,
                 optimization_guide::OptimizationTypeDecision>
      optimization_type_decisions_;

  // The map from optimization target to the last decision made for that target.
  base::flat_map<optimization_guide::proto::OptimizationTarget,
                 optimization_guide::OptimizationTargetDecision>
      optimization_target_decisions_;

  // The version of the painful page load model that was evaluated for the
  // page load.
  base::flat_map<optimization_guide::proto::OptimizationTarget, int64_t>
      optimization_target_model_versions_;

  // The score output after evaluating the painful page load model. If
  // populated, this is 100x the fractional value output by the model
  // evaluation.
  base::flat_map<optimization_guide::proto::OptimizationTarget, double>
      optimization_target_model_prediction_scores_;

  // The features used to make a prediction for any target.
  base::flat_map<optimization_guide::proto::ClientModelFeature, float>
      prediction_model_features_;

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

  // Whether the host was covered by a hints fetch at commit.
  base::Optional<bool> was_host_covered_by_fetch_at_commit_;

  // Whether a hint for the host was attempted to be fetched at some point
  // during the navigation.
  base::Optional<bool> was_hint_for_host_attempted_to_be_fetched_;

  // Whether the initiation of the navigation was from a same origin URL or not.
  bool is_same_origin_navigation_ = false;

  // The time that the hints fetch for this navigation started. Is only present
  // if a fetch was initiated for this navigation.
  base::Optional<base::TimeTicks> hints_fetch_start_;

  // The time that the hints fetch for the navigation ended. Is only present if
  // a fetch was initiated and successfully completed for this navigation.
  base::Optional<base::TimeTicks> hints_fetch_end_;

  // Used to get |weak_ptr_| to self on the UI thread.
  base::WeakPtrFactory<OptimizationGuideNavigationData> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_
