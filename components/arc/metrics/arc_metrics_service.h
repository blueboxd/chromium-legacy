// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_
#define COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/arc/mojom/metrics.mojom.h"
#include "components/arc/mojom/process.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "ui/events/ozone/gamepad/gamepad_observer.h"
#include "ui/wm/public/activation_change_observer.h"

class BrowserContextKeyedServiceFactory;
class PrefService;

namespace aura {
class Window;
}  // namespace aura

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Collects information from other ArcServices and send UMA metrics.
class ArcMetricsService : public KeyedService,
                          public wm::ActivationChangeObserver,
                          public session_manager::SessionManagerObserver,
                          public chromeos::PowerManagerClient::Observer,
                          public mojom::MetricsHost,
                          public ui::GamepadObserver {
 public:
  using WindowMatcher = base::RepeatingCallback<bool(const aura::Window*)>;

  // Sets the fake WindowMatcher for testing.
  void SetWindowMatcherForTesting(WindowMatcher window_matcher);

  // Sets Clock for testing.
  void SetClockForTesting(base::Clock* clock);

  // Sets TickClock for testing.
  void SetTickClockForTesting(base::TickClock* tick_clock);

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcMetricsService* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcMetricsService* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  // Returns factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  ArcMetricsService(content::BrowserContext* context,
                    ArcBridgeService* bridge_service);
  ~ArcMetricsService() override;

  // Implementations for ConnectionObserver<mojom::ProcessInstance>.
  void OnProcessConnectionReady();
  void OnProcessConnectionClosed();

  // MetricsHost overrides.
  void ReportBootProgress(std::vector<mojom::BootProgressEventPtr> events,
                          mojom::BootType boot_type) override;
  void ReportNativeBridge(mojom::NativeBridgeType native_bridge_type) override;

  // wm::ActivationChangeObserver overrides.
  // Records to UMA when a user has interacted with an ARC app window.
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // session_manager::SessionManagerObserver overrides.
  void OnSessionStateChanged() override;

  // chromeos::PowerManagerClient::Observer overrides.
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& proto) override;

  // ui::GamepadObserver overrides.
  void OnGamepadEvent(const ui::GamepadEvent& event) override;

  // ArcAppListPrefs::Observer callbacks which are called through
  // ArcMetricsServiceProxy.
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent);
  void OnTaskDestroyed(int32_t task_id);

 private:
  // Adapter to be able to also observe ProcessInstance events.
  class ProcessObserver : public ConnectionObserver<mojom::ProcessInstance> {
   public:
    explicit ProcessObserver(ArcMetricsService* arc_metrics_service);
    ~ProcessObserver() override;

   private:
    // ConnectionObserver<mojom::ProcessInstance> overrides.
    void OnConnectionReady() override;
    void OnConnectionClosed() override;

    ArcMetricsService* arc_metrics_service_;

    DISALLOW_COPY_AND_ASSIGN(ProcessObserver);
  };

  void RequestProcessList();
  void ParseProcessList(std::vector<mojom::RunningAppProcessInfoPtr> processes);

  // DBus callbacks.
  void OnArcStartTimeRetrieved(std::vector<mojom::BootProgressEventPtr> events,
                               mojom::BootType boot_type,
                               base::Optional<base::TimeTicks> arc_start_time);

  // Restores accumulated ARC++ engagement time in previous sessions from
  // profile preferences.
  void RestoreEngagementTimeFromPrefs();

  // Called periodically to save accumulated results to profile preferences.
  void SaveEngagementTimeToPrefs();

  // Called whenever engagement state is changed. Time spent in last state is
  // accumulated to corresponding metrics.
  void UpdateEngagementTime();

  // Records accumulated engagement time metrics to UMA if necessary (i.e. day
  // has changed).
  void RecordEngagementTimeToUmaIfNeeded();

  // Resets accumulated engagement times to zero, and updates both OS version
  // and day ID.
  void ResetEngagementTimePrefs();

  bool ShouldAccumulateEngagementTotalTime() const;
  bool ShouldAccumulateEngagementForegroundTime() const;
  bool ShouldAccumulateEngagementBackgroundTime() const;
  bool ShouldRecordEngagementTimeToUma() const;

  THREAD_CHECKER(thread_checker_);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  // A function to determine if a window is an ARC window, which can be
  // replaced in tests.
  WindowMatcher window_matcher_;

  ProcessObserver process_observer_;
  base::RepeatingTimer request_process_list_timer_;

  PrefService* const pref_service_;
  const base::Clock* clock_;
  const base::TickClock* tick_clock_;
  base::RepeatingTimer update_engagement_time_timer_;
  base::RepeatingTimer save_engagement_time_to_prefs_timer_;
  base::TimeTicks last_update_ticks_;

  // States for determining which engagement metrics should we accumulate to.
  bool was_session_active_ = false;
  bool was_screen_dimmed_ = false;
  bool was_arc_window_active_ = false;
  std::vector<int32_t> task_ids_;

  // Accumulated results and associated state which are saved to profile
  // preferences at fixed interval.
  int day_id_ = 0;
  base::TimeDelta engagement_time_total_;
  base::TimeDelta engagement_time_foreground_;
  base::TimeDelta engagement_time_background_;

  bool gamepad_interaction_recorded_ = false;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<ArcMetricsService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcMetricsService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_
