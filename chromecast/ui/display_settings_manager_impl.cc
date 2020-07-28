// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ui/display_settings_manager_impl.h"

#include "base/logging.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "chromecast/ui/display_settings/brightness_animation.h"
#include "chromecast/ui/display_settings/color_temperature_animation.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"

#if defined(USE_AURA)
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_display_configurator.h"
#include "chromecast/ui/display_settings/gamma_configurator.h"
#endif  // defined(USE_AURA)

namespace chromecast {

namespace {

constexpr base::TimeDelta kAnimationDuration = base::TimeDelta::FromSeconds(2);
constexpr base::TimeDelta kScreenOnOffDuration =
    base::TimeDelta::FromMilliseconds(200);

#if defined(USE_AURA)
// These delays are needed to ensure there are no visible artifacts due to the
// backlight turning on prior to the LCD fully initializing or vice-versa.
// TODO(b/161140301): Make this configurable for different products
// TODO(b/161268188): Remove these if the delays can be handled by the kernel
constexpr base::TimeDelta kDisplayPowerOnDelay =
    base::TimeDelta::FromMilliseconds(35);
constexpr base::TimeDelta kDisplayPowerOffDelay =
    base::TimeDelta::FromMilliseconds(85);
#endif  // defined(USE_AURA)

const float kMinApiBrightness = 0.0f;
const float kMaxApiBrightness = 1.0f;
const float kDefaultApiBrightness = kMaxApiBrightness;

}  // namespace

DisplaySettingsManagerImpl::DisplaySettingsManagerImpl(
    CastWindowManager* window_manager,
    const DisplaySettingsManager::ColorTemperatureConfig&
        color_temperature_config)
    : window_manager_(window_manager),
#if defined(USE_AURA)
      display_configurator_(
          shell::CastBrowserProcess::GetInstance()->display_configurator()),
      gamma_configurator_(
          std::make_unique<GammaConfigurator>(window_manager_,
                                              display_configurator_)),
#else
      display_configurator_(nullptr),
#endif  // defined(USE_AURA)
      brightness_(-1.0f),
      screen_on_(true),
#if defined(USE_AURA)
      screen_power_on_(true),
      allow_screen_power_off_(false),
#endif  // defined(USE_AURA)
      color_temperature_animation_(std::make_unique<ColorTemperatureAnimation>(
          window_manager_,
          display_configurator_,
          color_temperature_config)),
      weak_factory_(this) {
  DCHECK(window_manager_);
#if defined(USE_AURA)
  DCHECK(display_configurator_);
#endif  // defined(USE_AURA)
}

DisplaySettingsManagerImpl::~DisplaySettingsManagerImpl() = default;

void DisplaySettingsManagerImpl::SetDelegate(
    DisplaySettingsManager::Delegate* delegate) {
  brightness_animation_ = std::make_unique<BrightnessAnimation>(delegate);
}

void DisplaySettingsManagerImpl::ResetDelegate() {
  // Skip a brightess animation to its last step and stop
  // This is important for the final brightness to be cached on reboot
  brightness_animation_.reset();
}

void DisplaySettingsManagerImpl::SetGammaCalibration(
    const std::vector<display::GammaRampRGBEntry>& gamma) {
#if defined(USE_AURA)
  gamma_configurator_->OnCalibratedGammaLoaded(gamma);
#endif  // defined(USE_AURA)
}

void DisplaySettingsManagerImpl::NotifyBrightnessChanged(float new_brightness,
                                                         float old_brightness) {
  for (auto& observer : observers_)
    observer->OnDisplayBrightnessChanged(new_brightness);
}

void DisplaySettingsManagerImpl::SetColorInversion(bool enable) {
#if defined(USE_AURA)
  gamma_configurator_->SetColorInversion(enable);
#endif  // defined(USE_AURA)
  window_manager_->NotifyColorInversionEnabled(enable);
}

void DisplaySettingsManagerImpl::AddReceiver(
    mojo::PendingReceiver<mojom::DisplaySettings> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DisplaySettingsManagerImpl::SetColorTemperature(float temperature) {
  DVLOG(4) << "Setting color temperature to " << temperature << " Kelvin.";
  color_temperature_animation_->AnimateToNewValue(temperature,
                                                  kAnimationDuration);
}

void DisplaySettingsManagerImpl::SetColorTemperatureSmooth(
    float temperature,
    base::TimeDelta duration) {
  DVLOG(4) << "Setting color temperature to " << temperature
           << " Kelvin. Duration: " << duration;
  color_temperature_animation_->AnimateToNewValue(temperature, duration);
}

void DisplaySettingsManagerImpl::ResetColorTemperature() {
  color_temperature_animation_->AnimateToNeutral(kAnimationDuration);
}

void DisplaySettingsManagerImpl::SetBrightness(float brightness) {
  SetBrightnessSmooth(brightness, base::TimeDelta::FromSeconds(0));
}

void DisplaySettingsManagerImpl::SetBrightnessSmooth(float brightness,
                                                     base::TimeDelta duration) {
  if (brightness < kMinApiBrightness) {
    LOG(ERROR) << "brightness " << brightness
               << " is less than minimum brightness " << kMinApiBrightness
               << ".";
    return;
  }

  if (brightness > kMaxApiBrightness) {
    LOG(ERROR) << "brightness " << brightness
               << " is greater than maximum brightness " << kMaxApiBrightness
               << ".";
    return;
  }

  brightness_ = brightness;

  // If the screen is off, keep the new brightness but don't apply it
  if (!screen_on_) {
    return;
  }

  UpdateBrightness(duration);
}

void DisplaySettingsManagerImpl::ResetBrightness() {
  SetBrightness(kDefaultApiBrightness);
}

#if defined(USE_AURA)
void DisplaySettingsManagerImpl::OnDisplayOn(
    const base::flat_map<int64_t, bool>& statuses) {
  for (const auto& status : statuses) {
    bool display_success = status.second;
    if (!display_success) {
      // Fatal since the user has no other way of turning the screen on if this
      // failed.
      LOG(FATAL) << "Failed to enable screen";
      return;
    }
  }
  screen_power_on_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DisplaySettingsManagerImpl::OnDisplayOnTimeoutCompleted,
                     weak_factory_.GetWeakPtr()),
      kDisplayPowerOnDelay);
}

void DisplaySettingsManagerImpl::OnDisplayOnTimeoutCompleted() {
  UpdateBrightness(kScreenOnOffDuration);
  window_manager_->SetTouchInputDisabled(false /* since screen_on = true */);
}

void DisplaySettingsManagerImpl::OnDisplayOffTimeoutCompleted() {
  display_configurator_->DisableDisplay(
      base::BindOnce([](const base::flat_map<int64_t, bool>& statuses) {
        for (const auto& status : statuses) {
          bool display_success = status.second;
          LOG_IF(FATAL, !display_success) << "Failed to disable display";
        }
      }));
  screen_power_on_ = false;
}

void DisplaySettingsManagerImpl::SetScreenOn(bool screen_on) {
  // Allow this to run if screen_on == screen_on_ == false IF
  // previously, the screen was turned off without powering off the screen
  // and we want to power it off this time
  if (screen_on == screen_on_ &&
      !(!screen_on && allow_screen_power_off_ && screen_power_on_)) {
    return;
  }

  LOG(INFO) << "Setting screen on to " << screen_on;
  screen_on_ = screen_on;

  // TODO(b/161268188): This can be simplified and the delays removed
  // if backlight timing is handled by the kernel
  if (screen_on && !screen_power_on_) {
    display_configurator_->EnableDisplay(base::BindOnce(
        &DisplaySettingsManagerImpl::OnDisplayOn, weak_factory_.GetWeakPtr()));
  } else {
    UpdateBrightness(kScreenOnOffDuration);
    window_manager_->SetTouchInputDisabled(!screen_on_);
    if (!screen_on && allow_screen_power_off_) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              &DisplaySettingsManagerImpl::OnDisplayOffTimeoutCompleted,
              weak_factory_.GetWeakPtr()),
          kDisplayPowerOffDelay + kScreenOnOffDuration);
    }
  }
}

void DisplaySettingsManagerImpl::SetAllowScreenPowerOff(bool allow_power_off) {
  allow_screen_power_off_ = allow_power_off;
}
#else
void DisplaySettingsManagerImpl::SetScreenOn(bool screen_on) {
  if (screen_on == screen_on_) {
    return;
  }

  LOG(INFO) << "Setting screen on to " << screen_on;
  screen_on_ = screen_on;

  UpdateBrightness(kScreenOnOffDuration);
  window_manager_->SetTouchInputDisabled(!screen_on_);
}
void DisplaySettingsManagerImpl::SetAllowScreenPowerOff(bool allow_power_off) {}
#endif

void DisplaySettingsManagerImpl::AddDisplaySettingsObserver(
    mojo::PendingRemote<mojom::DisplaySettingsObserver> observer) {
  mojo::Remote<mojom::DisplaySettingsObserver> observer_remote(
      std::move(observer));
  observers_.Add(std::move(observer_remote));
}

void DisplaySettingsManagerImpl::UpdateBrightness(base::TimeDelta duration) {
  float brightness = screen_on_ ? brightness_ : 0;

  if (brightness_animation_)
    brightness_animation_->AnimateToNewValue(brightness, duration);
}

}  // namespace chromecast
