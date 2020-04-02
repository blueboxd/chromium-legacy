// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/events_metrics_manager.h"

#include <memory>
#include <utility>

#include "base/stl_util.h"

namespace cc {
namespace {

class ScopedMonitorImpl : public EventsMetricsManager::ScopedMonitor {
 public:
  explicit ScopedMonitorImpl(base::Optional<EventMetrics>* active_event)
      : active_event_(active_event) {
    DCHECK(active_event_);
  }

  ~ScopedMonitorImpl() override { *active_event_ = base::nullopt; }

 private:
  base::Optional<EventMetrics>* active_event_;
};

}  // namespace

EventsMetricsManager::ScopedMonitor::~ScopedMonitor() = default;

EventsMetricsManager::EventsMetricsManager() = default;
EventsMetricsManager::~EventsMetricsManager() = default;

std::unique_ptr<EventsMetricsManager::ScopedMonitor>
EventsMetricsManager::GetScopedMonitor(const EventMetrics& event_metrics) {
  DCHECK(!active_event_);
  if (!event_metrics.IsWhitelisted())
    return nullptr;
  active_event_ = event_metrics;
  return std::make_unique<ScopedMonitorImpl>(&active_event_);
}

void EventsMetricsManager::SaveActiveEventMetrics() {
  if (active_event_) {
    saved_events_.push_back(*active_event_);
    active_event_ = base::nullopt;
  }
}

std::vector<EventMetrics> EventsMetricsManager::TakeSavedEventsMetrics() {
  std::vector<EventMetrics> result;
  result.swap(saved_events_);
  return result;
}

}  // namespace cc
