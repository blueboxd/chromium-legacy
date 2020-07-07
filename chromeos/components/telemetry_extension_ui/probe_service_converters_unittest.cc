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
      health::mojom::ProbeCategoryEnum::kCachedVpdData};
  EXPECT_THAT(
      ConvertCategoryVector(kInput),
      ElementsAre(
          cros_healthd::mojom::ProbeCategoryEnum::kBattery,
          cros_healthd::mojom::ProbeCategoryEnum::kNonRemovableBlockDevices,
          cros_healthd::mojom::ProbeCategoryEnum::kCachedVpdData));
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

TEST(ProbeServiceConvertors, UInt64ValuePtrNull) {
  EXPECT_TRUE(Convert(cros_healthd::mojom::UInt64ValuePtr()).is_null());
}

TEST(ProbeServiceConvertors, UInt64ValuePtr) {
  constexpr uint64_t kValue = 100500;
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

  // Here we intentionaly use health::mojom::BatteryInfo::New not to
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
      Convert(info.Clone()),
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

TEST(ProbeServiceConvertors, NonRemovableBlockDeviceResultPtrNull) {
  EXPECT_TRUE(Convert(cros_healthd::mojom::NonRemovableBlockDeviceResultPtr())
                  .is_null());
}

TEST(ProbeServiceConvertors, NonRemovableBlockDeviceResultPtrInfo) {
  constexpr char kPath1[] = "Path1";
  constexpr char kPath2[] = "Path2";

  auto info1 = cros_healthd::mojom::NonRemovableBlockDeviceInfo::New();
  info1->path = kPath1;

  auto info2 = cros_healthd::mojom::NonRemovableBlockDeviceInfo::New();
  info2->path = kPath2;

  std::vector<cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr> infos;
  infos.push_back(std::move(info1));
  infos.push_back(std::move(info2));

  const health::mojom::NonRemovableBlockDeviceResultPtr ptr = Convert(
      cros_healthd::mojom::NonRemovableBlockDeviceResult::NewBlockDeviceInfo(
          std::move(infos)));
  ASSERT_TRUE(ptr);
  EXPECT_TRUE(ptr->is_block_device_info());
  ASSERT_EQ(ptr->get_block_device_info().size(), 2ULL);
  EXPECT_EQ((ptr->get_block_device_info())[0]->path, kPath1);
  EXPECT_EQ((ptr->get_block_device_info())[1]->path, kPath2);
}

TEST(ProbeServiceConvertors, NonRemovableBlockDeviceResultPtrError) {
  const health::mojom::NonRemovableBlockDeviceResultPtr ptr = Convert(
      cros_healthd::mojom::NonRemovableBlockDeviceResult::NewError(nullptr));
  ASSERT_TRUE(ptr);
  EXPECT_TRUE(ptr->is_error());
}

TEST(ProbeServiceConvertors, CachedVpdInfoPtrNull) {
  EXPECT_TRUE(Convert(cros_healthd::mojom::CachedVpdInfoPtr()).is_null());
}

TEST(ProbeServiceConvertors, CachedVpdInfoPtr) {
  constexpr char kSkuNumber[] = "sku-1";

  // Here we don't use cros_healthd::mojom::CachedVpdInfo::New
  // because CachedVpdInfo may contain some fields that we
  // don't use yet.
  auto info = cros_healthd::mojom::CachedVpdInfo::New();

  info->sku_number = kSkuNumber;

  // Here we intentionaly use health::mojom::CachedVpdInfo::New
  // not to forget to test new fields.
  EXPECT_EQ(Convert(info.Clone()),
            health::mojom::CachedVpdInfo::New(kSkuNumber));
}

TEST(ProbeServiceConvertors, CachedVpdResultPtrNull) {
  EXPECT_TRUE(Convert(cros_healthd::mojom::CachedVpdResultPtr()).is_null());
}

TEST(ProbeServiceConvertors, CachedVpdResultPtrInfo) {
  const health::mojom::CachedVpdResultPtr ptr =
      Convert(cros_healthd::mojom::CachedVpdResult::NewVpdInfo(nullptr));
  ASSERT_TRUE(ptr);
  EXPECT_TRUE(ptr->is_vpd_info());
}

TEST(ProbeServiceConvertors, CachedVpdResultPtrError) {
  const health::mojom::CachedVpdResultPtr ptr =
      Convert(cros_healthd::mojom::CachedVpdResult::NewError(nullptr));
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

TEST(ProbeServiceConvertors, TelemetryInfoPtrHasBlockDeviceResult) {
  constexpr uint64_t kSize = 10000000;

  auto device_info = cros_healthd::mojom::NonRemovableBlockDeviceInfo::New();
  device_info->size = kSize;

  std::vector<cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr> device_infos;
  device_infos.push_back(std::move(device_info));

  auto input = cros_healthd::mojom::TelemetryInfo::New();
  input->block_device_result =
      cros_healthd::mojom::NonRemovableBlockDeviceResult::NewBlockDeviceInfo(
          std::move(device_infos));

  const health::mojom::TelemetryInfoPtr output = Convert(std::move(input));
  ASSERT_TRUE(output);
  ASSERT_TRUE(output->block_device_result);
  ASSERT_TRUE(output->block_device_result->is_block_device_info());

  const auto& device_info_output =
      output->block_device_result->get_block_device_info();
  ASSERT_EQ(device_info_output.size(), 1ULL);
  ASSERT_TRUE(device_info_output[0]);
  ASSERT_TRUE(device_info_output[0]->size);
  EXPECT_EQ(device_info_output[0]->size->value, kSize);
}

TEST(ProbeServiceConvertors, TelemetryInfoPtrHasCachedVpdResult) {
  constexpr char kSkuNumber[] = "sku-2";

  auto vpd_info_input = cros_healthd::mojom::CachedVpdInfo::New();
  vpd_info_input->sku_number = kSkuNumber;

  auto telemetry_info_input = cros_healthd::mojom::TelemetryInfo::New();

  telemetry_info_input->vpd_result =
      cros_healthd::mojom::CachedVpdResult::NewVpdInfo(
          std::move(vpd_info_input));

  const health::mojom::TelemetryInfoPtr telemetry_info_output =
      Convert(std::move(telemetry_info_input));
  ASSERT_TRUE(telemetry_info_output);
  ASSERT_TRUE(telemetry_info_output->vpd_result);
  ASSERT_TRUE(telemetry_info_output->vpd_result->is_vpd_info());

  const auto& vpd_info_output =
      telemetry_info_output->vpd_result->get_vpd_info();
  ASSERT_TRUE(vpd_info_output);
  ASSERT_TRUE(vpd_info_output->sku_number.has_value());
  EXPECT_EQ(vpd_info_output->sku_number.value(), kSkuNumber);
}

TEST(ProbeServiceConvertors, TelemetryInfoPtrWithNullFields) {
  const health::mojom::TelemetryInfoPtr telemetry_info_output =
      Convert(cros_healthd::mojom::TelemetryInfo::New());
  ASSERT_TRUE(telemetry_info_output);
  EXPECT_FALSE(telemetry_info_output->battery_result);
  EXPECT_FALSE(telemetry_info_output->block_device_result);
  EXPECT_FALSE(telemetry_info_output->vpd_result);
}

TEST(ProbeServiceConvertors, TelemetryInfoPtrNull) {
  EXPECT_TRUE(Convert(cros_healthd::mojom::TelemetryInfoPtr()).is_null());
}

}  // namespace probe_service_converters
}  // namespace chromeos
