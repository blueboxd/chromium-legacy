// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_EVENT_METRICS_H_
#define CC_METRICS_EVENT_METRICS_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "ui/events/types/event_type.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {

// Data about an event used by CompositorFrameReporter in generating event
// latency metrics.
class CC_EXPORT EventMetrics {
 public:
  EventMetrics(ui::EventType type,
               base::TimeTicks time_stamp,
               base::Optional<ui::ScrollInputType> scroll_input_type);

  EventMetrics(const EventMetrics&);
  EventMetrics& operator=(const EventMetrics&);

  // Determines if the event is whitelisted for event latency reporting. If not,
  // this event is ignored in histogram reporting.
  bool IsWhitelisted() const;

  // Returns a string representing event type. Should only be called for
  // whitelisted event types.
  const char* GetTypeName() const;

  // Returns a string representing scroll input type. Should only be called for
  // scroll events.
  const char* GetScrollTypeName() const;

  ui::EventType type() const { return type_; }

  base::TimeTicks time_stamp() const { return time_stamp_; }

  const base::Optional<ui::ScrollInputType>& scroll_input_type() const {
    return scroll_input_type_;
  }

  // Used in tests to check expectations on EventMetrics objects.
  bool operator==(const EventMetrics& other) const;

 private:
  ui::EventType type_;
  base::TimeTicks time_stamp_;

  // Only available for scroll events and represents the type of input device
  // for the event.
  base::Optional<ui::ScrollInputType> scroll_input_type_;
};

// Struct storing event metrics from both main and impl threads.
struct CC_EXPORT EventMetricsSet {
  EventMetricsSet();
  ~EventMetricsSet();
  EventMetricsSet(std::vector<EventMetrics> main_thread_event_metrics,
                  std::vector<EventMetrics> impl_thread_event_metrics);
  EventMetricsSet(EventMetricsSet&&);
  EventMetricsSet& operator=(EventMetricsSet&&);

  EventMetricsSet(const EventMetricsSet&) = delete;
  EventMetricsSet& operator=(const EventMetricsSet&) = delete;

  std::vector<EventMetrics> main_event_metrics;
  std::vector<EventMetrics> impl_event_metrics;
};

}  // namespace cc

#endif  // CC_METRICS_EVENT_METRICS_H_
