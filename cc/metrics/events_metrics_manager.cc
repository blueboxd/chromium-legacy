// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/events_metrics_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/stl_util.h"

namespace cc {
namespace {

class ScopedMonitorImpl : public EventsMetricsManager::ScopedMonitor {
 public:
  explicit ScopedMonitorImpl(base::OnceClosure done_callback)
      : closure_runner_(std::move(done_callback)) {}

 private:
  base::ScopedClosureRunner closure_runner_;
};

}  // namespace

EventsMetricsManager::ScopedMonitor::~ScopedMonitor() = default;

EventsMetricsManager::EventsMetricsManager() = default;
EventsMetricsManager::~EventsMetricsManager() = default;

std::unique_ptr<EventsMetricsManager::ScopedMonitor>
EventsMetricsManager::GetScopedMonitor(
    std::unique_ptr<EventMetrics> event_metrics) {
  DCHECK(!active_event_);
  if (!event_metrics)
    return nullptr;
  active_event_ = std::move(event_metrics);
  return std::make_unique<ScopedMonitorImpl>(base::BindOnce(
      &EventsMetricsManager::OnScopedMonitorEnded, weak_factory_.GetWeakPtr()));
}

void EventsMetricsManager::SaveActiveEventMetrics() {
  if (active_event_) {
    saved_events_.push_back(*active_event_);
    active_event_.reset();
  }
}

std::vector<EventMetrics> EventsMetricsManager::TakeSavedEventsMetrics() {
  std::vector<EventMetrics> result;
  result.swap(saved_events_);
  return result;
}

void EventsMetricsManager::OnScopedMonitorEnded() {
  active_event_.reset();
}

}  // namespace cc
