// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/predictor_factory.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/prediction/empty_predictor.h"
#include "ui/events/blink/prediction/kalman_predictor.h"
#include "ui/events/blink/prediction/least_squares_predictor.h"
#include "ui/events/blink/prediction/linear_predictor.h"

namespace ui {

namespace input_prediction {

const char kScrollPredictorNameLsq[] = "lsq";
const char kScrollPredictorNameKalman[] = "kalman";
const char kScrollPredictorNameKalmanTimeFiltered[] = "kalman_time_filtered";
const char kScrollPredictorNameLinearFirst[] = "linear_first";
const char kScrollPredictorNameLinearSecond[] = "linear_second";
const char kScrollPredictorNameEmpty[] = "empty";

}  // namespace input_prediction

namespace {
using input_prediction::PredictorType;
}

PredictorType PredictorFactory::GetPredictorTypeFromName(
    const std::string& predictor_name) {
  if (predictor_name == input_prediction::kScrollPredictorNameLsq)
    return PredictorType::kScrollPredictorTypeLsq;
  else if (predictor_name == input_prediction::kScrollPredictorNameKalman)
    return PredictorType::kScrollPredictorTypeKalman;
  else if (predictor_name ==
           input_prediction::kScrollPredictorNameKalmanTimeFiltered)
    return PredictorType::kScrollPredictorTypeKalmanTimeFiltered;
  else if (predictor_name == input_prediction::kScrollPredictorNameLinearFirst)
    return PredictorType::kScrollPredictorTypeLinearFirst;
  else if (predictor_name == input_prediction::kScrollPredictorNameLinearSecond)
    return PredictorType::kScrollPredictorTypeLinearSecond;
  else
    return PredictorType::kScrollPredictorTypeEmpty;
}

std::unique_ptr<InputPredictor> PredictorFactory::GetPredictor(
    PredictorType predictor_type) {
  if (predictor_type == PredictorType::kScrollPredictorTypeLsq)
    return std::make_unique<LeastSquaresPredictor>();
  else if (predictor_type == PredictorType::kScrollPredictorTypeKalman)
    return std::make_unique<KalmanPredictor>(false /* enable_time_filtering */);
  else if (predictor_type ==
           PredictorType::kScrollPredictorTypeKalmanTimeFiltered)
    return std::make_unique<KalmanPredictor>(true /* enable_time_filtering */);
  else if (predictor_type == PredictorType::kScrollPredictorTypeLinearFirst)
    return std::make_unique<LinearPredictor>(
        LinearPredictor::EquationOrder::kFirstOrder);
  else if (predictor_type == PredictorType::kScrollPredictorTypeLinearSecond)
    return std::make_unique<LinearPredictor>(
        LinearPredictor::EquationOrder::kSecondOrder);
  else
    return std::make_unique<EmptyPredictor>();
}

}  // namespace ui
