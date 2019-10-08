// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_session_statistic.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/optimization_guide_enums.h"
#include "content/public/browser/navigation_handle.h"

namespace optimization_guide {

PredictionManager::PredictionManager() : session_fcp_() {
  g_browser_process->network_quality_tracker()
      ->AddEffectiveConnectionTypeObserver(this);
}

PredictionManager::~PredictionManager() {
  g_browser_process->network_quality_tracker()
      ->RemoveEffectiveConnectionTypeObserver(this);
}

void PredictionManager::UpdateFCPSessionStatistics(base::TimeDelta fcp) {
  previous_load_fcp_ms_ = static_cast<float>(fcp.InMilliseconds());
  session_fcp_.AddSample(*previous_load_fcp_ms_);
}

void PredictionManager::RegisterOptimizationTargets(
    const std::vector<proto::OptimizationTarget>& optimization_targets) {
  SEQUENCE_CHECKER(sequence_checker_);

  for (const auto& optimization_target : optimization_targets) {
    if (optimization_target == proto::OPTIMIZATION_TARGET_UNKNOWN)
      continue;
    if (registered_optimization_targets_.find(optimization_target) !=
        registered_optimization_targets_.end()) {
      continue;
    }
    registered_optimization_targets_.insert(optimization_target);
  }
  // TODO(crbug/1001194): If the HintCacheStore is available/ready, ask it to
  // start loading the registered models.
}

base::Optional<float> PredictionManager::GetValueForClientFeature(
    const std::string& model_feature,
    content::NavigationHandle* navigation_handle) const {
  SEQUENCE_CHECKER(sequence_checker_);

  proto::ClientModelFeature client_model_feature;
  if (!proto::ClientModelFeature_Parse(model_feature, &client_model_feature))
    return base::nullopt;

  switch (client_model_feature) {
    case proto::CLIENT_MODEL_FEATURE_UNKNOWN: {
      return base::nullopt;
    }
    case proto::CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE: {
      return static_cast<float>(current_effective_connection_type_);
    }
    case proto::CLIENT_MODEL_FEATURE_PAGE_TRANSITION: {
      return static_cast<float>(navigation_handle->GetPageTransition());
    }
    case proto::CLIENT_MODEL_FEATURE_SITE_ENGAGEMENT_SCORE: {
      Profile* profile = Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
      SiteEngagementService* engagement_service =
          SiteEngagementService::Get(profile);
      // Precision loss is acceptable/expected for prediction models.
      return static_cast<float>(
          engagement_service->GetScore(navigation_handle->GetURL()));
    }
    case proto::CLIENT_MODEL_FEATURE_SAME_ORIGIN_NAVIGATION: {
      return static_cast<float>(url::IsSameOriginWith(
          navigation_handle->GetURL(), navigation_handle->GetPreviousURL()));
    }
    case proto::CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_MEAN: {
      return session_fcp_.GetMean();
    }
    case proto::
        CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_STANDARD_DEVIATION: {
      return session_fcp_.GetStdDev();
    }
    case proto::
        CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_PREVIOUS_PAGE_LOAD: {
      return previous_load_fcp_ms_.value_or(0.0);
    }
    default: {
      return base::nullopt;
    }
  }
}

base::flat_map<std::string, float> PredictionManager::BuildFeatureMap(
    content::NavigationHandle* navigation_handle,
    const base::flat_set<std::string>& model_features) const {
  SEQUENCE_CHECKER(sequence_checker_);
  base::flat_map<std::string, float> feature_map;
  if (model_features.size() == 0)
    return feature_map;

  const base::flat_map<std::string, float>* host_model_features = nullptr;

  std::string host = navigation_handle->GetURL().host();
  auto it = host_model_features_map_.find(host);
  if (it != host_model_features_map_.end())
    host_model_features = &(it->second);

  UMA_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.HasHostModelFeaturesForHost",
      host_model_features != nullptr);

  // If the feature is not implemented by the client, it is assumed that it is a
  // host model feature we have in the map. If it is not in either, a default is
  // created for it. This ensures that the prediction model will have values for
  // every feature that it requires to be evaluated.
  for (const auto& model_feature : model_features) {
    base::Optional<float> value =
        GetValueForClientFeature(model_feature, navigation_handle);
    if (value) {
      feature_map[model_feature] = *value;
      continue;
    }
    if (!host_model_features || !host_model_features->contains(model_feature)) {
      feature_map[model_feature] = 0.0;
      continue;
    }
    feature_map[model_feature] =
        host_model_features->find(model_feature)->second;
  }
  return feature_map;
}

OptimizationTargetDecision PredictionManager::ShouldTargetNavigation(
    content::NavigationHandle* navigation_handle,
    proto::OptimizationTarget optimization_target) const {
  SEQUENCE_CHECKER(sequence_checker_);
  DCHECK(navigation_handle->GetURL().SchemeIsHTTPOrHTTPS());

  // TODO(crbug/1001194): Add histogram to record that the optimization target
  // was not registered but was requested.
  if (!registered_optimization_targets_.contains(optimization_target))
    return OptimizationTargetDecision::kUnknown;

  auto it = optimization_target_prediction_model_map_.find(optimization_target);
  if (it == optimization_target_prediction_model_map_.end()) {
    // TODO(crbug/1001194): Check the store to see if there is a model
    // available. There will also be a check with metrics on if the model was
    // available in the but not loaded.
    return OptimizationTargetDecision::kModelNotAvailableOnClient;
  }
  PredictionModel* prediction_model = it->second.get();

  base::flat_map<std::string, float> feature_map =
      BuildFeatureMap(navigation_handle, prediction_model->GetModelFeatures());

  return prediction_model->Predict(feature_map);
}

void PredictionManager::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType effective_connection_type) {
  SEQUENCE_CHECKER(sequence_checker_);
  current_effective_connection_type_ = effective_connection_type;
}

void PredictionManager::SeedPredictionModelForTesting(
    proto::OptimizationTarget optimization_target,
    std::unique_ptr<PredictionModel> prediction_model) {
  registered_optimization_targets_.insert(optimization_target);
  optimization_target_prediction_model_map_[optimization_target] =
      std::move(prediction_model);
}

void PredictionManager::SeedHostModelFeaturesMapForTesting(
    const std::string& host,
    base::flat_map<std::string, float> host_model_features) {
  host_model_features_map_[host] = host_model_features;
}

PredictionModel* PredictionManager::GetPredictionModelForTesting(
    proto::OptimizationTarget optimization_target) const {
  auto it = optimization_target_prediction_model_map_.find(optimization_target);
  if (it != optimization_target_prediction_model_map_.end())
    return it->second.get();
  return nullptr;
}

}  // namespace optimization_guide
