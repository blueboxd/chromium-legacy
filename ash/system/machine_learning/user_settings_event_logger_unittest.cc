// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/machine_learning/user_settings_event_logger.h"

#include "ash/shell.h"
#include "ash/system/machine_learning/user_settings_event.pb.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/test/task_environment.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace ash {
namespace ml {
namespace {

using chromeos::network_config::mojom::CellularStateProperties;
using chromeos::network_config::mojom::CellularStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::NetworkTypeStateProperties;
using chromeos::network_config::mojom::SecurityType;
using chromeos::network_config::mojom::WiFiStateProperties;
using chromeos::network_config::mojom::WiFiStatePropertiesPtr;
using ukm::TestUkmRecorder;

NetworkStatePropertiesPtr CreateWifiNetwork(int signal_strength,
                                            SecurityType security_type) {
  NetworkStatePropertiesPtr network = NetworkStateProperties::New();

  WiFiStatePropertiesPtr wifi = WiFiStateProperties::New();
  wifi->signal_strength = signal_strength;
  wifi->security = security_type;
  network->type = NetworkType::kWiFi;
  network->type_state = NetworkTypeStateProperties::New();
  network->type_state->set_wifi(std::move(wifi));

  return network;
}

NetworkStatePropertiesPtr CreateCellularNetwork(int signal_strength) {
  NetworkStatePropertiesPtr network = NetworkStateProperties::New();

  CellularStatePropertiesPtr cellular = CellularStateProperties::New();
  cellular->signal_strength = signal_strength;
  network->type = NetworkType::kCellular;
  network->type_state = NetworkTypeStateProperties::New();
  network->type_state->set_cellular(std::move(cellular));

  return network;
}

}  // namespace

class UserSettingsEventLoggerTest : public AshTestBase {
 public:
  UserSettingsEventLoggerTest()
      : AshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}
  ~UserSettingsEventLoggerTest() override = default;
  UserSettingsEventLoggerTest(const UserSettingsEventLoggerTest&) = delete;
  UserSettingsEventLoggerTest& operator=(const UserSettingsEventLoggerTest&) =
      delete;

  void SetUp() override {
    AshTestBase::SetUp();
    logger_ = UserSettingsEventLogger::Get();
  }

 protected:
  void FastForwardBySeconds(const int seconds) {
    task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(seconds));
  }

  std::vector<const ukm::mojom::UkmEntry*> GetUkmEntries() {
    return ukm_recorder_.GetEntriesByName(
        ukm::builders::UserSettingsEvent::kEntryName);
  }

  // Brightness features are logged at the end of the timer delay.
  void LogBrightnessAndWait(int previous_level, int current_level) {
    logger_->LogBrightnessUkmEvent(previous_level, current_level);
    task_environment_->FastForwardBy(kBrightnessDelay);
  }

  UserSettingsEventLogger* logger_;

 private:
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
};

TEST_F(UserSettingsEventLoggerTest, TestLogWifiEvent) {
  logger_->LogNetworkUkmEvent(
      *CreateWifiNetwork(50, SecurityType::kNone).get());
  logger_->LogNetworkUkmEvent(
      *CreateWifiNetwork(25, SecurityType::kWpaEap).get());

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(2ul, entries.size());

  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::WIFI);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "SignalStrength", 50);
  TestUkmRecorder::ExpectEntryMetric(entry, "HasWifiSecurity", false);

  entry = entries[1];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::WIFI);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "SignalStrength", 25);
  TestUkmRecorder::ExpectEntryMetric(entry, "HasWifiSecurity", true);
}

TEST_F(UserSettingsEventLoggerTest, TestLogCellularEvent) {
  logger_->LogNetworkUkmEvent(*CreateCellularNetwork(50).get());
  logger_->LogNetworkUkmEvent(*CreateCellularNetwork(25).get());

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(2ul, entries.size());

  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::CELLULAR);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "SignalStrength", 50);
  TestUkmRecorder::ExpectEntryMetric(entry, "UsedCellularInSession", false);

  entry = entries[1];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::CELLULAR);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "SignalStrength", 25);
  // After the first cellular event, |UsedCellularInSession| should now be true.
  TestUkmRecorder::ExpectEntryMetric(entry, "UsedCellularInSession", true);
}

TEST_F(UserSettingsEventLoggerTest, TestLogBluetoothEvent) {
  // Log an event with a null bluetooth address.
  logger_->LogBluetoothUkmEvent(BluetoothAddress());

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(1ul, entries.size());

  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::BLUETOOTH);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  // TODO(crbug/1014839): Test that |is_paired_bluetooth_device| is set
  // correctly.
}

TEST_F(UserSettingsEventLoggerTest, TestLogNightLightEvent) {
  Shell::Get()->night_light_controller()->SetScheduleType(
      NightLightController::ScheduleType::kSunsetToSunrise);
  logger_->LogNightLightUkmEvent(true);

  Shell::Get()->night_light_controller()->SetScheduleType(
      NightLightController::ScheduleType::kNone);
  logger_->LogNightLightUkmEvent(false);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(2ul, entries.size());

  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::NIGHT_LIGHT);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "PreviousValue", false);
  TestUkmRecorder::ExpectEntryMetric(entry, "CurrentValue", true);
  TestUkmRecorder::ExpectEntryMetric(entry, "HasNightLightSchedule", true);

  entry = entries[1];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::NIGHT_LIGHT);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "PreviousValue", true);
  TestUkmRecorder::ExpectEntryMetric(entry, "CurrentValue", false);
  TestUkmRecorder::ExpectEntryMetric(entry, "HasNightLightSchedule", false);
}

TEST_F(UserSettingsEventLoggerTest, TestLogQuietModeEvent) {
  // Log an event that quiet mode has been switched on.
  logger_->LogQuietModeUkmEvent(true);

  // Start two casting sessions.
  logger_->OnCastingSessionStartedOrStopped(true);
  logger_->OnCastingSessionStartedOrStopped(true);
  logger_->LogQuietModeUkmEvent(false);

  // End one casting session.
  logger_->OnCastingSessionStartedOrStopped(false);
  FastForwardBySeconds(301);
  logger_->LogQuietModeUkmEvent(false);

  // End the final casting session. |is_recently_presenting| should remain true
  // for 5 minutes, with 1 second given for leeway on either side.
  logger_->OnCastingSessionStartedOrStopped(false);
  FastForwardBySeconds(299);
  logger_->LogQuietModeUkmEvent(false);
  FastForwardBySeconds(2);
  logger_->LogQuietModeUkmEvent(false);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(5ul, entries.size());

  // Check that the first entry has all details recorded correctly.
  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::DO_NOT_DISTURB);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "PreviousValue", false);
  TestUkmRecorder::ExpectEntryMetric(entry, "CurrentValue", true);
  TestUkmRecorder::ExpectEntryMetric(entry, "IsRecentlyPresenting", false);

  // Check that subsequent entries correctly record |is_recently_presenting|.
  TestUkmRecorder::ExpectEntryMetric(entries[1], "IsRecentlyPresenting", true);
  TestUkmRecorder::ExpectEntryMetric(entries[2], "IsRecentlyPresenting", true);
  TestUkmRecorder::ExpectEntryMetric(entries[3], "IsRecentlyPresenting", true);
  TestUkmRecorder::ExpectEntryMetric(entries[4], "IsRecentlyPresenting", false);
}

TEST_F(UserSettingsEventLoggerTest, TestLogAccessibilityEvent) {
  // Log an event that high contrast mode has been enabled.
  logger_->LogAccessibilityUkmEvent(UserSettingsEvent::Event::HIGH_CONTRAST,
                                    true);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(1ul, entries.size());

  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::ACCESSIBILITY);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "PreviousValue", false);
  TestUkmRecorder::ExpectEntryMetric(entry, "CurrentValue", true);
  TestUkmRecorder::ExpectEntryMetric(entry, "AccessibilityId",
                                     UserSettingsEvent::Event::HIGH_CONTRAST);
}

TEST_F(UserSettingsEventLoggerTest, TestLogVolumeEvent) {
  logger_->LogVolumeUkmEvent(23, 98);
  logger_->OnOutputStarted();
  logger_->LogVolumeUkmEvent(0, 0);
  logger_->OnOutputStopped();
  logger_->LogVolumeUkmEvent(0, 0);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(3ul, entries.size());

  // Check that the first entry has all details recorded correctly.
  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::VOLUME);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "PreviousValue", 23);
  TestUkmRecorder::ExpectEntryMetric(entry, "CurrentValue", 98);
  TestUkmRecorder::ExpectEntryMetric(entry, "IsPlayingAudio", false);

  // Check that subsequent entries correctly record |is_playing_audio|.
  TestUkmRecorder::ExpectEntryMetric(entries[1], "IsPlayingAudio", true);
  TestUkmRecorder::ExpectEntryMetric(entries[2], "IsPlayingAudio", false);
}

TEST_F(UserSettingsEventLoggerTest, TestBrightnessFeatures) {
  LogBrightnessAndWait(12, 29);

  // Enter fullscreen.
  logger_->OnFullscreenStateChanged(true, nullptr);
  LogBrightnessAndWait(0, 0);

  // Exit fullscreen. |is_recently_fullscreen| should remain true for 5 minutes,
  // with 1 second given for leeway on either side.
  logger_->OnFullscreenStateChanged(false, nullptr);
  FastForwardBySeconds(299 - kBrightnessDelay.InSeconds());
  LogBrightnessAndWait(0, 0);
  FastForwardBySeconds(2);
  LogBrightnessAndWait(0, 0);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(4ul, entries.size());

  // Check that the first entry has all details recorded correctly.
  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::BRIGHTNESS);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "PreviousValue", 12);
  TestUkmRecorder::ExpectEntryMetric(entry, "CurrentValue", 29);
  TestUkmRecorder::ExpectEntryMetric(entry, "IsRecentlyFullscreen", false);

  // Check that subsequent entries correctly record |is_recently_fullscreen|.
  TestUkmRecorder::ExpectEntryMetric(entries[1], "IsRecentlyFullscreen", true);
  TestUkmRecorder::ExpectEntryMetric(entries[2], "IsRecentlyFullscreen", true);
  TestUkmRecorder::ExpectEntryMetric(entries[3], "IsRecentlyFullscreen", false);
}

TEST_F(UserSettingsEventLoggerTest, TestBrightnessDelay) {
  // Only log an event if there is a pause of |kBrightnessDelay|.
  logger_->LogBrightnessUkmEvent(10, 11);
  task_environment_->FastForwardBy(kBrightnessDelay / 2);
  logger_->LogBrightnessUkmEvent(11, 12);
  logger_->LogBrightnessUkmEvent(12, 13);
  logger_->LogBrightnessUkmEvent(13, 14);
  task_environment_->FastForwardBy(kBrightnessDelay / 2);
  logger_->LogBrightnessUkmEvent(14, 15);
  task_environment_->FastForwardBy(kBrightnessDelay);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(1ul, entries.size());

  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingId",
                                     UserSettingsEvent::Event::BRIGHTNESS);
  TestUkmRecorder::ExpectEntryMetric(entry, "SettingType",
                                     UserSettingsEvent::Event::QUICK_SETTINGS);
  TestUkmRecorder::ExpectEntryMetric(entry, "PreviousValue", 10);
  TestUkmRecorder::ExpectEntryMetric(entry, "CurrentValue", 15);
}

}  // namespace ml
}  // namespace ash
