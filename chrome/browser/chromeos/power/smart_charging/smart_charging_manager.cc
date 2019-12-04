// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/smart_charging/smart_charging_manager.h"

#include <memory>

#include "chrome/browser/chromeos/power/ml/recent_events_counter.h"
#include "chromeos/constants/devicetype.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"

namespace chromeos {
namespace power {

namespace {
// Interval at which data should be logged.
constexpr base::TimeDelta kLoggingInterval = base::TimeDelta::FromMinutes(30);

// Count number of key, mouse and touch events in the past 30 minutes.
constexpr base::TimeDelta kUserInputEventsDuration =
    base::TimeDelta::FromMinutes(30);

// Granularity of input events is per minute.
constexpr int kNumUserInputEventsBuckets =
    kUserInputEventsDuration / base::TimeDelta::FromMinutes(1);
}  // namespace

SmartChargingManager::SmartChargingManager(
    ui::UserActivityDetector* detector,
    std::unique_ptr<base::RepeatingTimer> periodic_timer)
    : periodic_timer_(std::move(periodic_timer)),
      mouse_counter_(std::make_unique<ml::RecentEventsCounter>(
          kUserInputEventsDuration,
          kNumUserInputEventsBuckets)),
      key_counter_(std::make_unique<ml::RecentEventsCounter>(
          kUserInputEventsDuration,
          kNumUserInputEventsBuckets)),
      stylus_counter_(std::make_unique<ml::RecentEventsCounter>(
          kUserInputEventsDuration,
          kNumUserInputEventsBuckets)),
      touch_counter_(std::make_unique<ml::RecentEventsCounter>(
          kUserInputEventsDuration,
          kNumUserInputEventsBuckets)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(detector);
  user_activity_observer_.Add(detector);
  power_manager_client_observer_.Add(chromeos::PowerManagerClient::Get());
}

SmartChargingManager::~SmartChargingManager() = default;

std::unique_ptr<SmartChargingManager> SmartChargingManager::CreateInstance() {
  // TODO(crbug.com/1028853): we are collecting data from Chromebook only. Since
  // this action is discouraged, we will modify the condition latter using dbus
  // calls.
  if (chromeos::GetDeviceType() != chromeos::DeviceType::kChromebook)
    return nullptr;

  ui::UserActivityDetector* const detector = ui::UserActivityDetector::Get();
  DCHECK(detector);

  std::unique_ptr<SmartChargingManager> screen_brightness_manager =
      std::make_unique<SmartChargingManager>(
          detector, std::make_unique<base::RepeatingTimer>());

  return screen_brightness_manager;
}

void SmartChargingManager::OnUserActivity(const ui::Event* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!event)
    return;
  const base::TimeDelta time_since_boot = boot_clock_.GetTimeSinceBoot();

  // Using time_since_boot instead of the event's time stamp so we can use the
  // boot clock.
  if (event->IsMouseEvent()) {
    mouse_counter_->Log(time_since_boot);
    return;
  }
  if (event->IsKeyEvent()) {
    key_counter_->Log(time_since_boot);
    return;
  }
  if (event->IsTouchEvent()) {
    if (event->AsTouchEvent()->pointer_details().pointer_type ==
        ui::EventPointerType::POINTER_TYPE_PEN) {
      stylus_counter_->Log(time_since_boot);
      return;
    }
    touch_counter_->Log(time_since_boot);
  }
}

void SmartChargingManager::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  screen_brightness_percent_ = change.percent();
}

void SmartChargingManager::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (proto.has_battery_percent()) {
    battery_percent_ = proto.battery_percent();
  }

  if (!proto.has_external_power()) {
    return;
  }

  // Logic for the first PowerChanged call.
  if (!external_power_.has_value()) {
    external_power_ = proto.external_power();
    periodic_timer_->Start(FROM_HERE, kLoggingInterval, this,
                           &SmartChargingManager::OnTimerFired);
    if (external_power_.value() == power_manager::PowerSupplyProperties::AC) {
      LogEvent(UserChargingEvent::Event::CHARGER_PLUGGED_IN);
    }
    return;
  }

  const bool was_on_ac =
      external_power_.value() == power_manager::PowerSupplyProperties::AC;
  const bool now_on_ac =
      proto.external_power() == power_manager::PowerSupplyProperties::AC;

  // User plugged the charger in.
  if (!was_on_ac && now_on_ac) {
    external_power_ = proto.external_power();
    LogEvent(UserChargingEvent::Event::CHARGER_PLUGGED_IN);
  } else if (was_on_ac && !now_on_ac) {
    // User unplugged the charger.
    external_power_ = proto.external_power();
    LogEvent(UserChargingEvent::Event::CHARGER_UNPLUGGED);
  }
}

void SmartChargingManager::PowerManagerBecameAvailable(bool available) {
  if (!available) {
    return;
  }
  chromeos::PowerManagerClient::Get()->RequestStatusUpdate();
  chromeos::PowerManagerClient::Get()->GetScreenBrightnessPercent(
      base::BindOnce(&SmartChargingManager::OnReceiveScreenBrightnessPercent,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SmartChargingManager::PopulateUserChargingEventProto(
    UserChargingEvent* proto) {
  auto& features = *proto->mutable_features();
  if (battery_percent_)
    features.set_battery_percentage(static_cast<int>(battery_percent_.value()));

  const base::TimeDelta time_since_boot = boot_clock_.GetTimeSinceBoot();
  features.set_num_recent_key_events(key_counter_->GetTotal(time_since_boot));
  features.set_num_recent_mouse_events(
      mouse_counter_->GetTotal(time_since_boot));
  features.set_num_recent_touch_events(
      touch_counter_->GetTotal(time_since_boot));
  features.set_num_recent_stylus_events(
      stylus_counter_->GetTotal(time_since_boot));

  if (screen_brightness_percent_)
    features.set_screen_brightness_percent(
        static_cast<int>(screen_brightness_percent_.value()));
}

void SmartChargingManager::LogEvent(
    const UserChargingEvent::Event::Reason& reason) {
  UserChargingEvent proto;
  proto.mutable_event()->set_event_id(++event_id_);
  proto.mutable_event()->set_reason(reason);
  PopulateUserChargingEventProto(&proto);

  // TODO(crbug.com/1028853): This is for testing only. Need to remove when
  // ukm logger is available.
  user_charging_event_for_test_ = proto;

  // TODO(crbug.com/1028853): Implements logic of this function.
}

void SmartChargingManager::OnTimerFired() {
  LogEvent(UserChargingEvent_Event::PERIODIC_LOG);
}

void SmartChargingManager::OnReceiveScreenBrightnessPercent(
    base::Optional<double> screen_brightness_percent) {
  if (screen_brightness_percent.has_value()) {
    screen_brightness_percent_ = *screen_brightness_percent;
  }
}

}  // namespace power
}  // namespace chromeos
