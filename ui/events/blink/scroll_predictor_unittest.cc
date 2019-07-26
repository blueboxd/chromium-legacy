// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/scroll_predictor.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/prediction/empty_filter.h"
#include "ui/events/blink/prediction/empty_predictor.h"
#include "ui/events/blink/prediction/filter_factory.h"
#include "ui/events/blink/prediction/kalman_predictor.h"
#include "ui/events/blink/prediction/least_squares_predictor.h"
#include "ui/events/blink/prediction/linear_predictor.h"
#include "ui/events/blink/prediction/predictor_factory.h"

namespace ui {
namespace test {
namespace {

using blink::WebGestureEvent;
using blink::WebInputEvent;

constexpr double kEpsilon = 0.001;

}  // namespace

class ScrollPredictorTest : public testing::Test {
 public:
  ScrollPredictorTest() {}

  void SetUp() override {
    original_events_.clear();
    scroll_predictor_ = std::make_unique<ScrollPredictor>();
    scroll_predictor_->predictor_ = std::make_unique<EmptyPredictor>();
  }

  void SetUpLSQPredictor() {
    scroll_predictor_->predictor_ = std::make_unique<LeastSquaresPredictor>();
  }

  WebScopedInputEvent CreateGestureScrollUpdate(
      float delta_x = 0,
      float delta_y = 0,
      double time_delta_in_milliseconds = 0,
      WebGestureEvent::InertialPhaseState phase =
          WebGestureEvent::InertialPhaseState::kNonMomentum) {
    WebGestureEvent gesture(
        WebInputEvent::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests() +
            base::TimeDelta::FromMillisecondsD(time_delta_in_milliseconds),
        blink::WebGestureDevice::kTouchscreen);
    gesture.data.scroll_update.delta_x = delta_x;
    gesture.data.scroll_update.delta_y = delta_y;
    gesture.data.scroll_update.inertial_phase = phase;

    original_events_.emplace_back(WebInputEventTraits::Clone(gesture),
                                  LatencyInfo(), base::NullCallback());

    return WebInputEventTraits::Clone(gesture);
  }

  void CoalesceWith(const WebScopedInputEvent& new_event,
                    WebScopedInputEvent& old_event) {
    Coalesce(*new_event, old_event.get());
  }

  void SendGestureScrollBegin() {
    WebGestureEvent gesture_begin(WebInputEvent::kGestureScrollBegin,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::GetStaticTimeStampForTests(),
                                  blink::WebGestureDevice::kTouchscreen);
    scroll_predictor_->ResetOnGestureScrollBegin(gesture_begin);
  }

  void HandleResampleScrollEvents(WebScopedInputEvent& event,
                                  double time_delta_in_milliseconds = 0) {
    std::unique_ptr<EventWithCallback> event_with_callback =
        std::make_unique<EventWithCallback>(std::move(event), LatencyInfo(),
                                            base::TimeTicks(),
                                            base::NullCallback());
    event_with_callback->original_events() = std::move(original_events_);

    event_with_callback = scroll_predictor_->ResampleScrollEvents(
        std::move(event_with_callback),
        WebInputEvent::GetStaticTimeStampForTests() +
            base::TimeDelta::FromMillisecondsD(time_delta_in_milliseconds));

    event = WebInputEventTraits::Clone(event_with_callback->event());
  }

  bool PredictionAvailable(ui::InputPredictor::InputData* result,
                           double time_delta_in_milliseconds = 0) {
    return scroll_predictor_->predictor_->GeneratePrediction(
        WebInputEvent::GetStaticTimeStampForTests() +
            base::TimeDelta::FromMillisecondsD(time_delta_in_milliseconds),
        result);
  }

  gfx::PointF GetLastAccumulatedDelta() {
    return scroll_predictor_->last_accumulated_delta_;
  }

  bool GetResamplingState() {
    return scroll_predictor_->should_resample_scroll_events_;
  }

  bool isFilteringEnabled() { return scroll_predictor_->filtering_enabled_; }

  void ConfigurePredictorFieldTrial(const base::Feature& feature,
                                    const std::string& predictor_type) {
    base::FieldTrialParams params;
    params["predictor"] = predictor_type;

    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(feature, params);
    EXPECT_EQ(params["predictor"],
              GetFieldTrialParamValueByFeature(feature, "predictor"));
  }

  void VerifyPredictorType(const char* expected_type) {
    EXPECT_EQ(expected_type, scroll_predictor_->predictor_->GetName());
  }

  void ConfigureFilterFieldTrial(const base::Feature& feature,
                                 const std::string& filter_name) {
    base::FieldTrialParams params;
    params["filter"] = filter_name;

    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(feature, params);
    EXPECT_EQ(params["filter"],
              GetFieldTrialParamValueByFeature(feature, "filter"));
  }

  void VerifyFilterType(const char* expected_type) {
    EXPECT_EQ(expected_type, scroll_predictor_->filter_->GetName());
  }

 protected:
  EventWithCallback::OriginalEventList original_events_;
  std::unique_ptr<ScrollPredictor> scroll_predictor_;

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ScrollPredictorTest);
};

TEST_F(ScrollPredictorTest, ScrollResamplingStates) {
  // initial
  EXPECT_FALSE(GetResamplingState());

  // after GSB
  SendGestureScrollBegin();
  EXPECT_TRUE(GetResamplingState());

  // after GSU with no phase
  WebScopedInputEvent gesture_update =
      CreateGestureScrollUpdate(0, 10, 10 /* ms */);
  HandleResampleScrollEvents(gesture_update, 15 /* ms */);
  EXPECT_TRUE(GetResamplingState());

  // after GSU with momentum phase
  gesture_update = CreateGestureScrollUpdate(
      0, 10, 10 /* ms */, WebGestureEvent::InertialPhaseState::kMomentum);
  HandleResampleScrollEvents(gesture_update, 15 /* ms */);
  EXPECT_FALSE(GetResamplingState());

  // after GSE
  WebGestureEvent gesture_end(WebInputEvent::kGestureScrollEnd,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests(),
                              blink::WebGestureDevice::kTouchscreen);
  WebScopedInputEvent event = WebInputEventTraits::Clone(gesture_end);
  HandleResampleScrollEvents(event);
  EXPECT_FALSE(GetResamplingState());
}

TEST_F(ScrollPredictorTest, ResampleGestureScrollEvents) {
  SendGestureScrollBegin();
  ui::InputPredictor::InputData result;
  EXPECT_FALSE(PredictionAvailable(&result));

  WebScopedInputEvent gesture_update = CreateGestureScrollUpdate(0, -20);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-20,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);

  // Aggregated event delta doesn't change with empty predictor applied.
  gesture_update = CreateGestureScrollUpdate(0, -20);
  CoalesceWith(CreateGestureScrollUpdate(0, -40), gesture_update);
  EXPECT_EQ(-60,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-60,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);

  // Cumulative amount of scroll from the GSB is stored in the empty predictor.
  EXPECT_TRUE(PredictionAvailable(&result));
  EXPECT_EQ(-80, result.pos.y());

  // Send another GSB, Prediction will be reset.
  SendGestureScrollBegin();
  EXPECT_FALSE(PredictionAvailable(&result));

  // Sent another GSU.
  gesture_update = CreateGestureScrollUpdate(0, -35);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-35,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);
  // Total amount of scroll is track from the last GSB.
  EXPECT_TRUE(PredictionAvailable(&result));
  EXPECT_EQ(-35, result.pos.y());
}

TEST_F(ScrollPredictorTest, ScrollInDifferentDirection) {
  SendGestureScrollBegin();
  ui::InputPredictor::InputData result;

  // Scroll down.
  WebScopedInputEvent gesture_update = CreateGestureScrollUpdate(0, -20);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-20,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);
  EXPECT_TRUE(PredictionAvailable(&result));
  EXPECT_EQ(-20, result.pos.y());

  // Scroll up.
  gesture_update = CreateGestureScrollUpdate(0, 25);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(0, static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                   ->data.scroll_update.delta_x);
  EXPECT_EQ(25, static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                    ->data.scroll_update.delta_y);
  EXPECT_TRUE(PredictionAvailable(&result));
  EXPECT_EQ(0, result.pos.x());
  EXPECT_EQ(5, result.pos.y());

  // Scroll left + right.
  gesture_update = CreateGestureScrollUpdate(-35, 0);
  CoalesceWith(CreateGestureScrollUpdate(60, 0), gesture_update);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(25, static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                    ->data.scroll_update.delta_x);
  EXPECT_EQ(0, static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                   ->data.scroll_update.delta_y);
  EXPECT_TRUE(PredictionAvailable(&result));
  EXPECT_EQ(25, result.pos.x());
  EXPECT_EQ(5, result.pos.y());
}

TEST_F(ScrollPredictorTest, ScrollUpdateWithEmptyOriginalEventList) {
  SendGestureScrollBegin();

  // Send a GSU with empty original event list.
  WebScopedInputEvent gesture_update = CreateGestureScrollUpdate(0, -20);
  original_events_.clear();
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-20,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);

  // No prediction available because the event is skipped.
  ui::InputPredictor::InputData result;
  EXPECT_FALSE(PredictionAvailable(&result));

  // Send a GSU with original event.
  gesture_update = CreateGestureScrollUpdate(0, -30);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-30,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);

  // Send another GSU with empty original event list.
  gesture_update = CreateGestureScrollUpdate(0, -40);
  original_events_.clear();
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-40,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);

  // Prediction only track GSU with original event list.
  EXPECT_TRUE(PredictionAvailable(&result));
  EXPECT_EQ(-30, result.pos.y());
}

TEST_F(ScrollPredictorTest, LSQPredictorTest) {
  SetUpLSQPredictor();
  SendGestureScrollBegin();

  ui::InputPredictor::InputData result;

  // Send 1st GSU, no prediction available.
  WebScopedInputEvent gesture_update =
      CreateGestureScrollUpdate(0, -30, 8 /* ms */);
  HandleResampleScrollEvents(gesture_update, 16 /* ms */);
  EXPECT_FALSE(PredictionAvailable(&result, 16 /* ms */));
  EXPECT_EQ(-30,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);
  EXPECT_EQ(WebInputEvent::GetStaticTimeStampForTests() +
                base::TimeDelta::FromMillisecondsD(8 /* ms */),
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->TimeStamp());

  // Send 2nd GSU, no prediction available, event aligned at original timestamp.
  gesture_update = CreateGestureScrollUpdate(0, -30, 16 /* ms */);
  HandleResampleScrollEvents(gesture_update, 24 /* ms */);
  EXPECT_EQ(-30,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);
  EXPECT_EQ(WebInputEvent::GetStaticTimeStampForTests() +
                base::TimeDelta::FromMillisecondsD(16 /* ms */),
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->TimeStamp());
  EXPECT_FALSE(PredictionAvailable(&result, 24 /* ms */));

  // Send 3rd and 4th GSU, prediction result returns the sum of delta_y, event
  // aligned at frame time.
  gesture_update = CreateGestureScrollUpdate(0, -30, 24 /* ms */);
  HandleResampleScrollEvents(gesture_update, 32 /* ms */);
  EXPECT_EQ(-60,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);
  EXPECT_EQ(WebInputEvent::GetStaticTimeStampForTests() +
                base::TimeDelta::FromMillisecondsD(32 /* ms */),
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->TimeStamp());
  EXPECT_TRUE(PredictionAvailable(&result, 32 /* ms */));
  EXPECT_EQ(-120, result.pos.y());

  gesture_update = CreateGestureScrollUpdate(0, -30, 32 /* ms */);
  HandleResampleScrollEvents(gesture_update, 40 /* ms */);
  EXPECT_EQ(-30,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);
  EXPECT_EQ(WebInputEvent::GetStaticTimeStampForTests() +
                base::TimeDelta::FromMillisecondsD(40 /* ms */),
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->TimeStamp());
  EXPECT_TRUE(PredictionAvailable(&result, 40 /* ms */));
  EXPECT_EQ(-150, result.pos.y());
}

TEST_F(ScrollPredictorTest, ScrollPredictorNotChangeScrollDirection) {
  SetUpLSQPredictor();
  SendGestureScrollBegin();

  // Send 4 GSUs with delta_y = 10
  WebScopedInputEvent gesture_update =
      CreateGestureScrollUpdate(0, 10, 10 /* ms */);
  HandleResampleScrollEvents(gesture_update, 15 /* ms */);
  gesture_update = CreateGestureScrollUpdate(0, 10, 20 /* ms */);
  HandleResampleScrollEvents(gesture_update, 25 /* ms */);
  gesture_update = CreateGestureScrollUpdate(0, 10, 30 /* ms */);
  HandleResampleScrollEvents(gesture_update, 35 /* ms */);
  gesture_update = CreateGestureScrollUpdate(0, 10, 40 /* ms */);
  HandleResampleScrollEvents(gesture_update, 45 /* ms */);
  EXPECT_NEAR(10,
              static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                  ->data.scroll_update.delta_y,
              kEpsilon);
  EXPECT_NEAR(45, GetLastAccumulatedDelta().y(), kEpsilon);

  // Send a GSU with delta_y = 2. So last resampled GSU we calculated is
  // overhead. No scroll back in this case.
  gesture_update = CreateGestureScrollUpdate(0, 2, 50 /* ms */);
  HandleResampleScrollEvents(gesture_update, 55 /* ms */);
  EXPECT_EQ(0, static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                   ->data.scroll_update.delta_y);
  EXPECT_NEAR(45, GetLastAccumulatedDelta().y(), kEpsilon);

  // Send a GSU with different scroll direction. Resampled GSU is in the new
  // direction.
  gesture_update = CreateGestureScrollUpdate(0, -6, 60 /* ms */);
  HandleResampleScrollEvents(gesture_update, 60 /* ms */);
  EXPECT_NEAR(-9,
              static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                  ->data.scroll_update.delta_y,
              kEpsilon);
  EXPECT_NEAR(36, GetLastAccumulatedDelta().y(), kEpsilon);
}

TEST_F(ScrollPredictorTest, ScrollPredictorTypeSelection) {
  // Empty Predictor when kResamplingScrollEvents is disabled.
  scroll_predictor_ = std::make_unique<ScrollPredictor>();
  VerifyPredictorType(input_prediction::kScrollPredictorNameEmpty);

  // When resampling is enabled, predictor type is set from
  // kResamplingScrollEvents.
  ConfigurePredictorFieldTrial(features::kResamplingScrollEvents,
                               input_prediction::kScrollPredictorNameEmpty);
  scroll_predictor_ = std::make_unique<ScrollPredictor>();
  VerifyPredictorType(input_prediction::kScrollPredictorNameEmpty);

  ConfigurePredictorFieldTrial(features::kResamplingScrollEvents,
                               input_prediction::kScrollPredictorNameLsq);
  scroll_predictor_ = std::make_unique<ScrollPredictor>();
  VerifyPredictorType(input_prediction::kScrollPredictorNameLsq);

  ConfigurePredictorFieldTrial(features::kResamplingScrollEvents,
                               input_prediction::kScrollPredictorNameKalman);
  scroll_predictor_ = std::make_unique<ScrollPredictor>();
  VerifyPredictorType(input_prediction::kScrollPredictorNameKalman);

  ConfigurePredictorFieldTrial(
      features::kResamplingScrollEvents,
      input_prediction::kScrollPredictorNameLinearFirst);
  scroll_predictor_ = std::make_unique<ScrollPredictor>();
  VerifyPredictorType(input_prediction::kScrollPredictorNameLinearFirst);
}

// Check the right filter is selected
TEST_F(ScrollPredictorTest, DefaultFilter) {
  ConfigureFilterFieldTrial(features::kFilteringScrollPrediction, "");
  scroll_predictor_ = std::make_unique<ScrollPredictor>();
  VerifyFilterType(input_prediction::kFilterNameEmpty);
  EXPECT_TRUE(isFilteringEnabled());

  ConfigureFilterFieldTrial(features::kFilteringScrollPrediction,
                            input_prediction::kFilterNameEmpty);
  scroll_predictor_ = std::make_unique<ScrollPredictor>();
  VerifyFilterType(input_prediction::kFilterNameEmpty);
  EXPECT_TRUE(isFilteringEnabled());
}

// We first send 100 events to the scroll predictor with kalman predictor
// enabled and filetring disable and save the results.
// We then send the same events with kalman and the empty filter, we should
// expect the same results.
TEST_F(ScrollPredictorTest, FilteringPrediction) {
  ConfigureFilterFieldTrial(features::kResamplingScrollEvents,
                            input_prediction::kScrollPredictorNameKalman);
  scroll_predictor_ = std::make_unique<ScrollPredictor>();

  std::vector<double> accumulated_deltas;
  WebScopedInputEvent gesture_update;

  for (int i = 0; i < 100; i++) {
    // Create event at time 8*i
    gesture_update = CreateGestureScrollUpdate(0, 3 * i, 8 * i /* ms */);
    // Handle the event 5 ms later
    HandleResampleScrollEvents(gesture_update, 8 * i + 5 /* ms */);
    EXPECT_FALSE(isFilteringEnabled());
    accumulated_deltas.push_back(GetLastAccumulatedDelta().y());
  }
  EXPECT_EQ((int)accumulated_deltas.size(), 100);

  // Now we enable filtering and compare the deltas
  ConfigurePredictorFieldTrial(features::kResamplingScrollEvents,
                               input_prediction::kScrollPredictorNameKalman);
  ConfigureFilterFieldTrial(features::kFilteringScrollPrediction,
                            input_prediction::kFilterNameEmpty);
  scroll_predictor_ = std::make_unique<ScrollPredictor>();

  for (int i = 0; i < 100; i++) {
    // Create event at time 8*i
    gesture_update = CreateGestureScrollUpdate(0, 3 * i, 8 * i /* ms */);
    // Handle the event 5 ms later
    HandleResampleScrollEvents(gesture_update, 8 * i + 5 /* ms */);
    EXPECT_TRUE(isFilteringEnabled());
    EXPECT_NEAR(accumulated_deltas[i], GetLastAccumulatedDelta().y(), 0.00001);
  }
}

}  // namespace test
}  // namespace ui
