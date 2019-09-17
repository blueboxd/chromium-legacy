// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INSTANCE_THROTTLE_ARC_INSTANCE_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INSTANCE_THROTTLE_ARC_INSTANCE_THROTTLE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/instance_throttle/arc_active_window_throttle_observer.h"
#include "chrome/browser/chromeos/arc/instance_throttle/arc_boot_phase_throttle_observer.h"
#include "chrome/browser/chromeos/arc/instance_throttle/arc_throttle_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace arc {
class ArcBridgeService;

// This class holds a number of observers which watch for several conditions
// (window activation, mojom instance connection, etc) and adjusts the
// throttling state of the ARC container on a change in conditions.
class ArcInstanceThrottle : public KeyedService {
 public:
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    virtual void SetCpuRestriction(bool) = 0;
    virtual void RecordCpuRestrictionDisabledUMA(
        const std::string& observer_name,
        base::TimeDelta delta) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Returns singleton instance for the given BrowserContext, or nullptr if
  // the browser |context| is not allowed to use ARC.
  static ArcInstanceThrottle* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcInstanceThrottle* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcInstanceThrottle(content::BrowserContext* context,
                      ArcBridgeService* arc_bridge_service);
  ~ArcInstanceThrottle() override;

  // KeyedService:
  void Shutdown() override;

  // Functions for testing
  void NotifyObserverStateChangedForTesting();
  void SetObserversForTesting(
      const std::vector<ArcThrottleObserver*>& observers);

  void set_delegate_for_testing(std::unique_ptr<Delegate> delegate) {
    delegate_ = std::move(delegate);
  }

 private:
  std::vector<ArcThrottleObserver*> GetAllObservers();
  void OnObserverStateChanged();
  void ThrottleInstance(ArcThrottleObserver::PriorityLevel level);

  ArcBridgeService* arc_bridge_service_;
  content::BrowserContext* context_;
  std::vector<ArcThrottleObserver*> observers_for_testing_;
  std::unique_ptr<Delegate> delegate_;
  ArcThrottleObserver::PriorityLevel level_{
      ArcThrottleObserver::PriorityLevel::UNKNOWN};
  ArcThrottleObserver* last_effective_observer_ = nullptr;
  base::TimeTicks last_throttle_transition_;

  // Throttle Observers
  ArcActiveWindowThrottleObserver active_window_throttle_observer_;
  ArcBootPhaseThrottleObserver boot_phase_throttle_observer_;

  base::WeakPtrFactory<ArcInstanceThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcInstanceThrottle);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INSTANCE_THROTTLE_ARC_INSTANCE_THROTTLE_H_
