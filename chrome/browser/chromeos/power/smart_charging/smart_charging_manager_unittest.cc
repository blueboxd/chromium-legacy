// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/smart_charging/smart_charging_manager.h"

#include "base/test/test_mock_time_task_runner.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace chromeos {
namespace power {

class SmartChargingManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  SmartChargingManagerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

  ~SmartChargingManagerTest() override = default;
  SmartChargingManagerTest(const SmartChargingManagerTest&) = delete;
  SmartChargingManagerTest& operator=(const SmartChargingManagerTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PowerManagerClient::InitializeFake();

    auto periodic_timer = std::make_unique<base::RepeatingTimer>();
    periodic_timer->SetTaskRunner(
        task_environment()->GetMainThreadTaskRunner());
    smart_charging_manager_ = std::make_unique<SmartChargingManager>(
        &user_activity_detector_, std::move(periodic_timer));
  }

  void TearDown() override {
    smart_charging_manager_.reset();
    PowerManagerClient::Shutdown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  UserChargingEvent GetUserChargingEvent() {
    return smart_charging_manager_->user_charging_event_for_test_;
  }

  base::Optional<power_manager::PowerSupplyProperties::ExternalPower>
  GetExternalPower() {
    return smart_charging_manager_->external_power_;
  }

 protected:
  void ReportUserActivity(const ui::Event* const event) {
    smart_charging_manager_->OnUserActivity(event);
  }

  void ReportPowerChangeEvent(
      const power_manager::PowerSupplyProperties::ExternalPower power,
      const float battery_percent) {
    power_manager::PowerSupplyProperties proto;
    proto.set_external_power(power);
    proto.set_battery_percent(battery_percent);
    FakePowerManagerClient::Get()->UpdatePowerProperties(proto);
  }

  void ReportBrightnessChangeEvent(const double level) {
    power_manager::BacklightBrightnessChange change;
    change.set_percent(level);
    smart_charging_manager_->ScreenBrightnessChanged(change);
  }

  void FireTimer() { smart_charging_manager_->OnTimerFired(); }

  void InitializeBrightness(const double level) {
    smart_charging_manager_->OnReceiveScreenBrightnessPercent(level);
  }

  void FastForwardTimeBySecs(const int seconds) {
    task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(seconds));
  }

  const gfx::Point kEventLocation = gfx::Point(90, 90);
  const ui::MouseEvent kMouseEvent = ui::MouseEvent(ui::ET_MOUSE_MOVED,
                                                    kEventLocation,
                                                    kEventLocation,
                                                    base::TimeTicks(),
                                                    0,
                                                    0);

 private:
  ui::UserActivityDetector user_activity_detector_;
  std::unique_ptr<SmartChargingManager> smart_charging_manager_;
};

TEST_F(SmartChargingManagerTest, BrightnessRecoredCorrectly) {
  InitializeBrightness(55.3f);

  // LogEvent() hasn't been called yet.
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::DISCONNECTED,
                         23.0f);
  EXPECT_FALSE(GetUserChargingEvent().has_features());
  EXPECT_FALSE(GetUserChargingEvent().has_event());

  ReportBrightnessChangeEvent(75.7f);

  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 15.0f);

  EXPECT_EQ(GetUserChargingEvent().features().screen_brightness_percent(), 75);
}

TEST_F(SmartChargingManagerTest, PowerChangedTest) {
  // LogEvent() hasn't been called yet.
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::DISCONNECTED,
                         23.0f);
  EXPECT_EQ(GetExternalPower().value(),
            power_manager::PowerSupplyProperties::DISCONNECTED);
  EXPECT_FALSE(GetUserChargingEvent().has_features());
  EXPECT_FALSE(GetUserChargingEvent().has_event());

  // Plugs in the charger.
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 15.0f);
  EXPECT_EQ(GetExternalPower().value(),
            power_manager::PowerSupplyProperties::AC);
  EXPECT_EQ(GetUserChargingEvent().features().battery_percentage(), 15);
  EXPECT_EQ(GetUserChargingEvent().event().event_id(), 0);
  EXPECT_EQ(GetUserChargingEvent().event().reason(),
            UserChargingEvent::Event::CHARGER_PLUGGED_IN);

  // Unplugs the charger.
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::USB, 85.0f);
  EXPECT_EQ(GetExternalPower().value(),
            power_manager::PowerSupplyProperties::USB);
  EXPECT_EQ(GetUserChargingEvent().features().battery_percentage(), 85);
  EXPECT_EQ(GetUserChargingEvent().event().event_id(), 1);
  EXPECT_EQ(GetUserChargingEvent().event().reason(),
            UserChargingEvent::Event::CHARGER_UNPLUGGED);
}

TEST_F(SmartChargingManagerTest, TimerFiredTest) {
  // Timer fired 2 times.
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::USB, 23.0f);
  FastForwardTimeBySecs(3600);
  EXPECT_EQ(GetUserChargingEvent().features().battery_percentage(), 23);
  EXPECT_EQ(GetUserChargingEvent().event().event_id(), 1);
  EXPECT_EQ(GetUserChargingEvent().event().reason(),
            UserChargingEvent::Event::PERIODIC_LOG);

  // Charger plugged in.
  ReportPowerChangeEvent(power_manager::PowerSupplyProperties::AC, 50.0f);
  FastForwardTimeBySecs(1900);
  EXPECT_EQ(GetUserChargingEvent().features().battery_percentage(), 50);
  EXPECT_EQ(GetUserChargingEvent().event().event_id(), 3);
  EXPECT_EQ(GetUserChargingEvent().event().reason(),
            UserChargingEvent::Event::PERIODIC_LOG);
}

TEST_F(SmartChargingManagerTest, UserEventCounts) {
  FastForwardTimeBySecs(1);
  ReportUserActivity(&kMouseEvent);

  const ui::TouchEvent kTouchEvent(
      ui::ET_TOUCH_PRESSED, kEventLocation, base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  ReportUserActivity(&kTouchEvent);
  ReportUserActivity(&kTouchEvent);

  const ui::KeyEvent kKeyEvent(
      ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, 0,
      ui::DomKey::FromCharacter('a'), base::TimeTicks());
  ReportUserActivity(&kKeyEvent);
  ReportUserActivity(&kKeyEvent);
  ReportUserActivity(&kKeyEvent);

  const ui::TouchEvent kStylusEvent(
      ui::ET_TOUCH_MOVED, kEventLocation, base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_PEN, 0),
      ui::EF_NONE);
  ReportUserActivity(&kStylusEvent);
  ReportUserActivity(&kStylusEvent);
  ReportUserActivity(&kStylusEvent);
  ReportUserActivity(&kStylusEvent);

  FastForwardTimeBySecs(2);
  FireTimer();

  const auto& proto = GetUserChargingEvent();

  EXPECT_EQ(proto.features().num_recent_mouse_events(), 1);
  EXPECT_EQ(proto.features().num_recent_touch_events(), 2);
  EXPECT_EQ(proto.features().num_recent_key_events(), 3);
  EXPECT_EQ(proto.features().num_recent_stylus_events(), 4);
}

}  // namespace power
}  // namespace chromeos
