// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/instance_throttle/arc_instance_throttle.h"

#include <string>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/chromeos/arc/instance_throttle/arc_throttle_observer.h"
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
    : arc_bridge_service_(bridge_service),
      context_(context),
      delegate_(std::make_unique<DefaultDelegateImpl>()),
      weak_ptr_factory_(this) {
  auto callback =
      base::BindRepeating(&ArcInstanceThrottle::OnObserverStateChanged,
                          weak_ptr_factory_.GetWeakPtr());
  for (auto* observer : GetAllObservers())
    observer->StartObserving(bridge_service, context, callback);
}

ArcInstanceThrottle::~ArcInstanceThrottle() = default;

void ArcInstanceThrottle::Shutdown() {
  for (auto* observer : GetAllObservers())
    observer->StopObserving();
}

void ArcInstanceThrottle::OnObserverStateChanged() {
  ArcThrottleObserver::PriorityLevel max_level =
      ArcThrottleObserver::PriorityLevel::LOW;
  std::string active_observers;
  for (auto* observer : GetAllObservers()) {
    if (!observer->active())
      continue;
    active_observers += (" " + observer->GetDebugDescription());
    if (max_level < observer->level())
      max_level = observer->level();
  }
  if (!active_observers.empty())
    VLOG(1) << "Active Throttle Observers: " << active_observers;
  ThrottleInstance(max_level);
}

void ArcInstanceThrottle::ThrottleInstance(
    ArcThrottleObserver::PriorityLevel level) {
  if (level_ == level)
    return;
  level_ = level;
  switch (level_) {
    case ArcThrottleObserver::PriorityLevel::CRITICAL:
    case ArcThrottleObserver::PriorityLevel::IMPORTANT:
    case ArcThrottleObserver::PriorityLevel::NORMAL:
      delegate_->SetCpuRestriction(false);
      break;
    case ArcThrottleObserver::PriorityLevel::LOW:
    case ArcThrottleObserver::PriorityLevel::UNKNOWN:
      delegate_->SetCpuRestriction(true);
      break;
  }
}

std::vector<ArcThrottleObserver*> ArcInstanceThrottle::GetAllObservers() {
  if (!observers_for_testing_.empty())
    return observers_for_testing_;
  return {&active_window_throttle_observer_, &boot_phase_throttle_observer_};
}

void ArcInstanceThrottle::NotifyObserverStateChangedForTesting() {
  OnObserverStateChanged();
}

void ArcInstanceThrottle::SetObserversForTesting(
    const std::vector<ArcThrottleObserver*>& observers) {
  for (auto* observer : GetAllObservers())
    observer->StopObserving();
  observers_for_testing_ = observers;
  auto callback =
      base::BindRepeating(&ArcInstanceThrottle::OnObserverStateChanged,
                          weak_ptr_factory_.GetWeakPtr());
  for (auto* observer : GetAllObservers())
    observer->StartObserving(arc_bridge_service_, context_, callback);
}
}  // namespace arc
