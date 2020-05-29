// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/event_metrics.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/stl_util.h"

namespace cc {
namespace {

// Names for enum values in EventMetrics::EventType.
constexpr const char* kEventTypeNames[] = {
    "MousePressed",        "MouseReleased",    "MouseWheel",
    "KeyPressed",          "KeyReleased",      "TouchPressed",
    "TouchReleased",       "TouchMoved",       "GestureScrollBegin",
    "GestureScrollUpdate", "GestureScrollEnd",
};
static_assert(base::size(kEventTypeNames) ==
                  static_cast<int>(EventMetrics::EventType::kMaxValue) + 1,
              "EventMetrics::EventType has changed.");

// Names for enum values in EventMetrics::ScrollType.
constexpr const char* kScrollTypeNames[] = {
    "Autoscroll",
    "Scrollbar",
    "Touchscreen",
    "Wheel",
};
static_assert(base::size(kScrollTypeNames) ==
                  static_cast<int>(EventMetrics::ScrollType::kMaxValue) + 1,
              "EventMetrics::ScrollType has changed.");

base::Optional<EventMetrics::EventType> ToWhitelistedEventType(
    ui::EventType ui_event_type) {
  constexpr ui::EventType kUiEventTypeWhitelist[]{
      ui::ET_MOUSE_PRESSED,        ui::ET_MOUSE_RELEASED,
      ui::ET_MOUSEWHEEL,           ui::ET_KEY_PRESSED,
      ui::ET_KEY_RELEASED,         ui::ET_TOUCH_PRESSED,
      ui::ET_TOUCH_RELEASED,       ui::ET_TOUCH_MOVED,
      ui::ET_GESTURE_SCROLL_BEGIN, ui::ET_GESTURE_SCROLL_UPDATE,
      ui::ET_GESTURE_SCROLL_END,
  };
  static_assert(base::size(kUiEventTypeWhitelist) ==
                    static_cast<int>(EventMetrics::EventType::kMaxValue) + 1,
                "EventMetrics::EventType has changed");
  for (size_t i = 0; i < base::size(kUiEventTypeWhitelist); i++) {
    if (ui_event_type == kUiEventTypeWhitelist[i])
      return static_cast<EventMetrics::EventType>(i);
  }
  return base::nullopt;
}

base::Optional<EventMetrics::ScrollType> ToScrollType(
    const base::Optional<ui::ScrollInputType>& scroll_input_type) {
  if (!scroll_input_type)
    return base::nullopt;

  switch (*scroll_input_type) {
    case ui::ScrollInputType::kAutoscroll:
      return EventMetrics::ScrollType::kAutoscroll;
    case ui::ScrollInputType::kScrollbar:
      return EventMetrics::ScrollType::kScrollbar;
    case ui::ScrollInputType::kTouchscreen:
      return EventMetrics::ScrollType::kTouchscreen;
    case ui::ScrollInputType::kWheel:
      return EventMetrics::ScrollType::kWheel;
  }
}

}  // namespace

std::unique_ptr<EventMetrics> EventMetrics::Create(
    ui::EventType type,
    base::TimeTicks time_stamp,
    base::Optional<ui::ScrollInputType> scroll_input_type) {
  base::Optional<EventType> whitelisted_type = ToWhitelistedEventType(type);
  if (!whitelisted_type)
    return nullptr;
  return base::WrapUnique(new EventMetrics(*whitelisted_type, time_stamp,
                                           ToScrollType(scroll_input_type)));
}

EventMetrics::EventMetrics(EventType type,
                           base::TimeTicks time_stamp,
                           base::Optional<ScrollType> scroll_type)
    : type_(type), time_stamp_(time_stamp), scroll_type_(scroll_type) {}

EventMetrics::EventMetrics(const EventMetrics&) = default;
EventMetrics& EventMetrics::operator=(const EventMetrics&) = default;

const char* EventMetrics::GetTypeName() const {
  return kEventTypeNames[static_cast<int>(type_)];
}

const char* EventMetrics::GetScrollTypeName() const {
  DCHECK(scroll_type_) << "Event is not a scroll event.";

  return kScrollTypeNames[static_cast<int>(*scroll_type_)];
}

bool EventMetrics::operator==(const EventMetrics& other) const {
  return std::tie(type_, time_stamp_, scroll_type_) ==
         std::tie(other.type_, other.time_stamp_, other.scroll_type_);
}

// EventMetricsSet
EventMetricsSet::EventMetricsSet() = default;
EventMetricsSet::~EventMetricsSet() = default;
EventMetricsSet::EventMetricsSet(
    std::vector<EventMetrics> main_thread_event_metrics,
    std::vector<EventMetrics> impl_thread_event_metrics)
    : main_event_metrics(std::move(main_thread_event_metrics)),
      impl_event_metrics(std::move(impl_thread_event_metrics)) {}
EventMetricsSet::EventMetricsSet(EventMetricsSet&& other) = default;
EventMetricsSet& EventMetricsSet::operator=(EventMetricsSet&& other) = default;

}  // namespace cc
