// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_controller.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/kiosk_next/kiosk_next_shell_controller_impl.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/fps_counter.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/tablet_mode_toggle_observer.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/internal_input_devices_event_blocker.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/window_state.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The hinge angle at which to enter tablet mode.
constexpr float kEnterTabletModeAngle = 200.0f;

// The angle at which to exit tablet mode, this is specifically less than the
// angle to enter tablet mode to prevent rapid toggling when near the angle.
constexpr float kExitTabletModeAngle = 160.0f;

// Defines a range for which accelerometer readings are considered accurate.
// When the lid is near open (or near closed) the accelerometer readings may be
// inaccurate and a lid that is fully open may appear to be near closed (and
// vice versa).
constexpr float kMinStableAngle = 20.0f;
constexpr float kMaxStableAngle = 340.0f;

// The time duration to consider an unstable lid angle to be valid. This is used
// to prevent entering tablet mode if an erroneous accelerometer reading makes
// the lid appear to be fully open when the user is opening the lid from a
// closed position or is closing the lid from an opened position.
constexpr base::TimeDelta kUnstableLidAngleDuration =
    base::TimeDelta::FromSeconds(2);

// When the device approaches vertical orientation (i.e. portrait orientation)
// the accelerometers for the base and lid approach the same values (i.e.
// gravity pointing in the direction of the hinge). When this happens abrupt
// small acceleration perpendicular to the hinge can lead to incorrect hinge
// angle calculations. To prevent this the accelerometer updates will be
// smoothed over time in order to reduce this noise.
// This is the minimum acceleration parallel to the hinge under which to begin
// smoothing in m/s^2.
constexpr float kHingeVerticalSmoothingStart = 7.0f;
// This is the maximum acceleration parallel to the hinge under which smoothing
// will incorporate new acceleration values, in m/s^2.
constexpr float kHingeVerticalSmoothingMaximum = 8.7f;

// The maximum deviation between the magnitude of the two accelerometers under
// which to detect hinge angle in m/s^2. These accelerometers are attached to
// the same physical device and so should be under the same acceleration.
constexpr float kNoisyMagnitudeDeviation = 1.0f;

// Interval between calls to RecordLidAngle().
constexpr base::TimeDelta kRecordLidAngleInterval =
    base::TimeDelta::FromHours(1);

// Time that should wait to reset |occlusion_tracker_pauser_| on
// entering/exiting tablet mode.
constexpr base::TimeDelta kOcclusionTrackerTimeout =
    base::TimeDelta::FromMilliseconds(500);

// Histogram names for recording animation smoothness when entering or exiting
// tablet mode.
constexpr char kTabletModeEnterHistogram[] =
    "Ash.TabletMode.AnimationSmoothness.Enter";
constexpr char kTabletModeExitHistogram[] =
    "Ash.TabletMode.AnimationSmoothness.Exit";

// Set to true for unit tests so tablet mode can be changed synchronously.
bool force_no_screenshot = false;

// The angle between AccelerometerReadings are considered stable if
// their magnitudes do not differ greatly. This returns false if the deviation
// between the screen and keyboard accelerometers is too high.
bool IsAngleBetweenAccelerometerReadingsStable(
    const AccelerometerUpdate& update) {
  return std::abs(
             update.GetVector(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD).Length() -
             update.GetVector(ACCELEROMETER_SOURCE_SCREEN).Length()) <=
         kNoisyMagnitudeDeviation;
}

bool ShouldInitTabletModeController() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshEnableTabletMode);
}

// Checks the command line to see which force tablet mode is turned on, if
// any.
TabletModeController::UiMode GetTabletMode() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshUiMode)) {
    std::string switch_value =
        command_line->GetSwitchValueASCII(switches::kAshUiMode);
    if (switch_value == switches::kAshUiModeClamshell)
      return TabletModeController::UiMode::kClamshell;

    if (switch_value == switches::kAshUiModeTablet)
      return TabletModeController::UiMode::kTabletMode;
  }
  return TabletModeController::UiMode::kNone;
}

// Returns true if the device has an active internal display.
bool HasActiveInternalDisplay() {
  return display::Display::HasInternalDisplay() &&
         Shell::Get()->display_manager()->IsActiveDisplayId(
             display::Display::InternalDisplayId());
}

bool IsTransformAnimationSequence(ui::LayerAnimationSequence* sequence) {
  DCHECK(sequence);
  return sequence->properties() & ui::LayerAnimationElement::TRANSFORM;
}

std::unique_ptr<ui::Layer> CreateLayerFromScreenshotResult(
    std::unique_ptr<viz::CopyOutputResult> copy_result) {
  DCHECK(!copy_result->IsEmpty());
  DCHECK_EQ(copy_result->format(), viz::CopyOutputResult::Format::RGBA_TEXTURE);

  const gfx::Size layer_size = copy_result->size();
  viz::TransferableResource transferable_resource =
      viz::TransferableResource::MakeGL(
          copy_result->GetTextureResult()->mailbox, GL_LINEAR, GL_TEXTURE_2D,
          copy_result->GetTextureResult()->sync_token, layer_size,
          /*is_overlay_candidate=*/false);
  std::unique_ptr<viz::SingleReleaseCallback> release_callback =
      copy_result->TakeTextureOwnership();
  auto screenshot_layer = std::make_unique<ui::Layer>();
  screenshot_layer->SetTransferableResource(
      transferable_resource, std::move(release_callback), layer_size);

  return screenshot_layer;
}

}  // namespace

// Class which records animation smoothness when entering or exiting tablet
// mode. No stats should be recorded if no windows are animated.
class TabletModeController::TabletModeTransitionFpsCounter : public FpsCounter {
 public:
  TabletModeTransitionFpsCounter(ui::Compositor* compositor,
                                 bool enter_tablet_mode)
      : FpsCounter(compositor), enter_tablet_mode_(enter_tablet_mode) {}
  ~TabletModeTransitionFpsCounter() override = default;

  void LogUma() {
    int smoothness = ComputeSmoothness();
    if (smoothness < 0)
      return;

    if (enter_tablet_mode_)
      UMA_HISTOGRAM_PERCENTAGE(kTabletModeEnterHistogram, smoothness);
    else
      UMA_HISTOGRAM_PERCENTAGE(kTabletModeExitHistogram, smoothness);
  }

  bool enter_tablet_mode() const { return enter_tablet_mode_; }

 private:
  bool enter_tablet_mode_;
  DISALLOW_COPY_AND_ASSIGN(TabletModeTransitionFpsCounter);
};

constexpr char TabletModeController::kLidAngleHistogramName[];

TabletModeController::TabletModeController()
    : event_blocker_(std::make_unique<InternalInputDevicesEventBlocker>()),
      tablet_mode_usage_interval_start_time_(base::Time::Now()),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  Shell::Get()->AddShellObserver(this);
  base::RecordAction(base::UserMetricsAction("Touchview_Initially_Disabled"));

  // TODO(jonross): Do not create TabletModeController if the flag is
  // unavailable. This will require refactoring
  // IsTabletModeWindowManagerEnabled to check for the existence of the
  // controller.
  if (ShouldInitTabletModeController()) {
    Shell::Get()->window_tree_host_manager()->AddObserver(this);
    AccelerometerReader::GetInstance()->AddObserver(this);
    ui::DeviceDataManager::GetInstance()->AddObserver(this);
    bluetooth_devices_observer_ =
        std::make_unique<BluetoothDevicesObserver>(base::BindRepeating(
            &TabletModeController::OnBluetoothAdapterOrDeviceChanged,
            base::Unretained(this)));
  }

  Shell::Get()->kiosk_next_shell_controller()->AddObserver(this);

  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  power_manager_client->AddObserver(this);
  power_manager_client->GetSwitchStates(base::BindOnce(
      &TabletModeController::OnGetSwitchStates, weak_factory_.GetWeakPtr()));
}

TabletModeController::~TabletModeController() {
  if (tablet_mode_window_manager_)
    tablet_mode_window_manager_->Shutdown();

  UMA_HISTOGRAM_COUNTS_1000("Tablet.AppWindowDrag.CountOfPerUserSession",
                            app_window_drag_count_);
  UMA_HISTOGRAM_COUNTS_1000(
      "Tablet.AppWindowDrag.InSplitView.CountOfPerUserSession",
      app_window_drag_in_splitview_count_);
  UMA_HISTOGRAM_COUNTS_1000("Tablet.TabDrag.CountOfPerUserSession",
                            tab_drag_count_);
  UMA_HISTOGRAM_COUNTS_1000("Tablet.TabDrag.InSplitView.CountOfPerUserSession",
                            tab_drag_in_splitview_count_);

  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->kiosk_next_shell_controller()->RemoveObserver(this);

  if (ShouldInitTabletModeController()) {
    Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
    AccelerometerReader::GetInstance()->RemoveObserver(this);
    ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
  }
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);

  for (auto& observer : tablet_mode_observers_)
    observer.OnTabletControllerDestroyed();
}

// static
void TabletModeController::SetForceNoScreenshotForTest() {
  force_no_screenshot = true;
}

// TODO(jcliang): Hide or remove EnableTabletModeWindowManager
// (http://crbug.com/620241).
void TabletModeController::EnableTabletModeWindowManager(bool should_enable) {
  bool is_enabled = IsTabletModeWindowManagerEnabled();
  if (should_enable == is_enabled)
    return;

  // Hide the context menu on entering tablet mode to prevent users from
  // accessing forbidden options. Hide the context menu on exiting tablet mode
  // to match behaviors.
  for (auto* root_window : Shell::Get()->GetAllRootWindows())
    RootWindowController::ForWindow(root_window)->HideContextMenu();

  // Suspend occlusion tracker when entering or exiting tablet mode.
  SuspendOcclusionTracker();
  DeleteScreenshot();

  if (should_enable) {
    state_ = State::kEnteringTabletMode;

    // Take a screenshot if there is a top window that will get animated.
    // TODO(sammiequon): Handle the case where the top window is not on the
    // primary display.
    aura::Window* top_window = TabletModeWindowManager::GetTopWindow();
    bool top_window_on_primary_display =
        top_window &&
        top_window->GetRootWindow() == Shell::GetPrimaryRootWindow();
    if (!force_no_screenshot && top_window_on_primary_display) {
      screenshot_set_callback_.Reset(
          base::BindOnce(&TabletModeController::FinishInitTabletMode,
                         weak_factory_.GetWeakPtr()));
      TakeScreenshot(top_window, screenshot_set_callback_.callback());
    } else {
      FinishInitTabletMode();
    }
  } else {
    state_ = State::kExitingTabletMode;

    tablet_mode_window_manager_->SetIgnoreWmEventsForExit();
    for (auto& observer : tablet_mode_observers_)
      observer.OnTabletModeEnding();
    tablet_mode_window_manager_->Shutdown();
    tablet_mode_window_manager_.reset();
    base::RecordAction(base::UserMetricsAction("Touchview_Disabled"));
    RecordTabletModeUsageInterval(TABLET_MODE_INTERVAL_ACTIVE);
    for (auto& observer : tablet_mode_observers_)
      observer.OnTabletModeEnded();

    state_ = State::kInClamshellMode;
    if (toggle_observer_)  // Null at startup and in tests.
      toggle_observer_->OnTabletModeToggled(false);
    VLOG(1) << "Exit tablet mode.";
  }

  UpdateInternalInputDevicesEventBlocker();
}

bool TabletModeController::IsTabletModeWindowManagerEnabled() const {
  return !!tablet_mode_window_manager_;
}

void TabletModeController::AddWindow(aura::Window* window) {
  if (IsTabletModeWindowManagerEnabled())
    tablet_mode_window_manager_->AddWindow(window);
}

void TabletModeController::AddObserver(TabletModeObserver* observer) {
  tablet_mode_observers_.AddObserver(observer);
}

void TabletModeController::RemoveObserver(TabletModeObserver* observer) {
  tablet_mode_observers_.RemoveObserver(observer);
}

bool TabletModeController::ShouldAutoHideTitlebars(views::Widget* widget) {
  DCHECK(widget);
  const bool tablet_mode = IsTabletModeWindowManagerEnabled();
  if (!tablet_mode)
    return false;

  return widget->IsMaximized() ||
         wm::GetWindowState(widget->GetNativeWindow())->IsSnapped();
}

bool TabletModeController::AreInternalInputDeviceEventsBlocked() const {
  return event_blocker_->should_be_blocked();
}

bool TabletModeController::TriggerRecordLidAngleTimerForTesting() {
  if (!record_lid_angle_timer_.IsRunning())
    return false;

  record_lid_angle_timer_.user_task().Run();
  return true;
}

void TabletModeController::MaybeObserveBoundsAnimation(aura::Window* window) {
  StopObservingAnimation(/*record_stats=*/false, /*delete_screenshot=*/false);

  if (state_ != State::kEnteringTabletMode &&
      state_ != State::kExitingTabletMode) {
    return;
  }

  observed_window_ = window;
  observed_layer_ = window->layer();
  window->AddObserver(this);
  observed_layer_->GetAnimator()->AddObserver(this);
}

void TabletModeController::StopObservingAnimation(bool record_stats,
                                                  bool delete_screenshot) {
  StopObserving();

  if (observed_layer_)
    observed_layer_->GetAnimator()->RemoveObserver(this);
  observed_layer_ = nullptr;
  if (observed_window_)
    observed_window_->RemoveObserver(this);
  observed_window_ = nullptr;
  if (record_stats && fps_counter_)
    fps_counter_->LogUma();
  fps_counter_.reset();

  if (delete_screenshot)
    DeleteScreenshot();
}

void TabletModeController::SetTabletModeToggleObserver(
    TabletModeToggleObserver* observer) {
  DCHECK(observer);
  DCHECK(!toggle_observer_);
  toggle_observer_ = observer;
}

bool TabletModeController::IsEnabled() const {
  return IsTabletModeWindowManagerEnabled();
}

void TabletModeController::SetEnabledForTest(bool enabled) {
  // Disable Accelerometer and PowerManagerClient observers to prevent possible
  // tablet mode overrides. It won't be possible to physically switch to/from
  // tablet mode after calling this function. This is needed for tests that
  // run on DUTs and require switching to/back tablet mode in runtime, like some
  // ARC++ Tast tests.
  AccelerometerReader::GetInstance()->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  EnableTabletModeWindowManager(enabled);
}

void TabletModeController::OnShellInitialized() {
  force_ui_mode_ = GetTabletMode();
  if (force_ui_mode_ == UiMode::kTabletMode)
    AttemptEnterTabletMode();
}

void TabletModeController::OnDisplayConfigurationChanged() {
  if (!AllowUiModeChange())
    return;

  if (!HasActiveInternalDisplay()) {
    AttemptLeaveTabletMode();
  } else if (tablet_mode_switch_is_on_ && !IsTabletModeWindowManagerEnabled()) {
    // The internal display has returned, as we are exiting docked mode.
    // The device is still in tablet mode, so trigger tablet mode, as this
    // switch leads to the ignoring of accelerometer events. When the switch is
    // not set the next stable accelerometer readings will trigger maximize
    // mode.
    AttemptEnterTabletMode();
  }
}

void TabletModeController::OnChromeTerminating() {
  // The system is about to shut down, so record TabletMode usage interval
  // metrics based on whether TabletMode mode is currently active.
  RecordTabletModeUsageInterval(CurrentTabletModeIntervalType());

  if (CanEnterTabletMode()) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.TouchView.TouchViewActiveTotal",
                                total_tablet_mode_time_.InMinutes(), 1,
                                base::TimeDelta::FromDays(7).InMinutes(), 50);
    UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.TouchView.TouchViewInactiveTotal",
                                total_non_tablet_mode_time_.InMinutes(), 1,
                                base::TimeDelta::FromDays(7).InMinutes(), 50);
    base::TimeDelta total_runtime =
        total_tablet_mode_time_ + total_non_tablet_mode_time_;
    if (total_runtime.InSeconds() > 0) {
      UMA_HISTOGRAM_PERCENTAGE("Ash.TouchView.TouchViewActivePercentage",
                               100 * total_tablet_mode_time_.InSeconds() /
                                   total_runtime.InSeconds());
    }
  }
}

void TabletModeController::OnAccelerometerUpdated(
    scoped_refptr<const AccelerometerUpdate> update) {
  if (!AllowUiModeChange())
    return;

  // When ChromeOS EC lid angle driver is present, EC can handle lid angle
  // calculation, thus Chrome side lid angle calculation is disabled. In this
  // case, TabletModeController no longer listens to accelerometer events.
  if (update->HasLidAngleDriver(ACCELEROMETER_SOURCE_SCREEN) ||
      update->HasLidAngleDriver(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD)) {
    AccelerometerReader::GetInstance()->RemoveObserver(this);
    return;
  }

  have_seen_accelerometer_data_ = true;
  can_detect_lid_angle_ = update->has(ACCELEROMETER_SOURCE_SCREEN) &&
                          update->has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD);
  if (!can_detect_lid_angle_) {
    if (record_lid_angle_timer_.IsRunning())
      record_lid_angle_timer_.Stop();
    return;
  }

  if (!HasActiveInternalDisplay())
    return;

  // Whether or not we enter tablet mode affects whether we handle screen
  // rotation, so determine whether to enter tablet mode first.
  if (update->IsReadingStable(ACCELEROMETER_SOURCE_SCREEN) &&
      update->IsReadingStable(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD) &&
      IsAngleBetweenAccelerometerReadingsStable(*update)) {
    // update.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD)
    // Ignore the reading if it appears unstable. The reading is considered
    // unstable if it deviates too much from gravity and/or the magnitude of the
    // reading from the lid differs too much from the reading from the base.
    HandleHingeRotation(update);
  }
}

void TabletModeController::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    const base::TimeTicks& time) {
  if (!AllowUiModeChange())
    return;

  VLOG(1) << "Lid event received: " << static_cast<int>(state);
  const bool open = state == chromeos::PowerManagerClient::LidState::OPEN;
  lid_is_closed_ = !open;

  if (!tablet_mode_switch_is_on_)
    AttemptLeaveTabletMode();
}

void TabletModeController::TabletModeEventReceived(
    chromeos::PowerManagerClient::TabletMode mode,
    const base::TimeTicks& time) {
  if (!AllowUiModeChange())
    return;

  VLOG(1) << "Tablet mode event received: " << static_cast<int>(mode);
  const bool on = mode == chromeos::PowerManagerClient::TabletMode::ON;
  tablet_mode_switch_is_on_ = on;

  // Do not change if docked.
  if (!HasActiveInternalDisplay())
    return;

  // For updated EC, the tablet mode switch activates at 200 degrees, and
  // deactivates at 160 degrees.
  // For old EC, the tablet mode switch activates at 300 degrees, so it's
  // always reliable when |on|. However we wish to exit tablet mode at a
  // smaller angle, so when |on| is false we ignore if it is possible to
  // calculate the lid angle.
  if (on && !IsTabletModeWindowManagerEnabled()) {
    AttemptEnterTabletMode();
  } else if (!on && IsTabletModeWindowManagerEnabled() &&
             !can_detect_lid_angle_) {
    AttemptLeaveTabletMode();
  }

  // Even if we do not change its ui mode, we should update its input device
  // blocker as tablet mode events may come in because of the lid angle/or folio
  // keyboard state changes but ui mode might still stay the same.
  UpdateInternalInputDevicesEventBlocker();
}

void TabletModeController::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  // The system is about to suspend, so record TabletMode usage interval metrics
  // based on whether TabletMode mode is currently active.
  RecordTabletModeUsageInterval(CurrentTabletModeIntervalType());

  // Stop listening to any incoming input device changes during suspend as the
  // input devices may be removed during suspend and cause the device enter/exit
  // tablet mode unexpectedly.
  if (ShouldInitTabletModeController()) {
    ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
    bluetooth_devices_observer_.reset();
  }
}

void TabletModeController::SuspendDone(const base::TimeDelta& sleep_duration) {
  // We do not want TabletMode usage metrics to include time spent in suspend.
  tablet_mode_usage_interval_start_time_ = base::Time::Now();

  // Start listening to the input device changes again.
  if (ShouldInitTabletModeController()) {
    bluetooth_devices_observer_ =
        std::make_unique<BluetoothDevicesObserver>(base::BindRepeating(
            &TabletModeController::OnBluetoothAdapterOrDeviceChanged,
            base::Unretained(this)));
    ui::DeviceDataManager::GetInstance()->AddObserver(this);
    // Call HandlePointingDeviceAddedOrRemoved() to iterate all available input
    // devices just in case we have missed all the notifications from
    // DeviceDataManager and  BluetoothDevicesObserver when SuspendDone() is
    // called.
    HandlePointingDeviceAddedOrRemoved();
  }
}

void TabletModeController::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if (input_device_types & (ui::InputDeviceEventObserver::kMouse |
                            ui::InputDeviceEventObserver::kTouchpad)) {
    if (input_device_types & ui::InputDeviceEventObserver::kMouse)
      VLOG(1) << "Mouse device configuration changed.";
    if (input_device_types & ui::InputDeviceEventObserver::kTouchpad)
      VLOG(1) << "Touchpad device configuration changed.";
    HandlePointingDeviceAddedOrRemoved();
  }
}

void TabletModeController::OnDeviceListsComplete() {
  HandlePointingDeviceAddedOrRemoved();
}

void TabletModeController::OnKioskNextEnabled() {
  kiosk_next_enabled_ = true;
  AttemptEnterTabletMode();
}

void TabletModeController::OnLayerAnimationStarted(
    ui::LayerAnimationSequence* sequence) {}

void TabletModeController::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  if (!fps_counter_ || !IsTransformAnimationSequence(sequence))
    return;

  StopObservingAnimation(/*record_stats=*/false, /*delete_screenshot=*/true);
}

void TabletModeController::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  if (!fps_counter_ || !IsTransformAnimationSequence(sequence))
    return;

  StopObservingAnimation(/*record_stats=*/true, /*delete_screenshot=*/true);
}

void TabletModeController::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {
  if (!IsTransformAnimationSequence(sequence))
    return;

  if (!fps_counter_) {
    fps_counter_ = std::make_unique<TabletModeTransitionFpsCounter>(
        observed_layer_->GetCompositor(), state_ == State::kEnteringTabletMode);
    return;
  }

  // If another animation is scheduled while the animation we were originally
  // watching is still animating, abort and do not log stats as the stats will
  // not be accurate.
  StopObservingAnimation(/*record_stats=*/false, /*delete_screenshot=*/true);
}

void TabletModeController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(observed_window_, window);
  StopObservingAnimation(/*record_stats=*/false, /*delete_screenshot=*/true);
}

void TabletModeController::HandleHingeRotation(
    scoped_refptr<const AccelerometerUpdate> update) {
  static const gfx::Vector3dF hinge_vector(1.0f, 0.0f, 0.0f);
  gfx::Vector3dF base_reading =
      update->GetVector(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD);
  gfx::Vector3dF lid_reading = update->GetVector(ACCELEROMETER_SOURCE_SCREEN);

  // As the hinge approaches a vertical angle, the base and lid accelerometers
  // approach the same values making any angle calculations highly inaccurate.
  // Smooth out instantaneous acceleration when nearly vertical to increase
  // accuracy.
  float largest_hinge_acceleration =
      std::max(std::abs(base_reading.x()), std::abs(lid_reading.x()));
  float smoothing_ratio =
      std::max(0.0f, std::min(1.0f, (largest_hinge_acceleration -
                                     kHingeVerticalSmoothingStart) /
                                        (kHingeVerticalSmoothingMaximum -
                                         kHingeVerticalSmoothingStart)));

  // We cannot trust the computed lid angle when the device is held vertically.
  bool is_angle_reliable =
      largest_hinge_acceleration <= kHingeVerticalSmoothingMaximum;

  base_smoothed_.Scale(smoothing_ratio);
  base_reading.Scale(1.0f - smoothing_ratio);
  base_smoothed_.Add(base_reading);

  lid_smoothed_.Scale(smoothing_ratio);
  lid_reading.Scale(1.0f - smoothing_ratio);
  lid_smoothed_.Add(lid_reading);

  if (tablet_mode_switch_is_on_)
    return;

  // Ignore the component of acceleration parallel to the hinge for the purposes
  // of hinge angle calculation.
  gfx::Vector3dF base_flattened(base_smoothed_);
  gfx::Vector3dF lid_flattened(lid_smoothed_);
  base_flattened.set_x(0.0f);
  lid_flattened.set_x(0.0f);

  // Compute the angle between the base and the lid.
  lid_angle_ = 180.0f - gfx::ClockwiseAngleBetweenVectorsInDegrees(
                            base_flattened, lid_flattened, hinge_vector);
  if (lid_angle_ < 0.0f)
    lid_angle_ += 360.0f;

  bool is_angle_stable = is_angle_reliable && lid_angle_ >= kMinStableAngle &&
                         lid_angle_ <= kMaxStableAngle;

  if (is_angle_stable) {
    // Reset the timestamp of first unstable lid angle because we get a stable
    // reading.
    first_unstable_lid_angle_time_ = base::TimeTicks();
  } else if (first_unstable_lid_angle_time_.is_null()) {
    first_unstable_lid_angle_time_ = tick_clock_->NowTicks();
  }

  // Toggle tablet mode on or off when corresponding thresholds are passed.
  if (is_angle_stable && lid_angle_ <= kExitTabletModeAngle) {
    AttemptLeaveTabletMode();
  } else if (!lid_is_closed_ && lid_angle_ >= kEnterTabletModeAngle &&
             (is_angle_stable || CanUseUnstableLidAngle())) {
    AttemptEnterTabletMode();
  }

  // Start reporting the lid angle if we aren't already doing so.
  if (!record_lid_angle_timer_.IsRunning()) {
    record_lid_angle_timer_.Start(
        FROM_HERE, kRecordLidAngleInterval,
        base::BindRepeating(&TabletModeController::RecordLidAngle,
                            base::Unretained(this)));
  }
}

void TabletModeController::OnGetSwitchStates(
    base::Optional<chromeos::PowerManagerClient::SwitchStates> result) {
  if (!result.has_value())
    return;

  if (AccelerometerReader::GetInstance()->is_disabled())
    return;

  LidEventReceived(result->lid_state, base::TimeTicks::Now());
  TabletModeEventReceived(result->tablet_mode, base::TimeTicks::Now());
}

bool TabletModeController::CanUseUnstableLidAngle() const {
  DCHECK(!first_unstable_lid_angle_time_.is_null());

  const base::TimeTicks now = tick_clock_->NowTicks();
  DCHECK(now >= first_unstable_lid_angle_time_);
  const base::TimeDelta elapsed_time = now - first_unstable_lid_angle_time_;
  return elapsed_time >= kUnstableLidAngleDuration;
}

bool TabletModeController::CanEnterTabletMode() {
  // If we have ever seen accelerometer data, then HandleHingeRotation may
  // trigger tablet mode at some point in the future.
  // All TabletMode-enabled devices can enter tablet mode.
  return have_seen_accelerometer_data_ || IsEnabled();
}

void TabletModeController::AttemptEnterTabletMode() {
  if (IsTabletModeWindowManagerEnabled() || has_external_pointing_device_) {
    UpdateInternalInputDevicesEventBlocker();
    return;
  }

  EnableTabletModeWindowManager(true);
}

void TabletModeController::AttemptLeaveTabletMode() {
  if (!IsTabletModeWindowManagerEnabled()) {
    UpdateInternalInputDevicesEventBlocker();
    return;
  }

  EnableTabletModeWindowManager(false);
}

void TabletModeController::RecordTabletModeUsageInterval(
    TabletModeIntervalType type) {
  if (!CanEnterTabletMode())
    return;

  base::Time current_time = base::Time::Now();
  base::TimeDelta delta = current_time - tablet_mode_usage_interval_start_time_;
  switch (type) {
    case TABLET_MODE_INTERVAL_INACTIVE:
      UMA_HISTOGRAM_LONG_TIMES("Ash.TouchView.TouchViewInactive", delta);
      total_non_tablet_mode_time_ += delta;
      break;
    case TABLET_MODE_INTERVAL_ACTIVE:
      UMA_HISTOGRAM_LONG_TIMES("Ash.TouchView.TouchViewActive", delta);
      total_tablet_mode_time_ += delta;
      break;
  }

  tablet_mode_usage_interval_start_time_ = current_time;
}

void TabletModeController::RecordLidAngle() {
  DCHECK(can_detect_lid_angle_);
  base::LinearHistogram::FactoryGet(
      kLidAngleHistogramName, 1 /* minimum */, 360 /* maximum */,
      50 /* bucket_count */, base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(std::round(lid_angle_));
}

TabletModeController::TabletModeIntervalType
TabletModeController::CurrentTabletModeIntervalType() {
  if (IsTabletModeWindowManagerEnabled())
    return TABLET_MODE_INTERVAL_ACTIVE;
  return TABLET_MODE_INTERVAL_INACTIVE;
}

bool TabletModeController::AllowUiModeChange() const {
  return force_ui_mode_ == UiMode::kNone && !kiosk_next_enabled_;
}

void TabletModeController::HandlePointingDeviceAddedOrRemoved() {
  if (!AllowUiModeChange())
    return;

  bool has_external_pointing_device = false;
  // Check if there is an external mouse device.
  for (const ui::InputDevice& mouse :
       ui::DeviceDataManager::GetInstance()->GetMouseDevices()) {
    if (mouse.type == ui::INPUT_DEVICE_USB ||
        (mouse.type == ui::INPUT_DEVICE_BLUETOOTH &&
         bluetooth_devices_observer_->IsConnectedBluetoothDevice(mouse))) {
      has_external_pointing_device = true;
      break;
    }
  }
  // Check if there is an external touchpad device.
  if (!has_external_pointing_device) {
    for (const ui::InputDevice& touch_pad :
         ui::DeviceDataManager::GetInstance()->GetTouchpadDevices()) {
      if (touch_pad.type == ui::INPUT_DEVICE_USB ||
          (touch_pad.type == ui::INPUT_DEVICE_BLUETOOTH &&
           bluetooth_devices_observer_->IsConnectedBluetoothDevice(
               touch_pad))) {
        has_external_pointing_device = true;
        break;
      }
    }
  }

  if (has_external_pointing_device_ == has_external_pointing_device)
    return;

  has_external_pointing_device_ = has_external_pointing_device;

  // Enter clamshell mode whenever an external pointing device is attached.
  if (has_external_pointing_device) {
    AttemptLeaveTabletMode();
  } else if (HasActiveInternalDisplay() &&
             (LidAngleIsInTabletModeRange() || tablet_mode_switch_is_on_)) {
    // If there is no external pointing device, only enter tablet mode if docked
    // mode is inactive and 1) the lid angle can be detected and is in tablet
    // mode angle range. or 2) if the lid angle can't be detected (e.g., tablet
    // device or clamshell device) and |tablet_mode_switch_is_on_| is true (it
    // can only happen for tablet device as |tablet_mode_switch_is_on_| should
    // never be true for a clamshell device).
    AttemptEnterTabletMode();
  }
}

void TabletModeController::OnBluetoothAdapterOrDeviceChanged(
    device::BluetoothDevice* device) {
  // We only care about pointing type bluetooth device change. Note KEYBOARD
  // type is also included here as sometimes a bluetooth keyboard comes with a
  // touch pad.
  if (!device ||
      device->GetDeviceType() == device::BluetoothDeviceType::MOUSE ||
      device->GetDeviceType() ==
          device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO ||
      device->GetDeviceType() == device::BluetoothDeviceType::KEYBOARD ||
      device->GetDeviceType() == device::BluetoothDeviceType::TABLET) {
    VLOG(1) << "Bluetooth device configuration changed.";
    HandlePointingDeviceAddedOrRemoved();
  }
}

void TabletModeController::UpdateInternalInputDevicesEventBlocker() {
  bool should_block_internal_events = false;
  if (IsTabletModeWindowManagerEnabled()) {
    // If we are currently in tablet mode, the internal input events should
    // always be blocked.
    should_block_internal_events = (force_ui_mode_ == UiMode::kNone);
  } else if (HasActiveInternalDisplay() &&
             (LidAngleIsInTabletModeRange() || tablet_mode_switch_is_on_)) {
    // If we are currently in clamshell mode, the intenral input events should
    // only be blocked if the current lid angle belongs to tablet mode angle
    // or |tablet_mode_switch_is_on_| is true.
    // Note if we don't have an active internal display, the device is currently
    // in docked mode, and the user may still want to use the internal keyboard
    // and mouse in docked mode, we don't block internal events in this case.
    should_block_internal_events = true;
  }

  if (should_block_internal_events == AreInternalInputDeviceEventsBlocked())
    return;

  event_blocker_->UpdateInternalInputDevices(should_block_internal_events);
  for (auto& observer : tablet_mode_observers_)
    observer.OnTabletModeEventsBlockingChanged();
}

bool TabletModeController::LidAngleIsInTabletModeRange() {
  return can_detect_lid_angle_ && !lid_is_closed_ &&
         lid_angle_ >= kEnterTabletModeAngle;
}

void TabletModeController::SuspendOcclusionTracker() {
  occlusion_tracker_reset_timer_.Stop();
  occlusion_tracker_pauser_ =
      std::make_unique<aura::WindowOcclusionTracker::ScopedPause>();
  occlusion_tracker_reset_timer_.Start(FROM_HERE, kOcclusionTrackerTimeout,
                                       this,
                                       &TabletModeController::ResetPauser);
}

void TabletModeController::ResetPauser() {
  occlusion_tracker_pauser_.reset();
}

void TabletModeController::FinishInitTabletMode() {
  tablet_mode_window_manager_ = std::make_unique<TabletModeWindowManager>();
  tablet_mode_window_manager_->Init();

  base::RecordAction(base::UserMetricsAction("Touchview_Enabled"));
  RecordTabletModeUsageInterval(TABLET_MODE_INTERVAL_INACTIVE);
  for (auto& observer : tablet_mode_observers_)
    observer.OnTabletModeStarted();

  // In some cases, TabletModeWindowManager::TabletModeWindowManager uses
  // split view to represent windows that were snapped in desktop mode. If
  // there is a window snapped on one side but no window snapped on the other
  // side, then overview mode should be started (to be seen on the side with
  // no snapped window).
  const auto state = Shell::Get()->split_view_controller()->state();
  if (state == SplitViewState::kLeftSnapped ||
      state == SplitViewState::kRightSnapped) {
    Shell::Get()->overview_controller()->ToggleOverview();
  }

  state_ = State::kInTabletMode;
  if (toggle_observer_)  // Null at startup and in tests.
    toggle_observer_->OnTabletModeToggled(true);
  VLOG(1) << "Enter tablet mode.";
}

void TabletModeController::DeleteScreenshot() {
  screenshot_layer_.reset();
  screenshot_taken_callback_.Cancel();
  screenshot_set_callback_.Cancel();
}

void TabletModeController::TakeScreenshot(
    aura::Window* top_window,
    base::OnceClosure on_screenshot_taken) {
  DCHECK(top_window);
  DCHECK(!top_window->IsRootWindow());

  auto* screenshot_window = top_window->GetRootWindow()->GetChildById(
      kShellWindowId_ScreenRotationContainer);

  // Pause the compositor and hide the top window before taking a screenshot.
  // Use opacity zero instead of show/hide to preserve MRU ordering.
  const auto roots = Shell::GetAllRootWindows();
  for (auto* root : roots)
    root->GetHost()->compositor()->SetAllowLocksToExtendTimeout(true);
  top_window->layer()->SetOpacity(0.f);

  // Request a screenshot.
  screenshot_taken_callback_.Reset(base::BindOnce(
      &TabletModeController::OnScreenshotTaken, weak_factory_.GetWeakPtr(),
      top_window, std::move(on_screenshot_taken)));
  const gfx::Rect request_bounds(screenshot_window->layer()->size());
  auto screenshot_request = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA_TEXTURE,
      screenshot_taken_callback_.callback());
  screenshot_request->set_area(request_bounds);
  screenshot_request->set_result_selection(request_bounds);
  screenshot_window->layer()->RequestCopyOfOutput(
      std::move(screenshot_request));

  top_window->layer()->SetOpacity(1.f);
  for (auto* root : roots)
    root->GetHost()->compositor()->SetAllowLocksToExtendTimeout(false);
}

void TabletModeController::OnScreenshotTaken(
    aura::Window* top_window,
    base::OnceClosure on_screenshot_taken,
    std::unique_ptr<viz::CopyOutputResult> copy_result) {
  if (!copy_result || copy_result->IsEmpty()) {
    std::move(on_screenshot_taken).Run();
    return;
  }

  // Stack the screenshot under |top_window|, to fully occlude all windows
  // except |top_window| for the duration of the enter tablet mode animation.
  screenshot_layer_ = CreateLayerFromScreenshotResult(std::move(copy_result));
  top_window->parent()->layer()->Add(screenshot_layer_.get());
  screenshot_layer_->SetBounds(top_window->GetRootWindow()->bounds());
  top_window->parent()->layer()->StackBelow(screenshot_layer_.get(),
                                            top_window->layer());

  std::move(on_screenshot_taken).Run();
}

}  // namespace ash
