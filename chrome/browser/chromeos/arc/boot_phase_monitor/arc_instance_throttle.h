// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_BOOT_PHASE_MONITOR_ARC_INSTANCE_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_BOOT_PHASE_MONITOR_ARC_INSTANCE_THROTTLE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/sessions/session_restore_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "ui/wm/public/activation_change_observer.h"

namespace arc {

// A class that watches window activations and prioritizes the ARC instance when
// one of ARC windows is activated. The class also unprioritizes the instance
// when non-ARC window such as Chrome is activated. Additionally, the class
// prioritizes/unprioritizes in response to ARC boot events from
// ArcSessionManager.
class ArcInstanceThrottle : public KeyedService,
                            public wm::ActivationChangeObserver,
                            public ArcSessionManager::Observer,
                            public SessionRestoreObserver,
                            public ArcBootPhaseMonitorBridge::Observer {
 public:
  // Delegate for testing
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void SetCpuRestriction(bool) = 0;
  };
  // Returns singleton instance for the given BrowserContext, or nullptr if the
  // browser |context| is not allowed to use ARC.
  static ArcInstanceThrottle* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcInstanceThrottle* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcInstanceThrottle(content::BrowserContext* context,
                      ArcBridgeService* arc_bridge_service);
  ~ArcInstanceThrottle() override;

  // KeyedService override
  void Shutdown() override;

  // All of these observers will be refactored into locks.
  // wm::ActivationChangeObserver overrides:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;
  // ArcSessionManager::Observer overrides
  void OnArcPlayStoreEnabledChanged(bool enabled) override;
  void OnArcStarted() override;
  void OnArcInitialStart() override;
  void OnArcSessionStopped(ArcStopReason stop_reason) override;
  void OnArcSessionRestarting() override;

  // SessionRestoreObserver
  void OnSessionRestoreFinishedLoadingTabs() override;

  // ArcBootPhaseMonitorBridge::Observer
  void OnBootCompleted() override;

  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate) {
    delegate_for_testing_ = std::move(delegate);
  }
  void OnExtensionsReadyForTesting() { OnExtensionsReady(); }
  bool observing_window_activations() const {
    return observing_window_activations_;
  }

 private:
  void StartObservingWindowActivations();
  void StopObservingWindowActivations();
  // Called when ExtensionsServices finishes loading all extensions for the
  // profile.
  void OnExtensionsReady();

  content::BrowserContext* context_;
  std::unique_ptr<Delegate> delegate_for_testing_;
  bool observing_window_activations_ = false;
  bool provisioning_finished_ = false;
  bool boot_completed_ = false;

  base::WeakPtrFactory<ArcInstanceThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcInstanceThrottle);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_BOOT_PHASE_MONITOR_ARC_INSTANCE_THROTTLE_H_
