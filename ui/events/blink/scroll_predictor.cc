// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/scroll_predictor.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/blink/prediction/filter_factory.h"
#include "ui/events/blink/prediction/predictor_factory.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace ui {

ScrollPredictor::ScrollPredictor() {
  // Get the predictor from feature flags
  std::string predictor_name = GetFieldTrialParamValueByFeature(
      features::kResamplingScrollEvents, "predictor");

  input_prediction::PredictorType predictor_type =
      ui::PredictorFactory::GetPredictorTypeFromName(predictor_name);
  predictor_ = ui::PredictorFactory::GetPredictor(predictor_type);

  filtering_enabled_ =
      base::FeatureList::IsEnabled(features::kFilteringScrollPrediction);

  if (filtering_enabled_) {
    // Get the filter from feature flags
    std::string filter_name = GetFieldTrialParamValueByFeature(
        features::kFilteringScrollPrediction, "filter");

    input_prediction::FilterType filter_type =
        ui::FilterFactory::GetFilterTypeFromName(filter_name);
    filter_ = ui::FilterFactory::CreateFilter(filter_type);
  }
}

ScrollPredictor::~ScrollPredictor() = default;

void ScrollPredictor::ResetOnGestureScrollBegin(const WebGestureEvent& event) {
  DCHECK(event.GetType() == WebInputEvent::kGestureScrollBegin);
  // Only do resampling for scroll on touchscreen.
  if (event.SourceDevice() == blink::WebGestureDevice::kTouchscreen) {
    should_resample_scroll_events_ = true;
    Reset();
  }
}

std::unique_ptr<EventWithCallback> ScrollPredictor::ResampleScrollEvents(
    std::unique_ptr<EventWithCallback> event_with_callback,
    base::TimeTicks frame_time) {
  if (!should_resample_scroll_events_)
    return event_with_callback;

  const EventWithCallback::OriginalEventList& original_events =
      event_with_callback->original_events();

  if (event_with_callback->event().GetType() ==
      WebInputEvent::kGestureScrollUpdate) {
    // TODO(eirage): When scroll events are coalesced with pinch, we can have
    // empty original event list. In that case, we can't use the original events
    // to update the prediction. We don't want to use the aggregated event to
    // update because of the event time stamp, so skip the prediction for now.
    if (original_events.empty())
      return event_with_callback;

    TRACE_EVENT_BEGIN0("input", "ScrollPredictor::ResampleScrollEvents");

    temporary_accumulated_delta_ = current_accumulated_delta_;
    for (auto& coalesced_event : original_events)
      ComputeAccuracy(coalesced_event.event_);

    for (auto& coalesced_event : original_events)
      UpdatePrediction(coalesced_event.event_, frame_time);

    if (should_resample_scroll_events_)
      ResampleEvent(frame_time, event_with_callback->event_pointer(),
                    event_with_callback->mutable_latency_info());

    TRACE_EVENT_END2("input", "ScrollPredictor::ResampleScrollEvents",
                     "OriginalPosition", current_accumulated_delta_.ToString(),
                     "PredictedPosition", last_accumulated_delta_.ToString());
  } else if (event_with_callback->event().GetType() ==
             WebInputEvent::kGestureScrollEnd) {
    should_resample_scroll_events_ = false;
  }

  return event_with_callback;
}

void ScrollPredictor::Reset() {
  predictor_->Reset();
  if (filtering_enabled_)
    filter_->Reset();
  current_accumulated_delta_ = gfx::PointF();
  last_accumulated_delta_ = gfx::PointF();
}

void ScrollPredictor::UpdatePrediction(const WebScopedInputEvent& event,
                                       base::TimeTicks frame_time) {
  DCHECK(event->GetType() == WebInputEvent::kGestureScrollUpdate);
  const WebGestureEvent& gesture_event =
      static_cast<const WebGestureEvent&>(*event);
  // When fling, GSU is sending per frame, resampling is not needed.
  if (gesture_event.data.scroll_update.inertial_phase ==
      WebGestureEvent::InertialPhaseState::kMomentum) {
    should_resample_scroll_events_ = false;
    return;
  }

  current_accumulated_delta_.Offset(gesture_event.data.scroll_update.delta_x,
                                    gesture_event.data.scroll_update.delta_y);
  InputPredictor::InputData data = {current_accumulated_delta_,
                                    gesture_event.TimeStamp()};

  predictor_->Update(data);
  last_event_timestamp_ = gesture_event.TimeStamp();
}

void ScrollPredictor::ResampleEvent(base::TimeTicks time_stamp,
                                    WebInputEvent* event,
                                    LatencyInfo* latency_info) {
  DCHECK(event->GetType() == WebInputEvent::kGestureScrollUpdate);
  WebGestureEvent* gesture_event = static_cast<WebGestureEvent*>(event);

  gfx::PointF predicted_accumulated_delta = current_accumulated_delta_;
  InputPredictor::InputData result;

  base::TimeDelta prediction_delta = time_stamp - gesture_event->TimeStamp();
  bool predicted = false;
  // Disable prediction when dt < 0.
  if (prediction_delta > base::TimeDelta()) {
    // For resampling, we don't want to predict too far away because the result
    // will likely be inaccurate in that case. We cut off the prediction to the
    // maximum available for the current predictor
    prediction_delta =
        std::min(prediction_delta, predictor_->MaxResampleTime());

    // Compute the prediction timestamp
    base::TimeTicks prediction_time =
        gesture_event->TimeStamp() + prediction_delta;

    if (predictor_->HasPrediction() &&
        predictor_->GeneratePrediction(prediction_time, &result)) {
      predicted_accumulated_delta = result.pos;
      gesture_event->SetTimeStamp(prediction_time);
      predicted = true;
    }
  }

  // Feed the filter with the first non-predicted events but only apply
  // filtering on predicted events
  gfx::PointF filtered_pos = predicted_accumulated_delta;
  if (filtering_enabled_ && filter_->Filter(time_stamp, &filtered_pos) &&
      predicted)
    predicted_accumulated_delta = filtered_pos;

  // If the last resampled GSU over predict the delta, new GSU might try to
  // scroll back to make up the difference, which cause the scroll to jump back.
  // So we set the new delta to 0 when predicted delta is in different direction
  // to the original event.
  gfx::Vector2dF new_delta =
      predicted_accumulated_delta - last_accumulated_delta_;
  gesture_event->data.scroll_update.delta_x =
      (new_delta.x() * gesture_event->data.scroll_update.delta_x < 0)
          ? 0
          : new_delta.x();
  gesture_event->data.scroll_update.delta_y =
      (new_delta.y() * gesture_event->data.scroll_update.delta_y < 0)
          ? 0
          : new_delta.y();
  // Sync the predicted delta_y to latency_info for AverageLag metric.
  latency_info->set_predicted_scroll_update_delta(new_delta.y());

  last_accumulated_delta_.Offset(gesture_event->data.scroll_update.delta_x,
                                 gesture_event->data.scroll_update.delta_y);
}

void ScrollPredictor::ComputeAccuracy(const WebScopedInputEvent& event) {
  const WebGestureEvent& gesture_event =
      static_cast<const WebGestureEvent&>(*event);

  base::TimeDelta time_delta = event->TimeStamp() - last_event_timestamp_;
  std::string suffix;
  if (time_delta < base::TimeDelta::FromMilliseconds(10))
    suffix = "Short";
  else if (time_delta < base::TimeDelta::FromMilliseconds(20))
    suffix = "Middle";
  else if (time_delta < base::TimeDelta::FromMilliseconds(35))
    suffix = "Long";
  else
    return;

  InputPredictor::InputData predict_result;
  temporary_accumulated_delta_.Offset(gesture_event.data.scroll_update.delta_x,
                                      gesture_event.data.scroll_update.delta_y);
  if (predictor_->HasPrediction() &&
      predictor_->GeneratePrediction(event->TimeStamp(), &predict_result)) {
    float distance =
        (predict_result.pos - gfx::PointF(temporary_accumulated_delta_))
            .Length();
    base::UmaHistogramCounts1000(
        "Event.InputEventPrediction.Accuracy.Scroll." + suffix,
        static_cast<int>(distance));

    // If the distance from predicted position to actual position is in same
    // direction as the delta_y, the result is under predicted, otherwise over
    // predict.
    float dist_y = temporary_accumulated_delta_.y() - predict_result.pos.y();
    if (gesture_event.data.scroll_update.delta_y * dist_y < 0) {
      base::UmaHistogramCounts1000(
          "Event.InputEventPrediction.Accuracy.Scroll.OverPredict." + suffix,
          static_cast<int>(std::abs(dist_y)));
    } else {
      base::UmaHistogramCounts1000(
          "Event.InputEventPrediction.Accuracy.Scroll.UnderPredict." + suffix,
          static_cast<int>(std::abs(dist_y)));
    }
  }
}

}  // namespace ui
