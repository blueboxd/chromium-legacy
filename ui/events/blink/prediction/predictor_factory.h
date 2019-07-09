// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_PREDICTION_PREDICTOR_FACTORY_H_
#define UI_EVENTS_BLINK_PREDICTION_PREDICTOR_FACTORY_H_

#include "ui/events/blink/prediction/input_predictor.h"

namespace ui {

namespace input_prediction {

extern const char kScrollPredictorNameLsq[];
extern const char kScrollPredictorNameKalman[];
extern const char kScrollPredictorNameKalmanTimeFiltered[];
extern const char kScrollPredictorNameLinearFirst[];
extern const char kScrollPredictorNameLinearSecond[];
extern const char kScrollPredictorNameEmpty[];

enum class PredictorType {
  kScrollPredictorTypeLsq,
  kScrollPredictorTypeKalman,
  kScrollPredictorTypeKalmanTimeFiltered,
  kScrollPredictorTypeLinearFirst,
  kScrollPredictorTypeLinearSecond,
  kScrollPredictorTypeEmpty
};
}  // namespace input_prediction

class PredictorFactory {
 public:
  // Returns the PredictorType associated to the given predictor
  // name if found, otherwise returns kScrollPredictorTypeEmpty
  static input_prediction::PredictorType GetPredictorTypeFromName(
      const std::string& predictor_name);

  // Returns the predictor designed by its type if found, otherwise returns
  // PredictorEmpty
  static std::unique_ptr<InputPredictor> GetPredictor(
      input_prediction::PredictorType predictor_type);

 private:
  PredictorFactory() = delete;
  ~PredictorFactory() = delete;
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_PREDICTION_PREDICTOR_FACTORY_H_