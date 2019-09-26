// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_event_prediction.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/common/content_features.h"

using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebPointerEvent;
using blink::WebPointerProperties;
using blink::WebTouchEvent;

namespace content {

InputEventPrediction::InputEventPrediction(bool enable_resampling)
    : enable_resampling_(enable_resampling) {
  // When resampling is enabled, set predictor type by resampling flag params;
  // otherwise, get predictor type parameters from kInputPredictorTypeChoice
  // flag.
  std::string predictor_name =
      enable_resampling_
          ? GetFieldTrialParamValueByFeature(features::kResamplingInputEvents,
                                             "predictor")
          : GetFieldTrialParamValueByFeature(
                features::kInputPredictorTypeChoice, "predictor");

  if (predictor_name.empty())
    selected_predictor_type_ =
        ui::input_prediction::PredictorType::kScrollPredictorTypeKalman;
  else
    selected_predictor_type_ =
        ui::PredictorFactory::GetPredictorTypeFromName(predictor_name);

  mouse_predictor_ = CreatePredictor();
}

InputEventPrediction::~InputEventPrediction() {}

void InputEventPrediction::HandleEvents(
    blink::WebCoalescedInputEvent& coalesced_event,
    base::TimeTicks frame_time) {
  switch (coalesced_event.Event().GetType()) {
    case WebInputEvent::kMouseMove:
    case WebInputEvent::kTouchMove:
    case WebInputEvent::kPointerMove: {
      size_t coalesced_size = coalesced_event.CoalescedEventSize();
      for (size_t i = 0; i < coalesced_size; i++)
        ComputeAccuracy(coalesced_event.CoalescedEvent(i));

      for (size_t i = 0; i < coalesced_size; i++)
        UpdatePrediction(coalesced_event.CoalescedEvent(i));

      if (enable_resampling_)
        ApplyResampling(frame_time, coalesced_event.EventPointer());

      AddPredictedEvents(coalesced_event);
      break;
    }
    case WebInputEvent::kTouchScrollStarted:
    case WebInputEvent::kPointerCausedUaAction:
      pointer_id_predictor_map_.clear();
      break;
    default:
      ResetPredictor(coalesced_event.Event());
  }
}

std::unique_ptr<ui::InputPredictor> InputEventPrediction::CreatePredictor()
    const {
  return ui::PredictorFactory::GetPredictor(selected_predictor_type_);
}

void InputEventPrediction::UpdatePrediction(const WebInputEvent& event) {
  if (WebInputEvent::IsTouchEventType(event.GetType())) {
    DCHECK(event.GetType() == WebInputEvent::kTouchMove);
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    for (unsigned i = 0; i < touch_event.touches_length; ++i) {
      if (touch_event.touches[i].state == blink::WebTouchPoint::kStateMoved) {
        UpdateSinglePointer(touch_event.touches[i], touch_event.TimeStamp());
      }
    }
  } else if (WebInputEvent::IsMouseEventType(event.GetType())) {
    DCHECK(event.GetType() == WebInputEvent::kMouseMove);
    UpdateSinglePointer(static_cast<const WebMouseEvent&>(event),
                        event.TimeStamp());
  } else if (WebInputEvent::IsPointerEventType(event.GetType())) {
    DCHECK(event.GetType() == WebInputEvent::kPointerMove);
    UpdateSinglePointer(static_cast<const WebPointerEvent&>(event),
                        event.TimeStamp());
  }
  last_event_timestamp_ = event.TimeStamp();
}

// When resampling, we don't want to predict too far away because the result
// will likely be inaccurate in that case. We then cut off the prediction to
// the maximum available for the current predictor
void InputEventPrediction::ApplyResampling(base::TimeTicks frame_time,
                                           WebInputEvent* event) {
  base::TimeDelta prediction_delta = frame_time - event->TimeStamp();
  base::TimeTicks predict_time;
  WebPointerProperties* wpp_event;

  if (event->GetType() == WebInputEvent::kTouchMove) {
    WebTouchEvent* touch_event = static_cast<WebTouchEvent*>(event);
    for (unsigned i = 0; i < touch_event->touches_length; ++i) {
      if (touch_event->touches[i].state == blink::WebTouchPoint::kStateMoved) {
        wpp_event = &touch_event->touches[i];
        // Cutoff prediction if delta > MaxResampleTime
        auto predictor = pointer_id_predictor_map_.find(wpp_event->id);
        if (predictor != pointer_id_predictor_map_.end()) {
          prediction_delta =
              std::min(prediction_delta, predictor->second->MaxResampleTime());
          predict_time = event->TimeStamp() + prediction_delta;
          // Compute the prediction
          if (GetPointerPrediction(predict_time, wpp_event))
            event->SetTimeStamp(predict_time);
        }
      }
    }
  } else if (event->GetType() == WebInputEvent::kMouseMove) {
    wpp_event = static_cast<WebMouseEvent*>(event);
    // Cutoff prediction if delta > MaxResampleTime
    prediction_delta =
        std::min(prediction_delta, mouse_predictor_->MaxResampleTime());
    predict_time = event->TimeStamp() + prediction_delta;
    // Compute the prediction
    if (GetPointerPrediction(predict_time, wpp_event))
      event->SetTimeStamp(predict_time);
  } else if (event->GetType() == WebInputEvent::kPointerMove) {
    wpp_event = static_cast<WebPointerEvent*>(event);
    // Cutoff prediction if delta > MaxResampleTime
    auto predictor = pointer_id_predictor_map_.find(wpp_event->id);
    if (predictor != pointer_id_predictor_map_.end()) {
      prediction_delta =
          std::min(prediction_delta, predictor->second->MaxResampleTime());
      predict_time = event->TimeStamp() + prediction_delta;
      // Compute the prediction
      if (GetPointerPrediction(predict_time, wpp_event))
        event->SetTimeStamp(predict_time);
    }
  }
}

void InputEventPrediction::ResetPredictor(const WebInputEvent& event) {
  if (WebInputEvent::IsTouchEventType(event.GetType())) {
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    for (unsigned i = 0; i < touch_event.touches_length; ++i) {
      if (touch_event.touches[i].state != blink::WebTouchPoint::kStateMoved &&
          touch_event.touches[i].state !=
              blink::WebTouchPoint::kStateStationary)
        pointer_id_predictor_map_.erase(touch_event.touches[i].id);
    }
  } else if (WebInputEvent::IsMouseEventType(event.GetType())) {
    ResetSinglePredictor(static_cast<const WebMouseEvent&>(event));
  } else if (WebInputEvent::IsPointerEventType(event.GetType())) {
    ResetSinglePredictor(static_cast<const WebPointerEvent&>(event));
  }
}

void InputEventPrediction::AddPredictedEvents(
    blink::WebCoalescedInputEvent& coalesced_event) {
  base::TimeTicks predict_time = last_event_timestamp_;
  base::TimeTicks max_prediction_timestamp =
      last_event_timestamp_ + mouse_predictor_->MaxPredictionTime();
  bool success = true;
  while (success) {
    ui::WebScopedInputEvent predicted_event =
        ui::WebInputEventTraits::Clone(coalesced_event.Event());
    success = false;
    if (predicted_event->GetType() == WebInputEvent::kTouchMove) {
      WebTouchEvent& touch_event =
          static_cast<WebTouchEvent&>(*predicted_event);
      // Average all touch intervals
      base::TimeDelta touch_time_interval;
      for (unsigned i = 0; i < touch_event.touches_length; ++i) {
        touch_time_interval +=
            GetPredictionTimeInterval(&touch_event.touches[i]);
      }
      predict_time += touch_time_interval / touch_event.touches_length;
      if (predict_time <= max_prediction_timestamp) {
        for (unsigned i = 0; i < touch_event.touches_length; ++i) {
          if (touch_event.touches[i].state ==
              blink::WebTouchPoint::kStateMoved) {
            success =
                GetPointerPrediction(predict_time, &touch_event.touches[i]);
          }
        }
      }
    } else if (predicted_event->GetType() == WebInputEvent::kMouseMove) {
      WebMouseEvent& mouse_event =
          static_cast<WebMouseEvent&>(*predicted_event);
      predict_time += GetPredictionTimeInterval(&mouse_event);
      success = predict_time <= max_prediction_timestamp &&
                GetPointerPrediction(predict_time, &mouse_event);
    } else if (predicted_event->GetType() == WebInputEvent::kPointerMove) {
      WebPointerEvent& pointer_event =
          static_cast<WebPointerEvent&>(*predicted_event);
      predict_time += GetPredictionTimeInterval(&pointer_event);
      success = predict_time <= max_prediction_timestamp &&
                GetPointerPrediction(predict_time, &pointer_event);
    }
    if (success) {
      predicted_event->SetTimeStamp(predict_time);
      coalesced_event.AddPredictedEvent(*predicted_event);
    }
  }
}

base::TimeDelta InputEventPrediction::GetPredictionTimeInterval(
    WebPointerProperties* event) const {
  if (event->pointer_type != WebPointerProperties::PointerType::kMouse) {
    auto predictor = pointer_id_predictor_map_.find(event->id);
    if (predictor != pointer_id_predictor_map_.end()) {
      return predictor->second->TimeInterval();
    }
  }
  return mouse_predictor_->TimeInterval();
}

void InputEventPrediction::UpdateSinglePointer(
    const WebPointerProperties& event,
    base::TimeTicks event_time) {
  ui::InputPredictor::InputData data = {event.PositionInWidget(), event_time};
  if (event.pointer_type == WebPointerProperties::PointerType::kMouse)
    mouse_predictor_->Update(data);
  else {
    auto predictor = pointer_id_predictor_map_.find(event.id);
    if (predictor != pointer_id_predictor_map_.end()) {
      predictor->second->Update(data);
    } else {
      // Workaround for GLIBC C++ < 7.3 that fails to insert with braces
      // See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82522
      auto pair = std::make_pair(event.id, CreatePredictor());
      pointer_id_predictor_map_.insert(std::move(pair));
      pointer_id_predictor_map_[event.id]->Update(data);
    }
  }
}

bool InputEventPrediction::GetPointerPrediction(base::TimeTicks predict_time,
                                                WebPointerProperties* event) {
  ui::InputPredictor::InputData predict_result;
  if (event->pointer_type == WebPointerProperties::PointerType::kMouse) {
    if (mouse_predictor_->HasPrediction() &&
        mouse_predictor_->GeneratePrediction(predict_time, &predict_result)) {
      event->SetPositionInWidget(predict_result.pos);
      return true;
    }
  } else {
    // Reset mouse predictor if pointer type is touch or stylus
    mouse_predictor_->Reset();

    auto predictor = pointer_id_predictor_map_.find(event->id);
    if (predictor != pointer_id_predictor_map_.end() &&
        predictor->second->HasPrediction() &&
        predictor->second->GeneratePrediction(predict_time, &predict_result)) {
      event->SetPositionInWidget(predict_result.pos);
      return true;
    }
  }
  return false;
}

void InputEventPrediction::ResetSinglePredictor(
    const WebPointerProperties& event) {
  if (event.pointer_type == WebPointerProperties::PointerType::kMouse)
    mouse_predictor_->Reset();
  else
    pointer_id_predictor_map_.erase(event.id);
}

void InputEventPrediction::ComputeAccuracy(const WebInputEvent& event) const {
  base::TimeDelta time_delta = event.TimeStamp() - last_event_timestamp_;

  std::string suffix;
  if (time_delta < base::TimeDelta::FromMilliseconds(10))
    suffix = "Short";
  else if (time_delta < base::TimeDelta::FromMilliseconds(20))
    suffix = "Middle";
  else if (time_delta < base::TimeDelta::FromMilliseconds(35))
    suffix = "Long";
  else
    return;

  ui::InputPredictor::InputData predict_result;
  if (event.GetType() == WebInputEvent::kTouchMove) {
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    for (unsigned i = 0; i < touch_event.touches_length; ++i) {
      if (touch_event.touches[i].state == blink::WebTouchPoint::kStateMoved) {
        auto predictor =
            pointer_id_predictor_map_.find(touch_event.touches[i].id);
        if (predictor != pointer_id_predictor_map_.end() &&
            predictor->second->HasPrediction() &&
            predictor->second->GeneratePrediction(event.TimeStamp(),
                                                  &predict_result)) {
          float distance =
              (predict_result.pos -
               gfx::PointF(touch_event.touches[i].PositionInWidget()))
                  .Length();
          base::UmaHistogramCounts1000(
              "Event.InputEventPrediction.Accuracy.Touch." + suffix,
              static_cast<int>(distance));
        }
      }
    }
  } else if (event.GetType() == WebInputEvent::kMouseMove) {
    const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);
    if (mouse_predictor_->HasPrediction() &&
        mouse_predictor_->GeneratePrediction(event.TimeStamp(),
                                             &predict_result)) {
      float distance =
          (predict_result.pos - gfx::PointF(mouse_event.PositionInWidget()))
              .Length();
      base::UmaHistogramCounts1000(
          "Event.InputEventPrediction.Accuracy.Mouse." + suffix,
          static_cast<int>(distance));
    }
  }
}

}  // namespace content
