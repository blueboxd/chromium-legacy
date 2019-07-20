// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/kalman_predictor.h"
#include "ui/events/blink/prediction/predictor_factory.h"

namespace {

// Influence of acceleration during each prediction sample
constexpr float kAccelerationInfluence = 0.5f;
// Influence of velocity during each prediction sample
constexpr float kVelocityInfluence = 1.0f;

}  // namespace

namespace ui {

constexpr base::TimeDelta InputPredictor::kMaxTimeDelta;
constexpr base::TimeDelta InputPredictor::kMaxResampleTime;

KalmanPredictor::KalmanPredictor(const bool enable_time_filtering)
    : enable_time_filtering_(enable_time_filtering) {}

KalmanPredictor::~KalmanPredictor() = default;

const char* KalmanPredictor::GetName() const {
  return enable_time_filtering_
             ? input_prediction::kScrollPredictorNameKalmanTimeFiltered
             : input_prediction::kScrollPredictorNameKalman;
}

void KalmanPredictor::Reset() {
  x_predictor_.Reset();
  y_predictor_.Reset();
  last_point_.time_stamp = base::TimeTicks();
  time_filter_.Reset();
}

void KalmanPredictor::Update(const InputData& cur_input) {
  base::TimeDelta dt;
  if (!last_point_.time_stamp.is_null()) {
    // When last point is kMaxTimeDelta away, consider it is incontinuous.
    dt = cur_input.time_stamp - last_point_.time_stamp;
    if (dt > kMaxTimeDelta)
      Reset();
    else if (enable_time_filtering_)
      time_filter_.Update(dt.InMillisecondsF(), 0);
  }

  double dt_ms = enable_time_filtering_ ? time_filter_.GetPosition()
                                        : dt.InMillisecondsF();
  last_point_ = cur_input;
  x_predictor_.Update(cur_input.pos.x(), dt_ms);
  y_predictor_.Update(cur_input.pos.y(), dt_ms);
}

bool KalmanPredictor::HasPrediction() const {
  return x_predictor_.Stable() && y_predictor_.Stable();
}

bool KalmanPredictor::GeneratePrediction(base::TimeTicks predict_time,
                                         InputData* result) const {
  if (!HasPrediction())
    return false;

  float pred_dt = (predict_time - last_point_.time_stamp).InMillisecondsF();

  std::vector<InputData> pred_points;
  gfx::Vector2dF position(last_point_.pos.x(), last_point_.pos.y());
  // gfx::Vector2dF position = PredictPosition();
  gfx::Vector2dF velocity = PredictVelocity();
  gfx::Vector2dF acceleration = PredictAcceleration();

  position +=
      ScaleVector2d(velocity, kVelocityInfluence * pred_dt) +
      ScaleVector2d(acceleration, kAccelerationInfluence * pred_dt * pred_dt);

  result->pos.set_x(position.x());
  result->pos.set_y(position.y());
  return true;
}

gfx::Vector2dF KalmanPredictor::PredictPosition() const {
  return gfx::Vector2dF(x_predictor_.GetPosition(), y_predictor_.GetPosition());
}

gfx::Vector2dF KalmanPredictor::PredictVelocity() const {
  return gfx::Vector2dF(x_predictor_.GetVelocity(), y_predictor_.GetVelocity());
}

gfx::Vector2dF KalmanPredictor::PredictAcceleration() const {
  return gfx::Vector2dF(x_predictor_.GetAcceleration(),
                        y_predictor_.GetAcceleration());
}

}  // namespace ui
