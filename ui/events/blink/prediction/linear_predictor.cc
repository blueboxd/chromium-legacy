// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/linear_predictor.h"
#include "ui/events/blink/prediction/predictor_factory.h"

namespace ui {

LinearPredictor::LinearPredictor(EquationOrder order) {
  equation_order_ = order;
}

LinearPredictor::~LinearPredictor() {}

const char* LinearPredictor::GetName() const {
  return equation_order_ == EquationOrder::kFirstOrder
             ? input_prediction::kScrollPredictorNameLinearFirst
             : input_prediction::kScrollPredictorNameLinearSecond;
}

void LinearPredictor::Reset() {
  events_queue_.clear();
}

size_t LinearPredictor::NumberOfEventsNeeded() {
  return static_cast<size_t>(equation_order_);
}

void LinearPredictor::Update(const InputData& new_input) {
  // The last input received is at least kMaxDeltaTime away, we consider it
  // is a new trajectory
  if (!events_queue_.empty() &&
      new_input.time_stamp - events_queue_.back().time_stamp > kMaxTimeDelta) {
    Reset();
  }

  // Queue the new event and keep only the number of last events needed
  events_queue_.push_back(new_input);
  if (events_queue_.size() > static_cast<size_t>(equation_order_)) {
    events_queue_.pop_front();
  }

  // Compute the current velocity
  if (events_queue_.size() >= static_cast<size_t>(EquationOrder::kFirstOrder)) {
    // Even if cur_velocity is empty the first time, last_velocity is only
    // used when 3 events are in the queue, so it will be initialized
    last_velocity_.set_x(cur_velocity_.x());
    last_velocity_.set_y(cur_velocity_.y());

    // We have at least 2 events to compute the current velocity
    // Get delta time between the last 2 events
    // Note: this delta time is also used to compute the acceleration term
    events_dt_ = (events_queue_.at(events_queue_.size() - 1).time_stamp -
                  events_queue_.at(events_queue_.size() - 2).time_stamp)
                     .InMillisecondsF();

    // Get delta pos between the last 2 events
    gfx::Vector2dF delta_pos = events_queue_.at(events_queue_.size() - 1).pos -
                               events_queue_.at(events_queue_.size() - 2).pos;

    // Get the velocity
    cur_velocity_.set_x(ScaleVector2d(delta_pos, 1.0 / events_dt_).x());
    cur_velocity_.set_y(ScaleVector2d(delta_pos, 1.0 / events_dt_).y());
  }
}

bool LinearPredictor::HasPrediction() const {
  // Even if the current equation is second order, we still can predict a
  // first order
  return events_queue_.size() >=
         static_cast<size_t>(EquationOrder::kFirstOrder);
}

bool LinearPredictor::GeneratePrediction(base::TimeTicks predict_time,
                                         bool is_resampling,
                                         InputData* result) const {
  if (!HasPrediction())
    return false;

  float pred_dt =
      (predict_time - events_queue_.back().time_stamp).InMillisecondsF();

  // For resampling, we don't want to predict too far away,
  // We then take the maximum of prediction time
  if (is_resampling)
    pred_dt = std::fmax(pred_dt, kMaxResampleTime.InMillisecondsF());

  // Compute the prediction
  // Note : a first order prediction is computed when only 2 events are
  // available in the second order predictor
  GeneratePredictionFirstOrder(pred_dt, result);
  if (equation_order_ == EquationOrder::kSecondOrder &&
      events_queue_.size() == static_cast<size_t>(EquationOrder::kSecondOrder))
    // Add the acceleration term to the current result
    GeneratePredictionSecondOrder(pred_dt, result);

  return true;
}

void LinearPredictor::GeneratePredictionFirstOrder(float pred_dt,
                                                   InputData* result) const {
  result->pos =
      events_queue_.back().pos + ScaleVector2d(cur_velocity_, pred_dt);
}

void LinearPredictor::GeneratePredictionSecondOrder(float pred_dt,
                                                    InputData* result) const {
  DCHECK(equation_order_ == EquationOrder::kSecondOrder);

  // Compute the acceleration between the last two velocities
  gfx::Vector2dF acceleration =
      ScaleVector2d(cur_velocity_ - last_velocity_, 1.0 / events_dt_);
  // Update the prediction
  result->pos =
      result->pos + ScaleVector2d(acceleration, 0.5 * pred_dt * pred_dt);
}

}  // namespace ui