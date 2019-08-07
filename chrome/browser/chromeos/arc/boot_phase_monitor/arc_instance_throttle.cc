// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_instance_throttle.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/single_sample_metrics.h"
#include "base/one_shot_event.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_stop_reason.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "ui/wm/public/activation_client.h"
namespace arc {

namespace {

// Returns true if the window is in the app list window container.
bool IsAppListWindow(const aura::Window* window) {
  if (!window)
    return false;

  const aura::Window* parent = window->parent();
  return parent &&
         parent->id() == ash::ShellWindowId::kShellWindowId_AppListContainer;
}

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
    DependsOn(extensions::ExtensionsBrowserClient::Get()
                  ->GetExtensionSystemFactory());
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
                                         ArcBridgeService* arc_bridge_service)
    : context_(context),
      delegate_for_testing_(std::make_unique<DefaultDelegateImpl>()),
      weak_ptr_factory_(this) {
  auto* arc_session_manager = ArcSessionManager::Get();
  DCHECK(arc_session_manager);
  arc_session_manager->AddObserver(this);
  SessionRestore::AddObserver(this);
  auto* boot_phase_monitor =
      ArcBootPhaseMonitorBridge::GetForBrowserContext(context_);
  if (boot_phase_monitor)  // for unit testing.
    boot_phase_monitor->AddObserver(this);

  auto* profile = Profile::FromBrowserContext(context);
  auto* extension_system = extensions::ExtensionSystem::Get(profile);
  DCHECK(extension_system);
  extension_system->ready().Post(
      FROM_HERE, base::BindOnce(&ArcInstanceThrottle::OnExtensionsReady,
                                weak_ptr_factory_.GetWeakPtr()));
}

ArcInstanceThrottle::~ArcInstanceThrottle() {}

void ArcInstanceThrottle::Shutdown() {
  auto* arc_session_manager = ArcSessionManager::Get();
  arc_session_manager->RemoveObserver(this);
  SessionRestore::RemoveObserver(this);
  auto* boot_phase_monitor =
      ArcBootPhaseMonitorBridge::GetForBrowserContext(context_);
  if (boot_phase_monitor)  // for unit testing.
    boot_phase_monitor->RemoveObserver(this);
}

void ArcInstanceThrottle::OnWindowActivated(ActivationReason reason,
                                            aura::Window* gained_active,
                                            aura::Window* lost_active) {
  // The current active window defines whether the instance should be throttled
  // or not i.e. if it's a native window then throttle Android, if it's an
  // Android window then unthrottle it.
  //
  // However, the app list needs to be handled differently. The app list is
  // deemed as an temporary overlay over the user's main window for browsing
  // through or selecting an app. Consequently, if the app list is brought over
  // a Chrome window then IsAppListWindow(gained_active) is true and this event
  // is ignored. The instance continues to be throttled. Once the app list is
  // closed then IsAppListWindow(lost_active) is true and the instance continues
  // to be throttled. Similarly, if the app list is brought over an Android
  // window, the instance continues to be unthrottled.
  //
  // On launching an app from the app list on Chrome OS the following events
  // happen -
  //
  // - When an Android app icon is clicked the instance is unthrottled. This
  // logic resides in |LaunchAppWithIntent| in
  // src/chrome/browser/ui/app_list/arc/arc_app_utils.cc.
  //
  // - Between the time the app opens there is a narrow slice of time where
  // this callback is triggered with |lost_active| equal to the app list window
  // and the gained active possibly a native window. Without the check below the
  // instance will be throttled again, further delaying the app launch. If the
  // app was a native one then the instance was throttled anyway.
  if (IsAppListWindow(lost_active) || IsAppListWindow(gained_active))
    return;
  delegate_for_testing_->SetCpuRestriction(!IsArcAppWindow(gained_active));
}

void ArcInstanceThrottle::StartObservingWindowActivations() {
  observing_window_activations_ = true;
  if (!ash::Shell::HasInstance())  // for unit testing.
    return;
  ash::Shell::Get()->activation_client()->AddObserver(this);
}

void ArcInstanceThrottle::StopObservingWindowActivations() {
  observing_window_activations_ = false;
  delegate_for_testing_->SetCpuRestriction(false);
  if (!ash::Shell::HasInstance())
    return;
  ash::Shell::Get()->activation_client()->RemoveObserver(this);
  VLOG(1) << "ArcInstanceThrottle has stopped observing and "
             "delegate_for_testing_->SetCpuRestriction(false)";
}

void ArcInstanceThrottle::OnExtensionsReady() {
  VLOG(2) << "All extensions are loaded";
  if (!boot_completed_ && !provisioning_finished_) {
    VLOG(1) << "Allowing the instance to use more CPU resources";
    delegate_for_testing_->SetCpuRestriction(false);
  }
}

void ArcInstanceThrottle::OnArcStarted() {
  VLOG(1) << "ArcInstanceThrottle "
             "delegate_for_testing_->SetCpuRestriction(false) in "
             "OnArcStarted()";
  delegate_for_testing_->SetCpuRestriction(false);
}

// Called after provisioning is finished
void ArcInstanceThrottle::OnArcInitialStart() {
  provisioning_finished_ = true;
  if (observing_window_activations_)
    return;
  StartObservingWindowActivations();
  VLOG(1) << "ArcInstanceThrottle started observing in OnArcInitialStart()";
}

void ArcInstanceThrottle::OnArcSessionStopped(ArcStopReason stop_reason) {
  // Stop observing so that the window observer won't interfere with the
  // container startup when the user opts in to ARC. Will remove once we have
  // locks to cover container stopping/restarting.
  if (!observing_window_activations_)
    return;
  StopObservingWindowActivations();
  VLOG(1) << "ArcInstanceThrottle stopped observing in OnArcSessionStopped()";
}

void ArcInstanceThrottle::OnArcSessionRestarting() {
  // Stop observing so that the window observer won't interfere with the
  // container restart.
  if (!observing_window_activations_)
    return;
  StopObservingWindowActivations();
  VLOG(1)
      << "ArcInstanceThrottle stopped observing in OnArcSessionRestarting()";
}

void ArcInstanceThrottle::OnArcPlayStoreEnabledChanged(bool enabled) {
  if (enabled)
    delegate_for_testing_->SetCpuRestriction(false);
}

void ArcInstanceThrottle::OnSessionRestoreFinishedLoadingTabs() {
  VLOG(1) << "All tabs have been restored";
  // If OnArcInitialStart() or OnBootCompleted haven't been called, relax the
  // restriction to let the instance fully start.
  if (!boot_completed_ && !provisioning_finished_) {
    VLOG(1) << "Allowing the instance to use more CPU resources";
    delegate_for_testing_->SetCpuRestriction(false);
  }
}

// Called after boot is completed.
void ArcInstanceThrottle::OnBootCompleted() {
  boot_completed_ = true;
  if (observing_window_activations_)
    return;
  StartObservingWindowActivations();
  VLOG(1) << "ArcInstanceThrottle started observing in OnBootCompleted()";
}
}  // namespace arc
