// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::StrictMock;
using ::testing::WithArgs;

namespace chromeos {
namespace cros_healthd {
namespace {

std::vector<mojom::DiagnosticRoutineEnum> MakeAvailableRoutines() {
  return std::vector<mojom::DiagnosticRoutineEnum>{
      mojom::DiagnosticRoutineEnum::kUrandom,
      mojom::DiagnosticRoutineEnum::kBatteryCapacity,
      mojom::DiagnosticRoutineEnum::kBatteryHealth,
      mojom::DiagnosticRoutineEnum::kSmartctlCheck,
      mojom::DiagnosticRoutineEnum::kCpuCache,
      mojom::DiagnosticRoutineEnum::kCpuStress,
      mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy,
      mojom::DiagnosticRoutineEnum::kNvmeWearLevel,
      mojom::DiagnosticRoutineEnum::kNvmeSelfTest,
  };
}

mojom::RunRoutineResponsePtr MakeRunRoutineResponse() {
  return mojom::RunRoutineResponse::New(
      /*id=*/13, /*status=*/mojom::DiagnosticRoutineStatusEnum::kReady);
}

mojom::RoutineUpdatePtr MakeInteractiveRoutineUpdate() {
  mojom::InteractiveRoutineUpdate interactive_update(
      /*user_message=*/mojom::DiagnosticRoutineUserMessageEnum::kUnplugACPower);

  mojom::RoutineUpdateUnion update_union;
  update_union.set_interactive_update(interactive_update.Clone());

  return mojom::RoutineUpdate::New(
      /*progress_percent=*/42,
      /*output=*/mojo::ScopedHandle(), update_union.Clone());
}

mojom::RoutineUpdatePtr MakeNonInteractiveRoutineUpdate() {
  mojom::NonInteractiveRoutineUpdate noninteractive_update(
      /*status=*/mojom::DiagnosticRoutineStatusEnum::kRunning,
      /*status_message=*/"status_message");

  mojom::RoutineUpdateUnion update_union;
  update_union.set_noninteractive_update(noninteractive_update.Clone());

  return mojom::RoutineUpdate::New(
      /*progress_percent=*/43,
      /*output=*/mojo::ScopedHandle(), update_union.Clone());
}

base::Optional<std::vector<mojom::NonRemovableBlockDeviceInfoPtr>>
MakeNonRemovableBlockDeviceInfo() {
  std::vector<mojom::NonRemovableBlockDeviceInfoPtr> info;
  info.push_back(mojom::NonRemovableBlockDeviceInfo::New(
      "test_path", 123 /* size */, "test_type", 10 /* manfid */, "test_name",
      768 /* serial */));
  info.push_back(mojom::NonRemovableBlockDeviceInfo::New(
      "test_path2", 124 /* size */, "test_type2", 11 /* manfid */, "test_name2",
      767 /* serial */));
  return base::Optional<std::vector<mojom::NonRemovableBlockDeviceInfoPtr>>(
      std::move(info));
}

mojom::BatteryInfoPtr MakeBatteryInfo() {
  mojom::SmartBatteryInfoPtr smart_info = mojom::SmartBatteryInfo::New(
      "2018-08-06" /* manufacture_date */, 981729 /* temperature */);
  return mojom::BatteryInfo::New(
      2 /* cycle_count */, 12.9 /* voltage_now */,
      "battery_vendor" /* vendor */, "serial_number" /* serial_number */,
      5.275 /* charge_full_design */, 5.292 /* charge_full */,
      11.55 /* voltage_min_design */, "battery_model" /* model_name */,
      5.123 /* charge_now */, 98.123 /* current_now */,
      "battery_technology" /* technology */, "battery_status" /* status */,
      std::move(smart_info));
}

mojom::CachedVpdInfoPtr MakeCachedVpdInfo() {
  return mojom::CachedVpdInfo::New("fake_sku_number" /* sku_number */);
}

base::Optional<std::vector<mojom::CpuInfoPtr>> MakeCpuInfo() {
  std::vector<mojom::CpuInfoPtr> cpu_info;
  cpu_info.push_back(mojom::CpuInfo::New(
      "Dank CPU 1" /* model_name */,
      mojom::CpuArchitectureEnum::kX86_64 /* architecture */,
      3400000 /* max_clock_speed_khz */));
  cpu_info.push_back(mojom::CpuInfo::New(
      "Dank CPU 2" /* model_name */,
      mojom::CpuArchitectureEnum::kX86_64 /* architecture */,
      2600000 /* max_clock_speed_khz */));
  return cpu_info;
}

mojom::TimezoneInfoPtr MakeTimezoneInfo() {
  return mojom::TimezoneInfo::New("MST7MDT,M3.2.0,M11.1.0" /* posix */,
                                  "America/Denver" /* region */);
}

mojom::MemoryInfoPtr MakeMemoryInfo() {
  return mojom::MemoryInfo::New(987123 /* total_memory_kib */,
                                346432 /* free_memory_kib */,
                                45863 /* available_memory_kib */,
                                43264 /* page_faults_since_last_boot */);
}

base::Optional<std::vector<mojom::BacklightInfoPtr>> MakeBacklightInfo() {
  std::vector<mojom::BacklightInfoPtr> backlight_info;
  backlight_info.push_back(mojom::BacklightInfo::New(
      "path_1" /* path */, 6537 /* max_brightness */, 987 /* brightness */));
  backlight_info.push_back(mojom::BacklightInfo::New(
      "path_2" /* path */, 3242 /* max_brightness */, 65 /* brightness */));
  return backlight_info;
}

base::Optional<std::vector<mojom::FanInfoPtr>> MakeFanInfo() {
  std::vector<mojom::FanInfoPtr> fan_info;
  fan_info.push_back(mojom::FanInfo::New(1200 /* speed_rpm */));
  fan_info.push_back(mojom::FanInfo::New(2650 /* speed_rpm */));
  return fan_info;
}

mojom::TelemetryInfoPtr MakeTelemetryInfo() {
  return mojom::TelemetryInfo::New(
      MakeBatteryInfo() /* battery_info */,
      MakeNonRemovableBlockDeviceInfo() /* block_device_info */,
      MakeCachedVpdInfo() /* vpd_info */, MakeCpuInfo() /* cpu_info */,
      MakeTimezoneInfo() /* timezone_info */,
      MakeMemoryInfo() /* memory_info */,
      MakeBacklightInfo() /* backlight_info */, MakeFanInfo() /* fan_info */
  );
}

class CrosHealthdServiceConnectionTest : public testing::Test {
 public:
  CrosHealthdServiceConnectionTest() = default;

  void SetUp() override { CrosHealthdClient::InitializeFake(); }

  void TearDown() override {
    CrosHealthdClient::Shutdown();

    // Wait for ServiceConnection to observe the destruction of the client.
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(CrosHealthdServiceConnectionTest);
};

TEST_F(CrosHealthdServiceConnectionTest, GetAvailableRoutines) {
  // Test that we can retrieve a list of available routines.
  auto routines = MakeAvailableRoutines();
  FakeCrosHealthdClient::Get()->SetAvailableRoutinesForTesting(routines);
  bool callback_done = false;
  ServiceConnection::GetInstance()->GetAvailableRoutines(base::BindOnce(
      [](bool* callback_done,
         const std::vector<mojom::DiagnosticRoutineEnum>& response) {
        EXPECT_EQ(response, MakeAvailableRoutines());
        *callback_done = true;
      },
      &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

TEST_F(CrosHealthdServiceConnectionTest, GetRoutineUpdate) {
  // Test that we can get an interactive routine update.
  auto interactive_update = MakeInteractiveRoutineUpdate();
  FakeCrosHealthdClient::Get()->SetGetRoutineUpdateResponseForTesting(
      interactive_update);
  bool callback_done = false;
  ServiceConnection::GetInstance()->GetRoutineUpdate(
      /*id=*/542, /*command=*/mojom::DiagnosticRoutineCommandEnum::kGetStatus,
      /*include_output=*/true,
      base::BindOnce(
          [](bool* callback_done, mojom::RoutineUpdatePtr response) {
            EXPECT_EQ(response, MakeInteractiveRoutineUpdate());
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);

  // Test that we can get a noninteractive routine update.
  auto noninteractive_update = MakeNonInteractiveRoutineUpdate();
  FakeCrosHealthdClient::Get()->SetGetRoutineUpdateResponseForTesting(
      noninteractive_update);
  callback_done = false;
  ServiceConnection::GetInstance()->GetRoutineUpdate(
      /*id=*/543, /*command=*/mojom::DiagnosticRoutineCommandEnum::kCancel,
      /*include_output=*/false,
      base::BindOnce(
          [](bool* callback_done, mojom::RoutineUpdatePtr response) {
            EXPECT_EQ(response, MakeNonInteractiveRoutineUpdate());
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

TEST_F(CrosHealthdServiceConnectionTest, RunUrandomRoutine) {
  // Test that we can run the urandom routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthdClient::Get()->SetRunRoutineResponseForTesting(response);
  bool callback_done = false;
  ServiceConnection::GetInstance()->RunUrandomRoutine(
      /*length_seconds=*/10,
      base::BindOnce(
          [](bool* callback_done, mojom::RunRoutineResponsePtr response) {
            EXPECT_EQ(response, MakeRunRoutineResponse());
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

TEST_F(CrosHealthdServiceConnectionTest, RunBatteryCapacityRoutine) {
  // Test that we can run the battery capacity routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthdClient::Get()->SetRunRoutineResponseForTesting(response);
  bool callback_done = false;
  ServiceConnection::GetInstance()->RunBatteryCapacityRoutine(
      /*low_mah=*/1001, /*high_mah=*/120345,
      base::BindOnce(
          [](bool* callback_done, mojom::RunRoutineResponsePtr response) {
            EXPECT_EQ(response, MakeRunRoutineResponse());
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

TEST_F(CrosHealthdServiceConnectionTest, RunBatteryHealthRoutine) {
  // Test that we can run the battery health routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthdClient::Get()->SetRunRoutineResponseForTesting(response);
  bool callback_done = false;
  ServiceConnection::GetInstance()->RunBatteryHealthRoutine(
      /*maximum_cycle_count=*/2, /*percent_battery_wear_allowed=*/90,
      base::BindOnce(
          [](bool* callback_done, mojom::RunRoutineResponsePtr response) {
            EXPECT_EQ(response, MakeRunRoutineResponse());
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

TEST_F(CrosHealthdServiceConnectionTest, RunSmartctlCheckRoutine) {
  // Test that we can run the smartctl check routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthdClient::Get()->SetRunRoutineResponseForTesting(response);
  bool callback_done = false;
  ServiceConnection::GetInstance()->RunSmartctlCheckRoutine(base::BindOnce(
      [](bool* callback_done, mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        *callback_done = true;
      },
      &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

TEST_F(CrosHealthdServiceConnectionTest, RunAcPowerRoutine) {
  // Test that we can run the AC power routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthdClient::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunAcPowerRoutine(
      mojom::AcPowerStatusEnum::kConnected,
      /*expected_power_type=*/"power_type",
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(CrosHealthdServiceConnectionTest, RunCpuCacheRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthdClient::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunCpuCacheRoutine(
      base::TimeDelta().FromSeconds(10),
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(CrosHealthdServiceConnectionTest, RunCpuStressRoutine) {
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthdClient::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunCpuStressRoutine(
      base::TimeDelta().FromSeconds(10),
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(CrosHealthdServiceConnectionTest, RunFloatingPointAccuracyRoutine) {
  // Test that we can run the floating point accuracy routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthdClient::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunFloatingPointAccuracyRoutine(
      /*exec_duration=*/base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(CrosHealthdServiceConnectionTest, RunNvmeWearLevelRoutine) {
  // Test that we can run the NVMe wear-level routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthdClient::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunNvmeWearLevelRoutine(
      /*wear_level_threshold=*/50,
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(CrosHealthdServiceConnectionTest, RunNvmeSelfTestRoutine) {
  // Test that we can run the NVMe self-test routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthdClient::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunNvmeSelfTestRoutine(
      mojom::NvmeSelfTestTypeEnum::kShortSelfTest,
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(CrosHealthdServiceConnectionTest, RunDiskReadRoutine) {
  // Test that we can run the disk read routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthdClient::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  base::TimeDelta exec_duration = base::TimeDelta().FromSeconds(10);
  ServiceConnection::GetInstance()->RunDiskReadRoutine(
      mojom::DiskReadRoutineTypeEnum::kLinearRead,
      /*exec_duration=*/exec_duration, /*file_size_mb=*/1024,
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(CrosHealthdServiceConnectionTest, RunPrimeSearchRoutine) {
  // Test that we can run the prime search routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthdClient::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  base::TimeDelta exec_duration = base::TimeDelta().FromSeconds(10);
  ServiceConnection::GetInstance()->RunPrimeSearchRoutine(
      /*exec_duration=*/exec_duration, /*max_num=*/1000000,
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(CrosHealthdServiceConnectionTest, RunBatteryDischargeRoutine) {
  // Test that we can run the battery discharge routine.
  auto response = MakeRunRoutineResponse();
  FakeCrosHealthdClient::Get()->SetRunRoutineResponseForTesting(response);
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()->RunBatteryDischargeRoutine(
      /*exec_duration=*/base::TimeDelta::FromSeconds(12),
      /*maximum_discharge_percent_allowed=*/99,
      base::BindLambdaForTesting([&](mojom::RunRoutineResponsePtr response) {
        EXPECT_EQ(response, MakeRunRoutineResponse());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(CrosHealthdServiceConnectionTest, ProbeTelemetryInfo) {
  // Test that we can send a request without categories.
  auto empty_info = mojom::TelemetryInfo::New();
  FakeCrosHealthdClient::Get()->SetProbeTelemetryInfoResponseForTesting(
      empty_info);
  const std::vector<mojom::ProbeCategoryEnum> no_categories = {};
  bool callback_done = false;
  ServiceConnection::GetInstance()->ProbeTelemetryInfo(
      no_categories, base::BindOnce(
                         [](bool* callback_done, mojom::TelemetryInfoPtr info) {
                           EXPECT_EQ(info, mojom::TelemetryInfo::New());
                           *callback_done = true;
                         },
                         &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);

  // Test that we can request all categories.
  auto response_info = MakeTelemetryInfo();
  FakeCrosHealthdClient::Get()->SetProbeTelemetryInfoResponseForTesting(
      response_info);
  const std::vector<mojom::ProbeCategoryEnum> categories_to_test = {
      mojom::ProbeCategoryEnum::kBattery,
      mojom::ProbeCategoryEnum::kNonRemovableBlockDevices,
      mojom::ProbeCategoryEnum::kCachedVpdData,
      mojom::ProbeCategoryEnum::kCpu,
      mojom::ProbeCategoryEnum::kTimezone,
      mojom::ProbeCategoryEnum::kMemory,
      mojom::ProbeCategoryEnum::kBacklight,
      mojom::ProbeCategoryEnum::kFan};
  callback_done = false;
  ServiceConnection::GetInstance()->ProbeTelemetryInfo(
      categories_to_test,
      base::BindOnce(
          [](bool* callback_done, mojom::TelemetryInfoPtr info) {
            EXPECT_EQ(info, MakeTelemetryInfo());
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);
}

}  // namespace
}  // namespace cros_healthd
}  // namespace chromeos
