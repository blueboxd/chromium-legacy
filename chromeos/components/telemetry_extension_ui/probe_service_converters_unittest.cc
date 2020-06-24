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

TEST(ProbeServiceConvertors, ProbeCategoryEnum) {
  EXPECT_EQ(Convert(health::mojom::ProbeCategoryEnum::kBattery),
            cros_healthd::mojom::ProbeCategoryEnum::kBattery);
}

TEST(ProbeServiceConvertors, ProbeCategoryEnumVector) {
  const std::vector<health::mojom::ProbeCategoryEnum> kInput{
      health::mojom::ProbeCategoryEnum::kBattery};
  EXPECT_THAT(Convert(kInput),
              ElementsAre(cros_healthd::mojom::ProbeCategoryEnum::kBattery));
}

TEST(ProbeServiceConvertors, ErrorType) {
  EXPECT_EQ(Convert(cros_healthd::mojom::ErrorType::kFileReadError),
            health::mojom::ErrorType::kFileReadError);

  EXPECT_EQ(Convert(cros_healthd::mojom::ErrorType::kParseError),
            health::mojom::ErrorType::kParseError);

  EXPECT_EQ(Convert(cros_healthd::mojom::ErrorType::kSystemUtilityError),
            health::mojom::ErrorType::kSystemUtilityError);
}

TEST(ProbeServiceConvertors, ProbeErrorPtrNull) {
  EXPECT_TRUE(Convert(cros_healthd::mojom::ProbeErrorPtr()).is_null());
}

TEST(ProbeServiceConvertors, ProbeErrorPtr) {
  constexpr char kMsg[] = "file not found";
  EXPECT_EQ(Convert(cros_healthd::mojom::ProbeError::New(
                cros_healthd::mojom::ErrorType::kFileReadError, kMsg)),
            health::mojom::ProbeError::New(
                health::mojom::ErrorType::kFileReadError, kMsg));
}

TEST(ProbeServiceConvertors, DoubleValuePtr) {
  constexpr double kValue = 100500.500100;
  EXPECT_EQ(Convert(kValue), health::mojom::DoubleValue::New(kValue));
}

TEST(ProbeServiceConvertors, Int64ValuePtr) {
  constexpr int64_t kValue = 100500;
  EXPECT_EQ(Convert(kValue), health::mojom::Int64Value::New(kValue));
}

TEST(ProbeServiceConvertors, UInt64ValuePtrNull) {
  EXPECT_TRUE(Convert(cros_healthd::mojom::UInt64ValuePtr()).is_null());
}

TEST(ProbeServiceConvertors, UInt64ValuePtr) {
  constexpr uint64_t kValue = -100500;
  EXPECT_EQ(Convert(cros_healthd::mojom::UInt64Value::New(kValue)),
            health::mojom::UInt64Value::New(kValue));
}

TEST(ProbeServiceConvertors, BatteryInfoPtrNull) {
  EXPECT_TRUE(Convert(cros_healthd::mojom::BatteryInfoPtr()).is_null());
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

  // Here we intentionaly use health::mojom::BatteryInfo::New to don't
  // forget to test new fields.
  EXPECT_EQ(
      Convert(battery_info.Clone()),
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

TEST(ProbeServiceConvertors, BatteryResultPtrNull) {
  EXPECT_TRUE(Convert(cros_healthd::mojom::BatteryResultPtr()).is_null());
}

TEST(ProbeServiceConvertors, BatteryResultPtrInfo) {
  const health::mojom::BatteryResultPtr ptr =
      Convert(cros_healthd::mojom::BatteryResult::NewBatteryInfo(nullptr));
  ASSERT_TRUE(ptr);
  EXPECT_TRUE(ptr->is_battery_info());
}

TEST(ProbeServiceConvertors, BatteryResultPtrError) {
  const health::mojom::BatteryResultPtr ptr =
      Convert(cros_healthd::mojom::BatteryResult::NewError(nullptr));
  ASSERT_TRUE(ptr);
  EXPECT_TRUE(ptr->is_error());
}

TEST(ProbeServiceConvertors, TelemetryInfoPtrHasBatteryResult) {
  constexpr int64_t kCycleCount = 1;

  auto battery_info_input = cros_healthd::mojom::BatteryInfo::New();
  battery_info_input->cycle_count = kCycleCount;

  auto telemetry_info_input = cros_healthd::mojom::TelemetryInfo::New();

  telemetry_info_input->battery_result =
      cros_healthd::mojom::BatteryResult::NewBatteryInfo(
          std::move(battery_info_input));

  const health::mojom::TelemetryInfoPtr telemetry_info_output =
      Convert(std::move(telemetry_info_input));
  ASSERT_TRUE(telemetry_info_output);
  ASSERT_TRUE(telemetry_info_output->battery_result);
  ASSERT_TRUE(telemetry_info_output->battery_result->is_battery_info());
  ASSERT_TRUE(telemetry_info_output->battery_result->get_battery_info());
  ASSERT_TRUE(
      telemetry_info_output->battery_result->get_battery_info()->cycle_count);
  EXPECT_EQ(telemetry_info_output->battery_result->get_battery_info()
                ->cycle_count->value,
            kCycleCount);
}

TEST(ProbeServiceConvertors, TelemetryInfoPtrWithNullFields) {
  const health::mojom::TelemetryInfoPtr telemetry_info_output =
      Convert(cros_healthd::mojom::TelemetryInfo::New());
  ASSERT_TRUE(telemetry_info_output);
  EXPECT_FALSE(telemetry_info_output->battery_result);
}

TEST(ProbeServiceConvertors, TelemetryInfoPtrNull) {
  EXPECT_TRUE(Convert(cros_healthd::mojom::TelemetryInfoPtr()).is_null());
}

}  // namespace probe_service_converters
}  // namespace chromeos
