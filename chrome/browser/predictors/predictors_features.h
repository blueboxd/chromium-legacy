// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PREDICTORS_FEATURES_H_
#define CHROME_BROWSER_PREDICTORS_PREDICTORS_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

extern const base::Feature kLoadingPredictorUseLocalPredictions;

extern const base::Feature kLoadingOnlyLearnHighPriorityResources;

extern const base::Feature kLoadingPredictorTableConfig;

extern const base::Feature kLoadingPreconnectToRedirectTarget;

extern const base::Feature kLoadingPredictorDisregardAlwaysAccessesNetwork;

extern const base::Feature kLoadingPredictorUseOptimizationGuide;

// Returns whether local predictions should be used to make preconnect
// predictions.
bool ShouldUseLocalPredictions();

// Returns whether optimization guide predictions should be used to make
// preconnect predictions.
//
// In addition to checking whether the feature is enabled, this will
// additionally check a feature parameter is specified to dictate if the
// predictions should be used to preconnect to subresource origins.
bool ShouldUseOptimizationGuidePredictionsToPreconnect();

}  // namespace features

#endif  // CHROME_BROWSER_PREDICTORS_PREDICTORS_FEATURES_H_
