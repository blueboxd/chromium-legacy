// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/predictors_features.h"

#include "base/metrics/field_trial_params.h"

namespace features {

// Whether local predictions should be used to make preconnect predictions.
const base::Feature kLoadingPredictorUseLocalPredictions{
    "LoadingPredictorUseLocalPredictions", base::FEATURE_ENABLED_BY_DEFAULT};

// Modifies loading predictor so that it only learns about subresources and
// origins that are high priority.
const base::Feature kLoadingOnlyLearnHighPriorityResources{
    "LoadingOnlyLearnHighPriorityResources", base::FEATURE_ENABLED_BY_DEFAULT};

// Configures the loading predictor table size and other base parameters.
const base::Feature kLoadingPredictorTableConfig{
    "LoadingPredictorTableConfig", base::FEATURE_DISABLED_BY_DEFAULT};

// Modifies loading predictor so that the predictions also contain origins of
// the redirect target of the navigation.
const base::Feature kLoadingPreconnectToRedirectTarget{
    "LoadingPreconnectToRedirectTarget", base::FEATURE_DISABLED_BY_DEFAULT};

// Modifies loading predictor so that the value of the |always_access_network|
// attribute is not used when computing the predicting score for an origin.
const base::Feature kLoadingPredictorDisregardAlwaysAccessesNetwork{
    "LoadingPredictorDisregardAlwaysAccessesNetwork",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Modifies loading predictor so that it can also use predictions coming from
// the optimization guide.
const base::Feature kLoadingPredictorUseOptimizationGuide{
    "LoadingPredictorUseOptimizationGuide", base::FEATURE_DISABLED_BY_DEFAULT};

bool ShouldUseLocalPredictions() {
  return base::FeatureList::IsEnabled(kLoadingPredictorUseLocalPredictions);
}

bool ShouldUseOptimizationGuidePredictionsToPreconnect() {
  if (!base::FeatureList::IsEnabled(kLoadingPredictorUseOptimizationGuide))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      kLoadingPredictorUseOptimizationGuide, "use_predictions_for_preconnect",
      true);
}

}  // namespace features
