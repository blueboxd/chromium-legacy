// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/instance_throttle/arc_instance_throttle.h"

#include <string>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/chromeos/throttle_observer.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_util.h"

namespace arc {

namespace {

class DefaultDelegateImpl : public ArcInstanceThrottle::Delegate {
 public:
  DefaultDelegateImpl() = default;
  ~DefaultDelegateImpl() override = default;
  void SetCpuRestriction(bool restrict) override {
    SetArcCpuRestriction(restrict);
  }

  void RecordCpuRestrictionDisabledUMA(const std::string& observer_name,
                                       base::TimeDelta delta) override {
    DVLOG(2) << "ARC throttling was disabled for "
             << delta.InMillisecondsRoundedUp()
             << " ms due to: " << observer_name;
    UmaHistogramLongTimes("Arc.CpuRestrictionDisabled." + observer_name, delta);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultDelegateImpl);
};

// Singleton factory for ArcInstanceThrottle.
class ArcInstanceThrottleFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcInstanceThrottle,
          ArcInstanceThrottleFactory> {
 public:
  static constexpr const char* kName = "ArcInstanceThrottleFactory";
  static ArcInstanceThrottleFactory* GetInstance() {
    return base::Singleton<ArcInstanceThrottleFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ArcInstanceThrottleFactory>;
  ArcInstanceThrottleFactory() {
    DependsOn(ArcBootPhaseMonitorBridgeFactory::GetInstance());
  }
  ~ArcInstanceThrottleFactory() override = default;
};

}  // namespace

// static
ArcInstanceThrottle* ArcInstanceThrottle::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcInstanceThrottleFactory::GetForBrowserContext(context);
}

// static
ArcInstanceThrottle* ArcInstanceThrottle::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcInstanceThrottleFactory::GetForBrowserContextForTesting(context);
}

ArcInstanceThrottle::ArcInstanceThrottle(content::BrowserContext* context,
                                         ArcBridgeService* bridge_service)
    : context_(context),
      delegate_(std::make_unique<DefaultDelegateImpl>()),
      weak_ptr_factory_(this) {
  auto callback =
      base::BindRepeating(&ArcInstanceThrottle::OnObserverStateChanged,
                          weak_ptr_factory_.GetWeakPtr());
  for (auto* observer : GetAllObservers())
    observer->StartObserving(context, callback);
}

ArcInstanceThrottle::~ArcInstanceThrottle() = default;

void ArcInstanceThrottle::Shutdown() {
  for (auto* observer : GetAllObservers())
    observer->StopObserving();
}

void ArcInstanceThrottle::OnObserverStateChanged() {
  chromeos::ThrottleObserver::PriorityLevel max_level =
      chromeos::ThrottleObserver::PriorityLevel::LOW;
  chromeos::ThrottleObserver* effective_observer = nullptr;
  std::string active_observers;

  for (auto* observer : GetAllObservers()) {
    if (!observer->active())
      continue;
    active_observers += (" " + observer->GetDebugDescription());
    if (observer->level() >= max_level) {
      max_level = observer->level();
      effective_observer = observer;
    }
  }

  DVLOG_IF(1, !active_observers.empty())
      << "Active Throttle Observers: " << active_observers;

  if (effective_observer != last_effective_observer_) {
    // If there is a new effective observer, record the duration that the last
    // effective observer was active.
    if (last_effective_observer_)
      delegate_->RecordCpuRestrictionDisabledUMA(
          last_effective_observer_->name(),
          base::TimeTicks::Now() - last_throttle_transition_);
    last_throttle_transition_ = base::TimeTicks::Now();
    last_effective_observer_ = effective_observer;
  }

  ThrottleInstance(max_level);
}

void ArcInstanceThrottle::ThrottleInstance(
    chromeos::ThrottleObserver::PriorityLevel level) {
  if (level_ == level)
    return;
  level_ = level;
  switch (level_) {
    case chromeos::ThrottleObserver::PriorityLevel::CRITICAL:
    case chromeos::ThrottleObserver::PriorityLevel::IMPORTANT:
    case chromeos::ThrottleObserver::PriorityLevel::NORMAL:
      delegate_->SetCpuRestriction(false);
      break;
    case chromeos::ThrottleObserver::PriorityLevel::LOW:
    case chromeos::ThrottleObserver::PriorityLevel::UNKNOWN:
      delegate_->SetCpuRestriction(true);
      break;
  }
}

std::vector<chromeos::ThrottleObserver*>
ArcInstanceThrottle::GetAllObservers() {
  if (!observers_for_testing_.empty())
    return observers_for_testing_;
  return {&active_window_throttle_observer_, &boot_phase_throttle_observer_};
}

void ArcInstanceThrottle::NotifyObserverStateChangedForTesting() {
  OnObserverStateChanged();
}

void ArcInstanceThrottle::SetObserversForTesting(
    const std::vector<chromeos::ThrottleObserver*>& observers) {
  for (auto* observer : GetAllObservers())
    observer->StopObserving();
  observers_for_testing_ = observers;
  auto callback =
      base::BindRepeating(&ArcInstanceThrottle::OnObserverStateChanged,
                          weak_ptr_factory_.GetWeakPtr());
  for (auto* observer : GetAllObservers())
    observer->StartObserving(context_, callback);
}
}  // namespace arc
