// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/prediction/empty_predictor.h"

#include "third_party/blink/public/common/features.h"

namespace blink {

EmptyPredictor::EmptyPredictor() {
  Reset();
}

EmptyPredictor::~EmptyPredictor() = default;

const char* EmptyPredictor::GetName() const {
  return blink::features::kScrollPredictorNameEmpty;
}

void EmptyPredictor::Reset() {
  last_input_ = base::nullopt;
}

void EmptyPredictor::Update(const InputData& cur_input) {
  last_input_ = cur_input;
}

bool EmptyPredictor::HasPrediction() const {
  return last_input_.has_value();
}

std::unique_ptr<InputPredictor::InputData> EmptyPredictor::GeneratePrediction(
    base::TimeTicks predict_time) const {
  if (!HasPrediction())
    return nullptr;
  return std::make_unique<InputData>(last_input_.value());
}

base::TimeDelta EmptyPredictor::TimeInterval() const {
  return kTimeInterval;
}

}  // namespace blink
