// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/input/predictor_factory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/widget/input/prediction/empty_predictor.h"
#include "third_party/blink/renderer/platform/widget/input/prediction/kalman_predictor.h"
#include "third_party/blink/renderer/platform/widget/input/prediction/least_squares_predictor.h"
#include "third_party/blink/renderer/platform/widget/input/prediction/linear_predictor.h"
#include "third_party/blink/renderer/platform/widget/input/prediction/linear_resampling.h"

namespace blink {

namespace {
using input_prediction::PredictorType;
}

// Set to UINT_MAX to trigger querying feature flags.
unsigned int PredictorFactory::predictor_options_ = UINT_MAX;

PredictorType PredictorFactory::GetPredictorTypeFromName(
    const std::string& predictor_name) {
  if (predictor_name == blink::features::kScrollPredictorNameLinearResampling)
    return PredictorType::kScrollPredictorTypeLinearResampling;
  else if (predictor_name == blink::features::kScrollPredictorNameLsq)
    return PredictorType::kScrollPredictorTypeLsq;
  else if (predictor_name == blink::features::kScrollPredictorNameKalman)
    return PredictorType::kScrollPredictorTypeKalman;
  else if (predictor_name == blink::features::kScrollPredictorNameLinearFirst)
    return PredictorType::kScrollPredictorTypeLinearFirst;
  else if (predictor_name == blink::features::kScrollPredictorNameLinearSecond)
    return PredictorType::kScrollPredictorTypeLinearSecond;
  else
    return PredictorType::kScrollPredictorTypeEmpty;
}

std::unique_ptr<InputPredictor> PredictorFactory::GetPredictor(
    PredictorType predictor_type) {
  if (predictor_type == PredictorType::kScrollPredictorTypeLinearResampling) {
    return std::make_unique<LinearResampling>();
  } else if (predictor_type == PredictorType::kScrollPredictorTypeLsq) {
    return std::make_unique<LeastSquaresPredictor>();
  } else if (predictor_type == PredictorType::kScrollPredictorTypeKalman) {
    return std::make_unique<KalmanPredictor>(GetKalmanPredictorOptions());
  } else if (predictor_type == PredictorType::kScrollPredictorTypeLinearFirst) {
    return std::make_unique<LinearPredictor>(
        LinearPredictor::EquationOrder::kFirstOrder);
  } else if (predictor_type ==
             PredictorType::kScrollPredictorTypeLinearSecond) {
    return std::make_unique<LinearPredictor>(
        LinearPredictor::EquationOrder::kSecondOrder);
  } else {
    return std::make_unique<EmptyPredictor>();
  }
}

unsigned int PredictorFactory::GetKalmanPredictorOptions() {
  if (predictor_options_ == UINT_MAX) {
    predictor_options_ =
        (base::FeatureList::IsEnabled(blink::features::kKalmanHeuristics)
             ? KalmanPredictor::PredictionOptions::kHeuristicsEnabled
             : 0) |
        (base::FeatureList::IsEnabled(blink::features::kKalmanDirectionCutOff)
             ? KalmanPredictor::PredictionOptions::kDirectionCutOffEnabled
             : 0);
  }
  return predictor_options_;
}

}  // namespace blink
