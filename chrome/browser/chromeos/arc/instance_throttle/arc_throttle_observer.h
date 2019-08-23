// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INSTANCE_THROTTLE_ARC_THROTTLE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INSTANCE_THROTTLE_ARC_THROTTLE_OBSERVER_H_

#include <string>

#include "base/callback.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace content {
class BrowserContext;
}

namespace arc {
class ArcBridgeService;

// Base throttle observer class. Each throttle observer watches a particular
// condition (window activates, mojom instance disconnects, and so on) and
// notifies any observers when there is a change.
class ArcThrottleObserver {
 public:
  using ObserverStateChangedCallback = base::RepeatingCallback<void()>;
  enum class PriorityLevel { UNKNOWN, LOW, NORMAL, IMPORTANT, CRITICAL };

  ArcThrottleObserver(PriorityLevel level, const std::string& name);
  virtual ~ArcThrottleObserver();

  // Starts observing (overridden in derived classes to register self as
  // observer for a particular condition)
  virtual void StartObserving(ArcBridgeService* arc_bridge_service,
                              content::BrowserContext* content,
                              const ObserverStateChangedCallback& callback);
  // Stops observing. This method is the last place in which context can be
  // used.
  virtual void StopObserving();

  // Sets the observer to active, and runs ObserverStateChanged callback if
  // there was a change in state.
  void SetActive(bool active);

  std::string GetDebugDescription() const;

  PriorityLevel level() const { return level_; }
  const std::string& name() const { return name_; }
  bool active() const { return active_; }

 protected:
  const PriorityLevel level_{PriorityLevel::UNKNOWN};
  bool active_ = false;
  const std::string name_;  // For logging purposes
  ObserverStateChangedCallback callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcThrottleObserver);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INSTANCE_THROTTLE_ARC_THROTTLE_OBSERVER_H_
