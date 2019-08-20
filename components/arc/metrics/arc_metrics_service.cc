// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/metrics/arc_metrics_service.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/metrics/stability_metrics_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/exo/wm_helper.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "ui/events/ozone/gamepad/gamepad_provider_ozone.h"

namespace arc {

namespace {

constexpr base::TimeDelta kUmaMinTime = base::TimeDelta::FromMilliseconds(1);
constexpr base::TimeDelta kUmaMaxTime = base::TimeDelta::FromSeconds(60);
constexpr int kUmaNumBuckets = 50;

constexpr base::TimeDelta kRequestProcessListPeriod =
    base::TimeDelta::FromMinutes(5);
constexpr char kArcProcessNamePrefix[] = "org.chromium.arc.";
constexpr char kGmsProcessNamePrefix[] = "com.google.android.gms";
constexpr char kBootProgressEnableScreen[] = "boot_progress_enable_screen";

constexpr base::TimeDelta kUpdateEngagementTimePeriod =
    base::TimeDelta::FromMinutes(1);
constexpr base::TimeDelta kSaveEngagementTimeToPrefsPeriod =
    base::TimeDelta::FromMinutes(30);

std::string BootTypeToString(mojom::BootType boot_type) {
  switch (boot_type) {
    case mojom::BootType::UNKNOWN:
      break;
    case mojom::BootType::FIRST_BOOT:
      return ".FirstBoot";
    case mojom::BootType::FIRST_BOOT_AFTER_UPDATE:
      return ".FirstBootAfterUpdate";
    case mojom::BootType::REGULAR_BOOT:
      return ".RegularBoot";
  }
  NOTREACHED();
  return "";
}

inline int GetDayId(const base::Clock* clock) {
  return clock->Now().LocalMidnight().since_origin().InDays();
}

// Singleton factory for ArcMetricsService.
class ArcMetricsServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcMetricsService,
          ArcMetricsServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcMetricsServiceFactory";

  static ArcMetricsServiceFactory* GetInstance() {
    return base::Singleton<ArcMetricsServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcMetricsServiceFactory>;
  ArcMetricsServiceFactory() = default;
  ~ArcMetricsServiceFactory() override = default;
};

}  // namespace

// static
ArcMetricsService* ArcMetricsService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcMetricsServiceFactory::GetForBrowserContext(context);
}

// static
ArcMetricsService* ArcMetricsService::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcMetricsServiceFactory::GetForBrowserContextForTesting(context);
}

// static
BrowserContextKeyedServiceFactory* ArcMetricsService::GetFactory() {
  return ArcMetricsServiceFactory::GetInstance();
}

ArcMetricsService::ArcMetricsService(content::BrowserContext* context,
                                     ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      window_matcher_(base::BindRepeating(arc::IsArcAppWindow)),
      process_observer_(this),
      pref_service_(user_prefs::UserPrefs::Get(context)),
      clock_(base::DefaultClock::GetInstance()),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      last_update_ticks_(tick_clock_->NowTicks()) {
  arc_bridge_service_->metrics()->SetHost(this);
  arc_bridge_service_->process()->AddObserver(&process_observer_);
  // If WMHelper doesn't exist, do nothing. This occurs in tests.
  if (exo::WMHelper::HasInstance())
    exo::WMHelper::GetInstance()->AddActivationObserver(this);
  session_manager::SessionManager::Get()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  ui::GamepadProviderOzone::GetInstance()->AddGamepadObserver(this);

  DCHECK(pref_service_);
  RestoreEngagementTimeFromPrefs();
  update_engagement_time_timer_.Start(FROM_HERE, kUpdateEngagementTimePeriod,
                                      this,
                                      &ArcMetricsService::UpdateEngagementTime);
  save_engagement_time_to_prefs_timer_.Start(
      FROM_HERE, kSaveEngagementTimeToPrefsPeriod, this,
      &ArcMetricsService::SaveEngagementTimeToPrefs);

  StabilityMetricsManager::Get()->SetArcNativeBridgeType(
      NativeBridgeType::UNKNOWN);
}

ArcMetricsService::~ArcMetricsService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  save_engagement_time_to_prefs_timer_.Stop();
  update_engagement_time_timer_.Stop();
  UpdateEngagementTime();
  SaveEngagementTimeToPrefs();

  ui::GamepadProviderOzone::GetInstance()->RemoveGamepadObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  session_manager::SessionManager::Get()->RemoveObserver(this);
  // If WMHelper is already destroyed, do nothing.
  // TODO(crbug.com/748380): Fix shutdown order.
  if (exo::WMHelper::HasInstance())
    exo::WMHelper::GetInstance()->RemoveActivationObserver(this);
  arc_bridge_service_->process()->RemoveObserver(&process_observer_);
  arc_bridge_service_->metrics()->SetHost(nullptr);
}

void ArcMetricsService::SetWindowMatcherForTesting(
    WindowMatcher window_matcher) {
  window_matcher_ = window_matcher;
}

void ArcMetricsService::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void ArcMetricsService::SetTickClockForTesting(base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void ArcMetricsService::OnProcessConnectionReady() {
  VLOG(2) << "Start updating process list.";
  request_process_list_timer_.Start(FROM_HERE, kRequestProcessListPeriod, this,
                                    &ArcMetricsService::RequestProcessList);
}

void ArcMetricsService::OnProcessConnectionClosed() {
  VLOG(2) << "Stop updating process list.";
  request_process_list_timer_.Stop();
}

void ArcMetricsService::RequestProcessList() {
  mojom::ProcessInstance* process_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->process(), RequestProcessList);
  if (!process_instance)
    return;
  VLOG(2) << "RequestProcessList";
  process_instance->RequestProcessList(base::BindOnce(
      &ArcMetricsService::ParseProcessList, weak_ptr_factory_.GetWeakPtr()));
}

void ArcMetricsService::ParseProcessList(
    std::vector<mojom::RunningAppProcessInfoPtr> processes) {
  int running_app_count = 0;
  for (const auto& process : processes) {
    const std::string& process_name = process->process_name;
    const mojom::ProcessState& process_state = process->process_state;

    // Processes like the ARC launcher and intent helper are always running
    // and not counted as apps running by users. With the same reasoning,
    // GMS (Google Play Services) and its related processes are skipped as
    // well. The process_state check below filters out system processes,
    // services, apps that are cached because they've run before.
    if (base::StartsWith(process_name, kArcProcessNamePrefix,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(process_name, kGmsProcessNamePrefix,
                         base::CompareCase::SENSITIVE) ||
        process_state != mojom::ProcessState::TOP) {
      VLOG(2) << "Skipped " << process_name << " " << process_state;
    } else {
      ++running_app_count;
    }
  }

  UMA_HISTOGRAM_COUNTS_100("Arc.AppCount", running_app_count);
}

void ArcMetricsService::OnArcStartTimeRetrieved(
    std::vector<mojom::BootProgressEventPtr> events,
    mojom::BootType boot_type,
    base::Optional<base::TimeTicks> arc_start_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!arc_start_time.has_value()) {
    LOG(ERROR) << "Failed to retrieve ARC start timeticks.";
    return;
  }
  VLOG(2) << "ARC start @" << arc_start_time.value();

  DCHECK_NE(mojom::BootType::UNKNOWN, boot_type);
  const std::string suffix = BootTypeToString(boot_type);
  for (const auto& event : events) {
    VLOG(2) << "Report boot progress event:" << event->event << "@"
            << event->uptimeMillis;
    const std::string name = "Arc." + event->event + suffix;
    const base::TimeTicks uptime =
        base::TimeDelta::FromMilliseconds(event->uptimeMillis) +
        base::TimeTicks();
    const base::TimeDelta elapsed_time = uptime - arc_start_time.value();
    base::UmaHistogramCustomTimes(name, elapsed_time, kUmaMinTime, kUmaMaxTime,
                                  kUmaNumBuckets);
    if (event->event.compare(kBootProgressEnableScreen) == 0) {
      base::UmaHistogramCustomTimes("Arc.AndroidBootTime" + suffix,
                                    elapsed_time, kUmaMinTime, kUmaMaxTime,
                                    kUmaNumBuckets);
    }
  }
}

void ArcMetricsService::ReportBootProgress(
    std::vector<mojom::BootProgressEventPtr> events,
    mojom::BootType boot_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (boot_type == mojom::BootType::UNKNOWN) {
    LOG(WARNING) << "boot_type is unknown. Skip recording UMA.";
    return;
  }

  if (IsArcVmEnabled()) {
    // For VM builds, do not call into session_manager since we don't use it
    // for the builds. Using base::TimeTicks() is fine for now because 1) the
    // clocks in host and guest are not synchronized, and 2) the guest does not
    // support mini container.
    // TODO(yusukes): Once the guest supports mini container (details TBD), we
    // should have the guest itself report the timing of the upgrade.
    OnArcStartTimeRetrieved(std::move(events), boot_type, base::TimeTicks());
    return;
  }

  // Retrieve ARC full container's start time from session manager.
  chromeos::SessionManagerClient::Get()->GetArcStartTime(base::BindOnce(
      &ArcMetricsService::OnArcStartTimeRetrieved,
      weak_ptr_factory_.GetWeakPtr(), std::move(events), boot_type));
}

void ArcMetricsService::ReportNativeBridge(
    mojom::NativeBridgeType mojo_native_bridge_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << "Mojo native bridge type is " << mojo_native_bridge_type;

  NativeBridgeType native_bridge_type = NativeBridgeType::UNKNOWN;
  switch (mojo_native_bridge_type) {
    case mojom::NativeBridgeType::NONE:
      native_bridge_type = NativeBridgeType::NONE;
      break;
    case mojom::NativeBridgeType::HOUDINI:
      native_bridge_type = NativeBridgeType::HOUDINI;
      break;
    case mojom::NativeBridgeType::NDK_TRANSLATION:
      native_bridge_type = NativeBridgeType::NDK_TRANSLATION;
      break;
  }
  DCHECK_NE(native_bridge_type, NativeBridgeType::UNKNOWN)
      << mojo_native_bridge_type;

  StabilityMetricsManager::Get()->SetArcNativeBridgeType(native_bridge_type);
}

void ArcMetricsService::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  UpdateEngagementTime();
  was_arc_window_active_ = window_matcher_.Run(gained_active);
  if (!was_arc_window_active_) {
    gamepad_interaction_recorded_ = false;
    return;
  }
  UMA_HISTOGRAM_ENUMERATION(
      "Arc.UserInteraction",
      UserInteractionType::APP_CONTENT_WINDOW_INTERACTION);
}

void ArcMetricsService::OnSessionStateChanged() {
  UpdateEngagementTime();
  was_session_active_ =
      session_manager::SessionManager::Get()->session_state() ==
      session_manager::SessionState::ACTIVE;
}

void ArcMetricsService::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& proto) {
  UpdateEngagementTime();
  was_screen_dimmed_ = proto.dimmed();
}

void ArcMetricsService::OnGamepadEvent(const ui::GamepadEvent& event) {
  if (!was_arc_window_active_)
    return;
  if (gamepad_interaction_recorded_)
    return;
  gamepad_interaction_recorded_ = true;
  UMA_HISTOGRAM_ENUMERATION("Arc.UserInteraction",
                            UserInteractionType::GAMEPAD_INTERACTION);
}

void ArcMetricsService::OnTaskCreated(int32_t task_id,
                                      const std::string& package_name,
                                      const std::string& activity,
                                      const std::string& intent) {
  UpdateEngagementTime();
  task_ids_.push_back(task_id);
}

void ArcMetricsService::OnTaskDestroyed(int32_t task_id) {
  UpdateEngagementTime();
  auto it = std::find(task_ids_.begin(), task_ids_.end(), task_id);
  if (it == task_ids_.end()) {
    LOG(WARNING) << "unknown task_id, background time might be undermeasured";
    return;
  }
  task_ids_.erase(it);
}

void ArcMetricsService::RestoreEngagementTimeFromPrefs() {
  // Restore accumulated results only if they were recorded on the same OS
  // version.
  if (pref_service_->GetString(prefs::kEngagementTimeOsVersion) ==
      base::SysInfo::OperatingSystemVersion()) {
    day_id_ = pref_service_->GetInteger(prefs::kEngagementTimeDayId);
    engagement_time_total_ =
        pref_service_->GetTimeDelta(prefs::kEngagementTimeTotal);
    engagement_time_foreground_ =
        pref_service_->GetTimeDelta(prefs::kEngagementTimeForeground);
    engagement_time_background_ =
        pref_service_->GetTimeDelta(prefs::kEngagementTimeBackground);
  } else {
    ResetEngagementTimePrefs();
  }

  RecordEngagementTimeToUmaIfNeeded();
}

void ArcMetricsService::SaveEngagementTimeToPrefs() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(pref_service_);

  pref_service_->SetString(prefs::kEngagementTimeOsVersion,
                           base::SysInfo::OperatingSystemVersion());
  pref_service_->SetInteger(prefs::kEngagementTimeDayId, day_id_);
  pref_service_->SetTimeDelta(prefs::kEngagementTimeTotal,
                              engagement_time_total_);
  pref_service_->SetTimeDelta(prefs::kEngagementTimeForeground,
                              engagement_time_foreground_);
  pref_service_->SetTimeDelta(prefs::kEngagementTimeBackground,
                              engagement_time_background_);
}

void ArcMetricsService::UpdateEngagementTime() {
  VLOG(2) << "last state: dimmed=" << was_screen_dimmed_
          << " active=" << was_session_active_
          << " focus=" << was_arc_window_active_
          << " #tasks=" << task_ids_.size();

  base::TimeTicks now = tick_clock_->NowTicks();
  base::TimeDelta elapsed = now - last_update_ticks_;

  if (ShouldAccumulateEngagementTotalTime()) {
    VLOG(2) << "accumulate to total time " << elapsed;
    engagement_time_total_ += elapsed;
    if (ShouldAccumulateEngagementForegroundTime()) {
      VLOG(2) << "accumulate to foreground time " << elapsed;
      engagement_time_foreground_ += elapsed;
    } else if (ShouldAccumulateEngagementBackgroundTime()) {
      VLOG(2) << "accumulate to background time " << elapsed;
      engagement_time_background_ += elapsed;
    }
  }

  last_update_ticks_ = now;
  RecordEngagementTimeToUmaIfNeeded();
}

void ArcMetricsService::RecordEngagementTimeToUmaIfNeeded() {
  if (!ShouldRecordEngagementTimeToUma())
    return;
  VLOG(2) << "day changed, recording engagement time to UMA";
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Arc.EngagementTime.Total", engagement_time_total_,
      base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromDays(1) + kUpdateEngagementTimePeriod, 50);
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Arc.EngagementTime.ArcTotal",
      engagement_time_foreground_ + engagement_time_background_,
      base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromDays(1) + kUpdateEngagementTimePeriod, 50);
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Arc.EngagementTime.Foreground", engagement_time_foreground_,
      base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromDays(1) + kUpdateEngagementTimePeriod, 50);
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Arc.EngagementTime.Background", engagement_time_background_,
      base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromDays(1) + kUpdateEngagementTimePeriod, 50);
  ResetEngagementTimePrefs();
}

void ArcMetricsService::ResetEngagementTimePrefs() {
  day_id_ = GetDayId(clock_);
  engagement_time_total_ = base::TimeDelta();
  engagement_time_foreground_ = base::TimeDelta();
  engagement_time_background_ = base::TimeDelta();
  SaveEngagementTimeToPrefs();
}

bool ArcMetricsService::ShouldAccumulateEngagementTotalTime() const {
  return was_session_active_ && !was_screen_dimmed_;
}

bool ArcMetricsService::ShouldAccumulateEngagementForegroundTime() const {
  return was_arc_window_active_;
}

bool ArcMetricsService::ShouldAccumulateEngagementBackgroundTime() const {
  return task_ids_.size() > 0;
}

bool ArcMetricsService::ShouldRecordEngagementTimeToUma() const {
  return day_id_ != GetDayId(clock_);
}

ArcMetricsService::ProcessObserver::ProcessObserver(
    ArcMetricsService* arc_metrics_service)
    : arc_metrics_service_(arc_metrics_service) {}

ArcMetricsService::ProcessObserver::~ProcessObserver() = default;

void ArcMetricsService::ProcessObserver::OnConnectionReady() {
  arc_metrics_service_->OnProcessConnectionReady();
}

void ArcMetricsService::ProcessObserver::OnConnectionClosed() {
  arc_metrics_service_->OnProcessConnectionClosed();
}

}  // namespace arc
