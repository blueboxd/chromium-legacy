// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MANAGER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chrome/browser/optimization_guide/optimization_guide_session_statistic.h"
#include "components/optimization_guide/optimization_guide_enums.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "url/origin.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace optimization_guide {

enum class OptimizationGuideDecision;
class PredictionModel;

// A PredictionManager supported by the optimization guide that makes an
// OptimizationTargetDecision by evaluating the corresponding prediction model
// for an OptimizationTarget.
class PredictionManager
    : public network::NetworkQualityTracker::EffectiveConnectionTypeObserver {
 public:
  PredictionManager();
  ~PredictionManager() override;

  // Registers the optimization targets that may have ShouldTargetNavigtation
  // requested by consumers of the Optimization Guide.
  void RegisterOptimizationTargets(
      const std::vector<proto::OptimizationTarget>& optimization_targets);

  // Determines if the navigation matches the critieria for
  // |optimization_target|. Returns kUnknown if a PredictionModel for the
  // optimization target is not registered and kModelNotAvailableOnClient if the
  // if model for the optimization target is not currently on the client.
  OptimizationTargetDecision ShouldTargetNavigation(
      content::NavigationHandle* navigation_handle,
      proto::OptimizationTarget optimization_target) const;

  // Updates |session_fcp_| and |previous_fcp_| with |fcp|.
  void UpdateFCPSessionStatistics(base::TimeDelta fcp);

  // network::NetworkQualityTracker::EffectiveConnectionTypeObserver
  // implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override;

 protected:
  // Adds a prediction model for an optimization target into
  // |optimization_target_prediction_model_map_| for testing.
  void SeedPredictionModelForTesting(
      proto::OptimizationTarget optimization_target,
      std::unique_ptr<PredictionModel> prediction_model);

  // Adds host model features for a host into the |host_model_features_map_|
  // for testing.
  void SeedHostModelFeaturesMapForTesting(
      const std::string& host,
      base::flat_map<std::string, float> host_model_features);

  // Returns the prediction model for the optimization target used by this
  // PredictionManager for testing.
  PredictionModel* GetPredictionModelForTesting(
      proto::OptimizationTarget optimization_target) const;

 private:
  // Constructs and returns  a map containing the current feature values for the
  // requested set of model features.
  base::flat_map<std::string, float> BuildFeatureMap(
      content::NavigationHandle* navigation_handle,
      const base::flat_set<std::string>& model_features) const;

  // Calculates and returns the current value for the client feature specified
  // by |model_feature|. Returns nullopt if the client does not support the
  // model feature.
  base::Optional<float> GetValueForClientFeature(
      const std::string& model_feature,
      content::NavigationHandle* navigation_handle) const;

  // A map of optimization target to the prediction model capable of making
  // an optimization target decision for it.
  base::flat_map<proto::OptimizationTarget, std::unique_ptr<PredictionModel>>
      optimization_target_prediction_model_map_;

  // The set of optimization targets that have been registered with the
  // prediction manager.
  base::flat_set<proto::OptimizationTarget> registered_optimization_targets_;

  // A map of host to host model features known to the prediction manager.
  //
  // TODO(crbug/1001194): When loading features for the map, the size should be
  // restricted.
  base::flat_map<std::string, base::flat_map<std::string, float>>
      host_model_features_map_;

  // The current session's FCP statistics for HTTP/HTTPS navigations.
  OptimizationGuideSessionStatistic session_fcp_;

  // A float representation of the time to FCP of the previous HTTP/HTTPS page
  // load. This is nullopt when no previous page load exists (the first page
  // load of a session).
  base::Optional<float> previous_load_fcp_ms_;

  // The current estimate of the EffectiveConnectionType.
  net::EffectiveConnectionType current_effective_connection_type_ =
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PredictionManager);
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MANAGER_H_
