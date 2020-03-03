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
  ScopedMonitorImpl(
      base::flat_map<ScopedMonitor*, EventMetrics>* events_metrics,
      const EventMetrics& event_metrics)
      : events_metrics_(events_metrics) {
    events_metrics_->emplace(this, event_metrics);
  }

  ~ScopedMonitorImpl() override { events_metrics_->erase(this); }

 private:
  base::flat_map<ScopedMonitor*, EventMetrics>* const events_metrics_;
};

}  // namespace

EventsMetricsManager::ScopedMonitor::~ScopedMonitor() = default;

EventsMetricsManager::EventsMetricsManager() = default;
EventsMetricsManager::~EventsMetricsManager() = default;

std::unique_ptr<EventsMetricsManager::ScopedMonitor>
EventsMetricsManager::GetScopedMonitor(const EventMetrics& event_metrics) {
  if (!event_metrics.IsWhitelisted())
    return nullptr;
  return std::make_unique<ScopedMonitorImpl>(&active_events_, event_metrics);
}

void EventsMetricsManager::SaveActiveEventsMetrics() {
  saved_events_.reserve(saved_events_.size() + active_events_.size());
  for (auto it = active_events_.begin(); it != active_events_.end();) {
    saved_events_.push_back(it->second);
    it = active_events_.erase(it);
  }
}

std::vector<EventMetrics> EventsMetricsManager::TakeSavedEventsMetrics() {
  std::vector<EventMetrics> result;
  result.swap(saved_events_);
  return result;
}

void EventsMetricsManager::AppendToSavedEventsMetrics(
    std::vector<EventMetrics> events_metrics) {
  saved_events_.reserve(saved_events_.size() + events_metrics.size());
  saved_events_.insert(saved_events_.end(), events_metrics.begin(),
                       events_metrics.end());
}

}  // namespace cc
