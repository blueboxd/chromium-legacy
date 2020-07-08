// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/probe_service_converters.h"

#include <cstdint>
#include <vector>

#include "chromeos/components/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace chromeos {
namespace probe_service_converters {

TEST(ProbeServiceConvertors, ConvertCategoryVector) {
  const std::vector<health::mojom::ProbeCategoryEnum> kInput{
      health::mojom::ProbeCategoryEnum::kBattery,
      health::mojom::ProbeCategoryEnum::kNonRemovableBlockDevices,
      health::mojom::ProbeCategoryEnum::kCachedVpdData,
      health::mojom::ProbeCategoryEnum::kCpu,
      health::mojom::ProbeCategoryEnum::kTimezone,
      health::mojom::ProbeCategoryEnum::kMemory,
      health::mojom::ProbeCategoryEnum::kBacklight,
      health::mojom::ProbeCategoryEnum::kFan,
      health::mojom::ProbeCategoryEnum::kStatefulPartition,
      health::mojom::ProbeCategoryEnum::kBluetooth};
  EXPECT_THAT(
      ConvertCategoryVector(kInput),
      ElementsAre(
          cros_healthd::mojom::ProbeCategoryEnum::kBattery,
          cros_healthd::mojom::ProbeCategoryEnum::kNonRemovableBlockDevices,
          cros_healthd::mojom::ProbeCategoryEnum::kCachedVpdData,
          cros_healthd::mojom::ProbeCategoryEnum::kCpu,
          cros_healthd::mojom::ProbeCategoryEnum::kTimezone,
          cros_healthd::mojom::ProbeCategoryEnum::kMemory,
          cros_healthd::mojom::ProbeCategoryEnum::kBacklight,
          cros_healthd::mojom::ProbeCategoryEnum::kFan,
          cros_healthd::mojom::ProbeCategoryEnum::kStatefulPartition,
          cros_healthd::mojom::ProbeCategoryEnum::kBluetooth));
}

// Tests that |ConvertPtr| function returns nullptr if input is nullptr.
// ConvertPtr is a template, so we can test this function with any valid type.
TEST(ProbeServiceConvertors, ConvertPtrTakesNullPtr) {
  EXPECT_TRUE(ConvertPtr(cros_healthd::mojom::ProbeErrorPtr()).is_null());
}

TEST(ProbeServiceConvertors, ErrorType) {
  EXPECT_EQ(Convert(cros_healthd::mojom::ErrorType::kFileReadError),
            health::mojom::ErrorType::kFileReadError);

  EXPECT_EQ(Convert(cros_healthd::mojom::ErrorType::kParseError),
            health::mojom::ErrorType::kParseError);

  EXPECT_EQ(Convert(cros_healthd::mojom::ErrorType::kSystemUtilityError),
            health::mojom::ErrorType::kSystemUtilityError);
}

TEST(ProbeServiceConvertors, ProbeErrorPtr) {
  constexpr char kMsg[] = "file not found";
  EXPECT_EQ(ConvertPtr(cros_healthd::mojom::ProbeError::New(
                cros_healthd::mojom::ErrorType::kFileReadError, kMsg)),
            health::mojom::ProbeError::New(
                health::mojom::ErrorType::kFileReadError, kMsg));
}

TEST(ProbeServiceConvertors, BoolValue) {
  EXPECT_EQ(Convert(false), health::mojom::BoolValue::New(false));
  EXPECT_EQ(Convert(true), health::mojom::BoolValue::New(true));
}

TEST(ProbeServiceConvertors, DoubleValue) {
  constexpr double kValue = 100500.500100;
  EXPECT_EQ(Convert(kValue), health::mojom::DoubleValue::New(kValue));
}

TEST(ProbeServiceConvertors, Int64Value) {
  constexpr int64_t kValue = -100500;
  EXPECT_EQ(Convert(kValue), health::mojom::Int64Value::New(kValue));
}

TEST(ProbeServiceConvertors, UInt64Value) {
  constexpr uint64_t kValue = 100500;
  EXPECT_EQ(Convert(kValue), health::mojom::UInt64Value::New(kValue));
}

TEST(ProbeServiceConvertors, UInt64ValuePtr) {
  constexpr uint64_t kValue = 100500;
  EXPECT_EQ(ConvertPtr(cros_healthd::mojom::UInt64Value::New(kValue)),
            health::mojom::UInt64Value::New(kValue));
}

TEST(ProbeServiceConvertors, BatteryInfoPtr) {
  constexpr int64_t kCycleCount = 512;
  constexpr double kVoltageNow = 10.2;
  constexpr char kVendor[] = "Google";
  constexpr char kSerialNumber[] = "ABCDEF123456";
  constexpr double kChargeFullDesign = 1000.3;
  constexpr double kChargeFull = 999.0;
  constexpr double kVoltageMinDesign = 41.1;
  constexpr char kModelName[] = "Google Battery";
  constexpr double kChargeNow = 20.1;
  constexpr double kCurrentNow = 15.2;
  constexpr char kTechnology[] = "FastCharge";
  constexpr char kStatus[] = "Charging";
  constexpr char kManufactureDate[] = "2018-10-01";
  constexpr uint64_t kTemperature = 3097;

  // Here we don't use cros_healthd::mojom::BatteryInfo::New because BatteryInfo
  // may contain some fields that we don't use yet.
  auto battery_info = cros_healthd::mojom::BatteryInfo::New();
  battery_info->cycle_count = kCycleCount;
  battery_info->voltage_now = kVoltageNow;
  battery_info->vendor = kVendor;
  battery_info->serial_number = kSerialNumber;
  battery_info->charge_full_design = kChargeFullDesign;
  battery_info->charge_full = kChargeFull;
  battery_info->voltage_min_design = kVoltageMinDesign;
  battery_info->model_name = kModelName;
  battery_info->charge_now = kChargeNow;
  battery_info->current_now = kCurrentNow;
  battery_info->technology = kTechnology;
  battery_info->status = kStatus;
  battery_info->manufacture_date = kManufactureDate;
  battery_info->temperature =
      cros_healthd::mojom::UInt64Value::New(kTemperature);

  // Here we intentionaly use health::mojom::BatteryInfo::New not to
  // forget to test new fields.
  EXPECT_EQ(
      ConvertPtr(battery_info.Clone()),
      health::mojom::BatteryInfo::New(
          health::mojom::Int64Value::New(kCycleCount),
          health::mojom::DoubleValue::New(kVoltageNow), kVendor, kSerialNumber,
          health::mojom::DoubleValue::New(kChargeFullDesign),
          health::mojom::DoubleValue::New(kChargeFull),
          health::mojom::DoubleValue::New(kVoltageMinDesign), kModelName,
          health::mojom::DoubleValue::New(kChargeNow),
          health::mojom::DoubleValue::New(kCurrentNow), kTechnology, kStatus,
          kManufactureDate, health::mojom::UInt64Value::New(kTemperature)));
}

TEST(ProbeServiceConvertors, BatteryResultPtrInfo) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::BatteryResult::NewBatteryInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_battery_info());
}

TEST(ProbeServiceConvertors, BatteryResultPtrError) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::BatteryResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, NonRemovableBlockDeviceInfoPtr) {
  constexpr char kPath[] = "/dev/device1";
  constexpr uint64_t kSize = 1000000000;
  constexpr char kType[] = "NVMe";
  constexpr uint8_t kManufacturerId = 200;
  constexpr char kName[] = "goog";
  constexpr uint32_t kSerial = 0xaabbccdd;
  constexpr uint64_t kBytesReadSinceLastBoot = 10;
  constexpr uint64_t kBytesWrittenSinceLastBoot = 100;
  constexpr uint64_t kReadTimeSecondsSinceLastBoot = 1000;
  constexpr uint64_t kWriteTimeSecondsSinceLastBoot = 10000;
  constexpr uint64_t kIoTimeSecondsSinceLastBoot = 100000;
  constexpr uint64_t kDiscardTimeSecondsSinceLastBoot = 1000000;

  // Here we don't use cros_healthd::mojom::NonRemovableBlockDeviceInfo::New
  // because NonRemovableBlockDeviceInfo may contain some fields that we
  // don't use yet.
  auto info = cros_healthd::mojom::NonRemovableBlockDeviceInfo::New();

  info->path = kPath;
  info->size = kSize;
  info->type = kType;
  info->manufacturer_id = kManufacturerId;
  info->name = kName;
  info->serial = kSerial;
  info->bytes_read_since_last_boot = kBytesReadSinceLastBoot;
  info->bytes_written_since_last_boot = kBytesWrittenSinceLastBoot;
  info->read_time_seconds_since_last_boot = kReadTimeSecondsSinceLastBoot;
  info->write_time_seconds_since_last_boot = kWriteTimeSecondsSinceLastBoot;
  info->io_time_seconds_since_last_boot = kIoTimeSecondsSinceLastBoot;
  info->discard_time_seconds_since_last_boot =
      cros_healthd::mojom::UInt64Value::New(kDiscardTimeSecondsSinceLastBoot);

  // Here we intentionaly use health::mojom::NonRemovableBlockDeviceInfo::New
  // not to forget to test new fields.
  EXPECT_EQ(
      ConvertPtr(info.Clone()),
      health::mojom::NonRemovableBlockDeviceInfo::New(
          kPath, health::mojom::UInt64Value::New(kSize), kType,
          health::mojom::UInt32Value::New(kManufacturerId), kName,
          health::mojom::UInt32Value::New(kSerial),
          health::mojom::UInt64Value::New(kBytesReadSinceLastBoot),
          health::mojom::UInt64Value::New(kBytesWrittenSinceLastBoot),
          health::mojom::UInt64Value::New(kReadTimeSecondsSinceLastBoot),
          health::mojom::UInt64Value::New(kWriteTimeSecondsSinceLastBoot),
          health::mojom::UInt64Value::New(kIoTimeSecondsSinceLastBoot),
          health::mojom::UInt64Value::New(kDiscardTimeSecondsSinceLastBoot)));
}

TEST(ProbeServiceConvertors, NonRemovableBlockDeviceResultPtrInfo) {
  constexpr char kPath1[] = "Path1";
  constexpr char kPath2[] = "Path2";

  std::vector<cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr> infos;
  {
    auto info1 = cros_healthd::mojom::NonRemovableBlockDeviceInfo::New();
    info1->path = kPath1;

    auto info2 = cros_healthd::mojom::NonRemovableBlockDeviceInfo::New();
    info2->path = kPath2;

    infos.push_back(std::move(info1));
    infos.push_back(std::move(info2));
  }

  const auto output = ConvertPtr(
      cros_healthd::mojom::NonRemovableBlockDeviceResult::NewBlockDeviceInfo(
          std::move(infos)));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_block_device_info());
  ASSERT_EQ(output->get_block_device_info().size(), 2ULL);
  EXPECT_EQ(output->get_block_device_info()[0]->path, kPath1);
  EXPECT_EQ(output->get_block_device_info()[1]->path, kPath2);
}

TEST(ProbeServiceConvertors, NonRemovableBlockDeviceResultPtrError) {
  const health::mojom::NonRemovableBlockDeviceResultPtr output = ConvertPtr(
      cros_healthd::mojom::NonRemovableBlockDeviceResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, CachedVpdInfoPtr) {
  constexpr char kSkuNumber[] = "sku-1";

  auto input = cros_healthd::mojom::CachedVpdInfo::New();
  input->sku_number = kSkuNumber;

  EXPECT_EQ(ConvertPtr(std::move(input)),
            health::mojom::CachedVpdInfo::New(kSkuNumber));
}

TEST(ProbeServiceConvertors, CachedVpdResultPtrInfo) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::CachedVpdResult::NewVpdInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_vpd_info());
}

TEST(ProbeServiceConvertors, CachedVpdResultPtrError) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::CachedVpdResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, CpuCStateInfoPtr) {
  constexpr char kName[] = "C0";
  constexpr uint64_t kTimeInStateSinceLastBootUs = 123456;

  auto input = cros_healthd::mojom::CpuCStateInfo::New();
  {
    input->name = kName;
    input->time_in_state_since_last_boot_us = kTimeInStateSinceLastBootUs;
  }

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  EXPECT_EQ(output->name, kName);
  EXPECT_EQ(output->time_in_state_since_last_boot_us,
            health::mojom::UInt64Value::New(kTimeInStateSinceLastBootUs));
}

TEST(ProbeServiceConvertors, LogicalCpuInfoPtr) {
  constexpr uint32_t kMaxClockSpeedKhz = 1000;
  constexpr uint32_t kScalingMaxFrequencyKhz = 10000;
  constexpr uint32_t kScalingCurrentFrequencyKhz = 100000;
  constexpr uint32_t kIdleTimeUserHz = 1000000;

  constexpr char kCpuCStateName[] = "C1";

  auto input = cros_healthd::mojom::LogicalCpuInfo::New();
  {
    auto c_state = cros_healthd::mojom::CpuCStateInfo::New();
    c_state->name = kCpuCStateName;

    input->max_clock_speed_khz = kMaxClockSpeedKhz;
    input->scaling_max_frequency_khz = kScalingMaxFrequencyKhz;
    input->scaling_current_frequency_khz = kScalingCurrentFrequencyKhz;
    input->idle_time_user_hz = kIdleTimeUserHz;
    input->c_states.push_back(std::move(c_state));
  }

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  EXPECT_EQ(output->max_clock_speed_khz,
            health::mojom::UInt32Value::New(kMaxClockSpeedKhz));
  EXPECT_EQ(output->scaling_max_frequency_khz,
            health::mojom::UInt32Value::New(kScalingMaxFrequencyKhz));
  EXPECT_EQ(output->scaling_current_frequency_khz,
            health::mojom::UInt32Value::New(kScalingCurrentFrequencyKhz));
  EXPECT_EQ(output->idle_time_user,
            health::mojom::UInt64Value::New(kIdleTimeUserHz));
  ASSERT_EQ(output->c_states.size(), 1ULL);
  ASSERT_TRUE(output->c_states[0]);
  EXPECT_EQ(output->c_states[0]->name, kCpuCStateName);
}

TEST(ProbeServiceConvertors, PhysicalCpuInfoPtr) {
  constexpr char kModelName[] = "i9";
  constexpr uint32_t kMaxClockSpeedKhz = 1000;

  auto input = cros_healthd::mojom::PhysicalCpuInfo::New();
  {
    auto logical_info = cros_healthd::mojom::LogicalCpuInfo::New();
    logical_info->max_clock_speed_khz = kMaxClockSpeedKhz;

    input->model_name = kModelName;
    input->logical_cpus.push_back(std::move(logical_info));
  }

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  ASSERT_EQ(output->model_name, kModelName);
  ASSERT_EQ(output->logical_cpus.size(), 1ULL);
  ASSERT_TRUE(output->logical_cpus[0]);
  EXPECT_EQ(output->logical_cpus[0]->max_clock_speed_khz,
            health::mojom::UInt32Value::New(kMaxClockSpeedKhz));
}

TEST(ProbeServiceConvertors, CpuArchitectureEnum) {
  EXPECT_EQ(Convert(cros_healthd::mojom::CpuArchitectureEnum::kUnknown),
            health::mojom::CpuArchitectureEnum::kUnknown);
  EXPECT_EQ(Convert(cros_healthd::mojom::CpuArchitectureEnum::kX86_64),
            health::mojom::CpuArchitectureEnum::kX86_64);
  EXPECT_EQ(Convert(cros_healthd::mojom::CpuArchitectureEnum::kAArch64),
            health::mojom::CpuArchitectureEnum::kAArch64);
  EXPECT_EQ(Convert(cros_healthd::mojom::CpuArchitectureEnum::kArmv7l),
            health::mojom::CpuArchitectureEnum::kArmv7l);
}

TEST(ProbeServiceConvertors, CpuInfoPtr) {
  constexpr uint32_t kNumTotalThreads = 16;
  constexpr char kModelName[] = "i9";

  auto input = cros_healthd::mojom::CpuInfo::New();
  {
    auto physical_info = cros_healthd::mojom::PhysicalCpuInfo::New();
    physical_info->model_name = kModelName;

    input->num_total_threads = kNumTotalThreads;
    input->physical_cpus.push_back(std::move(physical_info));
  }

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  ASSERT_EQ(output->num_total_threads,
            health::mojom::UInt32Value::New(kNumTotalThreads));
  ASSERT_EQ(output->physical_cpus.size(), 1ULL);
  ASSERT_TRUE(output->physical_cpus[0]);
  EXPECT_EQ(output->physical_cpus[0]->model_name, kModelName);
}

TEST(ProbeServiceConvertors, CpuResultPtrInfo) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::CpuResult::NewCpuInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_cpu_info());
}

TEST(ProbeServiceConvertors, CpuResultPtrError) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::CpuResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, TimezoneInfoPtr) {
  constexpr char kPosix[] = "TZ=CST6CDT,M3.2.0/2:00:00,M11.1.0/2:00:00";
  constexpr char kRegion[] = "Europe/Berlin";

  auto input = cros_healthd::mojom::TimezoneInfo::New();
  input->posix = kPosix;
  input->region = kRegion;

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  EXPECT_EQ(output->posix, kPosix);
  EXPECT_EQ(output->region, kRegion);
}

TEST(ProbeServiceConvertors, TimezoneResultPtrInfo) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::TimezoneResult::NewTimezoneInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_timezone_info());
}

TEST(ProbeServiceConvertors, TimezoneResultPtrError) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::TimezoneResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, MemoryInfoPtr) {
  constexpr uint32_t kTotalMemoryKib = 100000;
  constexpr uint32_t kFreeMemoryKib = 10000;
  constexpr uint32_t kAvailableMemoryKib = 1000;
  constexpr uint32_t kPageFaultsSinceLastBoot = 100;

  auto input = cros_healthd::mojom::MemoryInfo::New();
  input->total_memory_kib = kTotalMemoryKib;
  input->free_memory_kib = kFreeMemoryKib;
  input->available_memory_kib = kAvailableMemoryKib;
  input->page_faults_since_last_boot = kPageFaultsSinceLastBoot;

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  EXPECT_EQ(output->total_memory_kib,
            health::mojom::UInt32Value::New(kTotalMemoryKib));
  EXPECT_EQ(output->free_memory_kib,
            health::mojom::UInt32Value::New(kFreeMemoryKib));
  EXPECT_EQ(output->available_memory_kib,
            health::mojom::UInt32Value::New(kAvailableMemoryKib));
  EXPECT_EQ(output->page_faults_since_last_boot,
            health::mojom::UInt64Value::New(kPageFaultsSinceLastBoot));
}

TEST(ProbeServiceConvertors, MemoryResultPtrInfo) {
  const health::mojom::MemoryResultPtr output =
      ConvertPtr(cros_healthd::mojom::MemoryResult::NewMemoryInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_memory_info());
}

TEST(ProbeServiceConvertors, MemoryResultPtrError) {
  const health::mojom::MemoryResultPtr output =
      ConvertPtr(cros_healthd::mojom::MemoryResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, BacklightInfoPtr) {
  constexpr char kPath[] = "/sys/backlight";
  constexpr uint32_t kMaxBrightness = 100000;
  constexpr uint32_t kBrightness = 90000;

  auto input = cros_healthd::mojom::BacklightInfo::New();
  input->path = kPath;
  input->max_brightness = kMaxBrightness;
  input->brightness = kBrightness;

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  EXPECT_EQ(output->path, kPath);
  EXPECT_EQ(output->max_brightness,
            health::mojom::UInt32Value::New(kMaxBrightness));
  EXPECT_EQ(output->brightness, health::mojom::UInt32Value::New(kBrightness));
}

TEST(ProbeServiceConvertors, BacklightResultPtrInfo) {
  constexpr char kPath[] = "/sys/backlight";

  cros_healthd::mojom::BacklightResultPtr input;
  {
    auto backlight_info = cros_healthd::mojom::BacklightInfo::New();
    backlight_info->path = kPath;

    std::vector<cros_healthd::mojom::BacklightInfoPtr> backlight_infos;
    backlight_infos.push_back(std::move(backlight_info));

    input = cros_healthd::mojom::BacklightResult::NewBacklightInfo(
        std::move(backlight_infos));
  }

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  ASSERT_TRUE(output->is_backlight_info());

  const auto& backlight_info_output = output->get_backlight_info();
  ASSERT_EQ(backlight_info_output.size(), 1ULL);
  ASSERT_TRUE(backlight_info_output[0]);
  EXPECT_EQ(backlight_info_output[0]->path, kPath);
}

TEST(ProbeServiceConvertors, BacklightResultPtrError) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::BacklightResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, FanInfoPtr) {
  constexpr uint32_t kSpeedRpm = 1000;

  auto input = cros_healthd::mojom::FanInfo::New();
  input->speed_rpm = kSpeedRpm;

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  EXPECT_EQ(output->speed_rpm, health::mojom::UInt32Value::New(kSpeedRpm));
}

TEST(ProbeServiceConvertors, FanResultPtrInfo) {
  constexpr uint32_t kSpeedRpm = 1000;

  cros_healthd::mojom::FanResultPtr input;
  {
    auto fan_info = cros_healthd::mojom::FanInfo::New();
    fan_info->speed_rpm = kSpeedRpm;

    std::vector<cros_healthd::mojom::FanInfoPtr> fan_infos;
    fan_infos.push_back(std::move(fan_info));

    input = cros_healthd::mojom::FanResult::NewFanInfo(std::move(fan_infos));
  }

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  ASSERT_TRUE(output->is_fan_info());

  const auto& fan_info_output = output->get_fan_info();
  ASSERT_EQ(fan_info_output.size(), 1ULL);
  ASSERT_TRUE(fan_info_output[0]);
  EXPECT_EQ(fan_info_output[0]->speed_rpm,
            health::mojom::UInt32Value::New(kSpeedRpm));
}

TEST(ProbeServiceConvertors, FanResultPtrError) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::FanResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, StatefulPartitionInfoPtr) {
  constexpr uint64_t k100MiB = 100 * 1024 * 1024;
  constexpr uint64_t kTotalSpace = 9000 * k100MiB + 17;
  constexpr uint64_t kRoundedAvailableSpace = 1000 * k100MiB;
  constexpr uint64_t kAvailableSpace = kRoundedAvailableSpace + 2000;

  auto input = cros_healthd::mojom::StatefulPartitionInfo::New();
  input->available_space = kAvailableSpace;
  input->total_space = kTotalSpace;

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  EXPECT_EQ(output->available_space,
            health::mojom::UInt64Value::New(kRoundedAvailableSpace));
  EXPECT_EQ(output->total_space, health::mojom::UInt64Value::New(kTotalSpace));
}

TEST(ProbeServiceConvertors, StatefulPartitionResultPtrInfo) {
  const auto output = ConvertPtr(
      cros_healthd::mojom::StatefulPartitionResult::NewPartitionInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_partition_info());
}

TEST(ProbeServiceConvertors, StatefulPartitionResultPtrError) {
  const auto output = ConvertPtr(
      cros_healthd::mojom::StatefulPartitionResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, BluetoothAdapterInfoPtr) {
  constexpr char kName[] = "hci0";
  constexpr char kAddress[] = "ab:cd:ef:12:34:56";
  constexpr bool kPowered = true;
  constexpr uint32_t kNumConnectedDevices = 3;

  auto input = cros_healthd::mojom::BluetoothAdapterInfo::New();
  input->name = kName;
  input->address = kAddress;
  input->powered = kPowered;
  input->num_connected_devices = kNumConnectedDevices;

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  EXPECT_EQ(output->name, kName);
  EXPECT_EQ(output->address, kAddress);
  EXPECT_EQ(output->powered, health::mojom::BoolValue::New(kPowered));
  EXPECT_EQ(output->num_connected_devices,
            health::mojom::UInt32Value::New(kNumConnectedDevices));
}

TEST(ProbeServiceConvertors, BluetoothResultPtrInfo) {
  constexpr char kName[] = "hci0";

  cros_healthd::mojom::BluetoothResultPtr input;
  {
    auto info = cros_healthd::mojom::BluetoothAdapterInfo::New();
    info->name = kName;

    std::vector<cros_healthd::mojom::BluetoothAdapterInfoPtr> infos;
    infos.push_back(std::move(info));

    input = cros_healthd::mojom::BluetoothResult::NewBluetoothAdapterInfo(
        std::move(infos));
  }

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  ASSERT_TRUE(output->is_bluetooth_adapter_info());

  const auto& bluetooth_adapter_info_output =
      output->get_bluetooth_adapter_info();
  ASSERT_EQ(bluetooth_adapter_info_output.size(), 1ULL);
  ASSERT_TRUE(bluetooth_adapter_info_output[0]);
  EXPECT_EQ(bluetooth_adapter_info_output[0]->name, kName);
}

TEST(ProbeServiceConvertors, BluetoothResultPtrError) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::BluetoothResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, TelemetryInfoPtrWithNotNullFields) {
  auto input = cros_healthd::mojom::TelemetryInfo::New();
  input->battery_result = cros_healthd::mojom::BatteryResult::New();
  input->block_device_result =
      cros_healthd::mojom::NonRemovableBlockDeviceResult::New();
  input->vpd_result = cros_healthd::mojom::CachedVpdResult::New();
  input->cpu_result = cros_healthd::mojom::CpuResult::New();
  input->timezone_result = cros_healthd::mojom::TimezoneResult::New();
  input->memory_result = cros_healthd::mojom::MemoryResult::New();
  input->backlight_result = cros_healthd::mojom::BacklightResult::New();
  input->fan_result = cros_healthd::mojom::FanResult::New();
  input->stateful_partition_result =
      cros_healthd::mojom::StatefulPartitionResult::New();
  input->bluetooth_result = cros_healthd::mojom::BluetoothResult::New();

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->battery_result);
  EXPECT_TRUE(output->block_device_result);
  EXPECT_TRUE(output->vpd_result);
  EXPECT_TRUE(output->cpu_result);
  EXPECT_TRUE(output->timezone_result);
  EXPECT_TRUE(output->memory_result);
  EXPECT_TRUE(output->backlight_result);
  EXPECT_TRUE(output->fan_result);
  EXPECT_TRUE(output->stateful_partition_result);
  EXPECT_TRUE(output->bluetooth_result);
}

TEST(ProbeServiceConvertors, TelemetryInfoPtrWithNullFields) {
  const auto output = ConvertPtr(cros_healthd::mojom::TelemetryInfo::New());
  ASSERT_TRUE(output);
  EXPECT_FALSE(output->battery_result);
  EXPECT_FALSE(output->block_device_result);
  EXPECT_FALSE(output->vpd_result);
  EXPECT_FALSE(output->cpu_result);
  EXPECT_FALSE(output->timezone_result);
  EXPECT_FALSE(output->memory_result);
  EXPECT_FALSE(output->backlight_result);
  EXPECT_FALSE(output->fan_result);
  EXPECT_FALSE(output->stateful_partition_result);
  EXPECT_FALSE(output->bluetooth_result);
}

}  // namespace probe_service_converters
}  // namespace chromeos
