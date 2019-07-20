// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/empty_predictor.h"
#include "ui/events/blink/prediction/predictor_factory.h"

namespace ui {

EmptyPredictor::EmptyPredictor() {
  Reset();
}

EmptyPredictor::~EmptyPredictor() = default;

const char* EmptyPredictor::GetName() const {
  return input_prediction::kScrollPredictorNameEmpty;
}

void EmptyPredictor::Reset() {
  last_input_ = base::nullopt;
}

void EmptyPredictor::Update(const InputData& cur_input) {
  last_input_ = cur_input;
}

bool EmptyPredictor::HasPrediction() const {
  return last_input_ != base::nullopt;
}

bool EmptyPredictor::GeneratePrediction(base::TimeTicks predict_time,
                                        InputData* result) const {
  if (!HasPrediction())
    return false;

  result->pos = last_input_.value().pos;
  return true;
}

}  // namespace ui
