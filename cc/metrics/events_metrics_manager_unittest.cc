// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/events_metrics_manager.h"

#include <utility>
#include <vector>

#include "cc/input/scroll_input_type.h"
#include "cc/metrics/event_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/types/event_type.h"

namespace cc {
namespace {
base::TimeTicks TimeAtMs(int ms) {
  return base::TimeTicks() + base::TimeDelta::FromMilliseconds(ms);
}
}  // namespace

class EventsMetricsManagerTest : public testing::Test {
 public:
  EventsMetricsManagerTest() = default;
  ~EventsMetricsManagerTest() override = default;

 protected:
  EventsMetricsManager manager_;
};

// Tests that EventMetrics are saved only if they have a whitelisted event type
// and SaveActiveEventMetrics() is called inside their corresponding monitor's
// scope.
TEST_F(EventsMetricsManagerTest, EventsMetricsSaved) {
  enum class Behavior {
    kDoNotSave,
    kSaveInsideScope,
    kSaveOutsideScope,
  };

  std::vector<std::pair<EventMetrics, Behavior>> events = {
      // A whitelisted event type for which SaveActiveEventMetrics() is not
      // called.
      {{ui::ET_MOUSE_PRESSED, TimeAtMs(0), base::nullopt},
       Behavior::kDoNotSave},

      // A whitelisted event type for which SaveActiveEventMetrics() is called
      // inside its monitor scope.
      {{ui::ET_MOUSE_PRESSED, TimeAtMs(1), base::nullopt},
       Behavior::kSaveInsideScope},

      // A whitelisted event type for which SaveActiveEventMetrics() is called
      // after its monitor scope is finished.
      {{ui::ET_MOUSE_PRESSED, TimeAtMs(2), base::nullopt},
       Behavior::kSaveOutsideScope},

      // A non-whitelisted event type for which SaveActiveEventMetrics() is
      // called inside its monitor scope.
      {{ui::ET_MOUSE_MOVED, TimeAtMs(3), base::nullopt},
       Behavior::kSaveInsideScope},
  };
  EXPECT_TRUE(events[0].first.IsWhitelisted());
  EXPECT_TRUE(events[1].first.IsWhitelisted());
  EXPECT_TRUE(events[2].first.IsWhitelisted());
  EXPECT_FALSE(events[3].first.IsWhitelisted());

  // Out of the above events, only those with a whitelisted event type, for
  // which SaveActiveEventMetrics() is called inside its monitor scope, are
  // expected to be saved.
  std::vector<EventMetrics> expected_saved_events = {
      events[1].first,
  };

  for (auto& event : events) {
    {
      auto monitor = manager_.GetScopedMonitor(event.first);
      if (event.second == Behavior::kSaveInsideScope)
        manager_.SaveActiveEventMetrics();
      // Ending the scope destroys the |monitor|.
    }
    if (event.second == Behavior::kSaveOutsideScope)
      manager_.SaveActiveEventMetrics();
  }

  // Check saved event metrics are as expected.
  EXPECT_THAT(manager_.TakeSavedEventsMetrics(),
              testing::ContainerEq(expected_saved_events));

  // The first call to TakeSavedEventsMetrics() should remove events metrics
  // from the manager, so the second call should return empty list.
  EXPECT_THAT(manager_.TakeSavedEventsMetrics(), testing::IsEmpty());
}

}  // namespace cc
