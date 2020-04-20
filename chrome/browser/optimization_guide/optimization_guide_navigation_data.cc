// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"

#include "base/base64.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"
#include "components/optimization_guide/hints_processing_util.h"
#include "content/public/browser/navigation_handle.h"
#include "net/nqe/effective_connection_type.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace {

// The returned string is used to record histograms for the optimization target.
// Also add the string to OptimizationGuide.OptimizationTargets histogram
// suffixes in histograms.xml.
std::string GetStringNameForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target) {
  switch (optimization_target) {
    case optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN:
      return "Unknown";
    case optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:
      return "PainfulPageLoad";
  }
  NOTREACHED();
  return std::string();
}

}  // namespace

OptimizationGuideNavigationData::OptimizationGuideNavigationData(
    int64_t navigation_id)
    : navigation_id_(navigation_id) {}

OptimizationGuideNavigationData::~OptimizationGuideNavigationData() {
  RecordMetrics();
}

// static
OptimizationGuideNavigationData*
OptimizationGuideNavigationData::GetFromNavigationHandle(
    content::NavigationHandle* navigation_handle) {
  OptimizationGuideWebContentsObserver*
      optimization_guide_web_contents_observer =
          OptimizationGuideWebContentsObserver::FromWebContents(
              navigation_handle->GetWebContents());
  if (!optimization_guide_web_contents_observer)
    return nullptr;
  return optimization_guide_web_contents_observer
      ->GetOrCreateOptimizationGuideNavigationData(navigation_handle);
}

void OptimizationGuideNavigationData::RecordMetrics() const {
  RecordOptimizationTypeAndTargetDecisions();
  RecordOptimizationGuideUKM();
}

void OptimizationGuideNavigationData::RecordOptimizationTypeAndTargetDecisions()
    const {
  // Record optimization type decisions.
  for (const auto& optimization_type_decision : optimization_type_decisions_) {
    optimization_guide::proto::OptimizationType optimization_type =
        optimization_type_decision.first;
    optimization_guide::OptimizationTypeDecision decision =
        optimization_type_decision.second;
    base::UmaHistogramExactLinear(
        base::StringPrintf("OptimizationGuide.ApplyDecision.%s",
                           optimization_guide::GetStringNameForOptimizationType(
                               optimization_type)
                               .c_str()),
        static_cast<int>(decision),
        static_cast<int>(
            optimization_guide::OptimizationTypeDecision::kMaxValue));
  }

  // Record optimization target decisions.
  for (const auto& optimization_target_decision :
       optimization_target_decisions_) {
    optimization_guide::proto::OptimizationTarget optimization_target =
        optimization_target_decision.first;
    optimization_guide::OptimizationTargetDecision decision =
        optimization_target_decision.second;
    base::UmaHistogramExactLinear(
        base::StringPrintf(
            "OptimizationGuide.TargetDecision.%s",
            GetStringNameForOptimizationTarget(optimization_target).c_str()),
        static_cast<int>(decision),
        static_cast<int>(
            optimization_guide::OptimizationTargetDecision::kMaxValue));
  }
}

void OptimizationGuideNavigationData::RecordOptimizationGuideUKM() const {
  bool did_record_metric = false;
  ukm::SourceId ukm_source_id =
      ukm::ConvertToSourceId(navigation_id_, ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::OptimizationGuide builder(ukm_source_id);

  // Record model metrics.
  for (const auto& optimization_target_model_version :
       optimization_target_model_versions_) {
    if (optimization_target_model_version.first ==
        optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD) {
      did_record_metric = true;
      builder.SetPainfulPageLoadModelVersion(
          optimization_target_model_version.second);
    }
  }
  for (const auto& optimization_target_model_prediction_score :
       optimization_target_model_prediction_scores_) {
    if (optimization_target_model_prediction_score.first ==
        optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD) {
      did_record_metric = true;
      builder.SetPainfulPageLoadModelPredictionScore(static_cast<int64_t>(
          100 * optimization_target_model_prediction_score.second));
    }
  }

  for (const auto& model_feature : prediction_model_features_) {
    switch (model_feature.first) {
      case optimization_guide::proto::CLIENT_MODEL_FEATURE_UNKNOWN: {
        continue;
      }
      case optimization_guide::proto::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE: {
        builder.SetPredictionModelFeatureEffectiveConnectionType(
            static_cast<net::EffectiveConnectionType>(model_feature.second));
        did_record_metric = true;
        continue;
      }
      case optimization_guide::proto::CLIENT_MODEL_FEATURE_PAGE_TRANSITION: {
        builder.SetPredictionModelFeaturePageTransition(
            static_cast<ui::PageTransition>(model_feature.second));
        did_record_metric = true;
        continue;
      }
      case optimization_guide::proto::
          CLIENT_MODEL_FEATURE_SITE_ENGAGEMENT_SCORE: {
        builder.SetPredictionModelFeatureSiteEngagementScore(
            static_cast<int>(std::roundf(model_feature.second / 10.0) * 10));
        did_record_metric = true;
        continue;
      }
      case optimization_guide::proto::
          CLIENT_MODEL_FEATURE_SAME_ORIGIN_NAVIGATION: {
        builder.SetPredictionModelFeatureIsSameOriginNavigation(
            static_cast<int>(model_feature.second));
        did_record_metric = true;
        continue;
      }
      case optimization_guide::proto::
          CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_MEAN: {
        builder.SetPredictionModelFeatureNavigationToFCPSessionMean(
            static_cast<int>(model_feature.second));
        did_record_metric = true;
        continue;
      }
      case optimization_guide::proto::
          CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_STANDARD_DEVIATION: {
        builder.SetPredictionModelFeatureNavigationToFCPSessionStdDev(
            static_cast<int>(model_feature.second));
        did_record_metric = true;
        continue;
      }
      case optimization_guide::proto::
          CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_PREVIOUS_PAGE_LOAD: {
        builder.SetPredictionModelFeaturePreviousPageLoadNavigationToFCP(
            static_cast<int>(model_feature.second));
        did_record_metric = true;
        continue;
      }
    }
  }

  // Record hint metrics.
  if (serialized_hint_version_string_.has_value() &&
      !serialized_hint_version_string_.value().empty()) {
    // Deserialize the serialized version string into its protobuffer.
    std::string binary_version_pb;
    if (base::Base64Decode(serialized_hint_version_string_.value(),
                           &binary_version_pb)) {
      optimization_guide::proto::Version hint_version;
      if (hint_version.ParseFromString(binary_version_pb)) {
        if (hint_version.has_generation_timestamp() &&
            hint_version.generation_timestamp().seconds() > 0) {
          did_record_metric = true;
          builder.SetHintGenerationTimestamp(
              hint_version.generation_timestamp().seconds());
        }
        if (hint_version.has_hint_source() &&
            hint_version.hint_source() !=
                optimization_guide::proto::HINT_SOURCE_UNKNOWN) {
          did_record_metric = true;
          builder.SetHintSource(static_cast<int>(hint_version.hint_source()));
        }
      }
    }
  }

  // Record hint coverage metrics.
  if (has_hint_before_commit_.has_value() ||
      has_hint_after_commit_.has_value()) {
    // Only record if we would potentially have had to provide optimization
    // guidance for the navigation.
    if (WasHostCoveredByHintOrFetchAtNavigationStart() ||
        WasHostCoveredByHintOrFetchAtCommit()) {
      builder.SetNavigationHostCovered(static_cast<int>(
          optimization_guide::NavigationHostCoveredStatus::kCovered));
    } else {
      bool hint_was_attempted_to_be_fetched =
          was_hint_for_host_attempted_to_be_fetched_.value_or(false);
      optimization_guide::NavigationHostCoveredStatus status =
          hint_was_attempted_to_be_fetched
              ? optimization_guide::NavigationHostCoveredStatus::
                    kFetchNotSuccessful
              : optimization_guide::NavigationHostCoveredStatus::
                    kFetchNotAttempted;
      builder.SetNavigationHostCovered(static_cast<int>(status));
    }
    did_record_metric = true;
  }

  // Record fetch latency metrics.
  if (hints_fetch_start_.has_value()) {
    if (hints_fetch_latency().has_value()) {
      builder.SetNavigationHintsFetchRequestLatency(
          hints_fetch_latency()->InMilliseconds());
    } else {
      builder.SetNavigationHintsFetchRequestLatency(INT64_MAX);
    }
    did_record_metric = true;
  }

  // Only record UKM if a metric was recorded.
  if (did_record_metric)
    builder.Record(ukm::UkmRecorder::Get());
}

bool OptimizationGuideNavigationData::
    WasHostCoveredByHintOrFetchAtNavigationStart() const {
  return has_hint_before_commit_.value_or(false) ||
         was_host_covered_by_fetch_at_navigation_start_.value_or(false);
}

bool OptimizationGuideNavigationData::WasHostCoveredByHintOrFetchAtCommit()
    const {
  return has_hint_after_commit_.value_or(false) ||
         was_host_covered_by_fetch_at_commit_.value_or(false);
}

base::Optional<optimization_guide::OptimizationTypeDecision>
OptimizationGuideNavigationData::GetDecisionForOptimizationType(
    optimization_guide::proto::OptimizationType optimization_type) const {
  auto optimization_type_decision_iter =
      optimization_type_decisions_.find(optimization_type);
  if (optimization_type_decision_iter == optimization_type_decisions_.end())
    return base::nullopt;
  return optimization_type_decision_iter->second;
}

void OptimizationGuideNavigationData::SetDecisionForOptimizationType(
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationTypeDecision decision) {
  optimization_type_decisions_[optimization_type] = decision;
}

base::Optional<optimization_guide::OptimizationTargetDecision>
OptimizationGuideNavigationData::GetDecisionForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target) const {
  auto optimization_target_decision_iter =
      optimization_target_decisions_.find(optimization_target);
  if (optimization_target_decision_iter == optimization_target_decisions_.end())
    return base::nullopt;
  return optimization_target_decision_iter->second;
}

void OptimizationGuideNavigationData::SetDecisionForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target,
    optimization_guide::OptimizationTargetDecision decision) {
  optimization_target_decisions_[optimization_target] = decision;
}

base::Optional<int64_t>
OptimizationGuideNavigationData::GetModelVersionForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target) const {
  auto optimization_target_model_version_iter =
      optimization_target_model_versions_.find(optimization_target);
  if (optimization_target_model_version_iter ==
      optimization_target_model_versions_.end())
    return base::nullopt;
  return optimization_target_model_version_iter->second;
}

void OptimizationGuideNavigationData::SetModelVersionForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target,
    int64_t model_version) {
  optimization_target_model_versions_[optimization_target] = model_version;
}

base::Optional<double>
OptimizationGuideNavigationData::GetModelPredictionScoreForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target) const {
  auto optimization_target_model_prediction_score_iter =
      optimization_target_model_prediction_scores_.find(optimization_target);
  if (optimization_target_model_prediction_score_iter ==
      optimization_target_model_prediction_scores_.end())
    return base::nullopt;
  return optimization_target_model_prediction_score_iter->second;
}

void OptimizationGuideNavigationData::
    SetModelPredictionScoreForOptimizationTarget(
        optimization_guide::proto::OptimizationTarget optimization_target,
        double model_prediction_score) {
  optimization_target_model_prediction_scores_[optimization_target] =
      model_prediction_score;
}

void OptimizationGuideNavigationData::SetValueForModelFeature(
    optimization_guide::proto::ClientModelFeature model_feature,
    float value) {
  prediction_model_features_[model_feature] = value;
}
base::Optional<float>
OptimizationGuideNavigationData::GetValueForModelFeatureForTesting(
    optimization_guide::proto::ClientModelFeature model_feature) {
  auto it = prediction_model_features_.find(model_feature);
  if (it == prediction_model_features_.end())
    return base::nullopt;
  return it->second;
}

base::Optional<base::TimeDelta>
OptimizationGuideNavigationData::hints_fetch_latency() const {
  if (!hints_fetch_start_ || !hints_fetch_end_) {
    // Either a fetch was not initiated for this navigation or the fetch did not
    // completely successfully.
    return base::nullopt;
  }

  if (*hints_fetch_end_ < *hints_fetch_start_) {
    // This can happen if a hints fetch was started for a redirect, but the
    // fetch had not successfully completed yet.
    return base::nullopt;
  }

  return *hints_fetch_end_ - *hints_fetch_start_;
}
