// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_throttle_observer.h"

namespace arc {

namespace {

std::string LevelToString(ArcThrottleObserver::PriorityLevel level) {
  switch (level) {
    case ArcThrottleObserver::PriorityLevel::LOW:
      return "PriorityLevel::LOW";
    case ArcThrottleObserver::PriorityLevel::NORMAL:
      return "PriorityLevel::NORMAL";
    case ArcThrottleObserver::PriorityLevel::IMPORTANT:
      return "PriorityLevel::IMPORTANT";
    case ArcThrottleObserver::PriorityLevel::CRITICAL:
      return "PriorityLevel::CRITICAL";
    case ArcThrottleObserver::PriorityLevel::UNKNOWN:
      return "PriorityLevel::UNKNOWN";
  }
}

}  // namespace

ArcThrottleObserver::ArcThrottleObserver(
    ArcThrottleObserver::PriorityLevel level,
    const std::string& name)
    : level_(level), name_(name) {}

ArcThrottleObserver::~ArcThrottleObserver() = default;

void ArcThrottleObserver::StartObserving(
    ArcBridgeService* arc_bridge_service,
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  DCHECK(!callback_);
  callback_ = callback;
}

void ArcThrottleObserver::StopObserving() {
  callback_.Reset();
}

void ArcThrottleObserver::SetActive(bool active) {
  if (active_ == active)
    return;
  active_ = active;
  if (callback_)
    callback_.Run();
}

std::string ArcThrottleObserver::GetDebugDescription() const {
  return ("ArcThrottleObserver(" + name() + ", " + LevelToString(level()) +
          ", " + (active() ? "active" : "inactive") + ")");
}

}  // namespace arc
