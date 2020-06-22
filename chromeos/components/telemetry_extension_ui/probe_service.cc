// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/probe_service.h"

#include <utility>

#include "base/bind.h"
#include "base/notreached.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"

namespace chromeos {

namespace {

std::vector<cros_healthd::mojom::ProbeCategoryEnum> ConvertCategories(
    const std::vector<health::mojom::ProbeCategoryEnum>& original_categories) {
  std::vector<cros_healthd::mojom::ProbeCategoryEnum> categories;
  for (auto category : original_categories) {
    switch (category) {
      case health::mojom::ProbeCategoryEnum::kBattery:
        categories.push_back(cros_healthd::mojom::ProbeCategoryEnum::kBattery);
        break;
    }
  }
  return categories;
}

health::mojom::ErrorType ConvertErrorType(cros_healthd::mojom::ErrorType type) {
  switch (type) {
    case cros_healthd::mojom::ErrorType::kFileReadError:
      return health::mojom::ErrorType::kFileReadError;
    case cros_healthd::mojom::ErrorType::kParseError:
      return health::mojom::ErrorType::kParseError;
    case cros_healthd::mojom::ErrorType::kSystemUtilityError:
      return health::mojom::ErrorType::kSystemUtilityError;
  }
  NOTREACHED();
}

health::mojom::ProbeErrorPtr ConvertProbeError(
    cros_healthd::mojom::ProbeErrorPtr ptr) {
  if (!ptr) {
    return nullptr;
  }
  return health::mojom::ProbeError::New(ConvertErrorType(ptr->type),
                                        std::move(ptr->msg));
}

health::mojom::UInt64ValuePtr ConvertUInt64Value(
    cros_healthd::mojom::UInt64ValuePtr ptr) {
  if (!ptr) {
    return nullptr;
  }
  return health::mojom::UInt64Value::New(ptr->value);
}

health::mojom::BatteryInfoPtr ConvertBatteryInfo(
    cros_healthd::mojom::BatteryInfoPtr ptr) {
  if (!ptr) {
    return nullptr;
  }
  auto info = health::mojom::BatteryInfo::New();

  info->cycle_count = ptr->cycle_count;
  info->voltage_now = ptr->voltage_now;
  info->vendor = std::move(ptr->vendor);
  info->serial_number = std::move(ptr->serial_number);
  info->charge_full_design = ptr->charge_full_design;
  info->charge_full = ptr->charge_full;
  info->voltage_min_design = ptr->voltage_min_design;
  info->model_name = std::move(ptr->model_name);
  info->charge_now = ptr->charge_now;
  info->current_now = ptr->current_now;
  info->technology = std::move(ptr->technology);
  info->status = std::move(ptr->status);

  if (ptr->manufacture_date.has_value()) {
    info->manufacture_date = std::move(ptr->manufacture_date.value());
  }

  info->temperature = ConvertUInt64Value(std::move(ptr->temperature));

  return info;
}

health::mojom::BatteryResultPtr ConvertBatteryResult(
    cros_healthd::mojom::BatteryResultPtr ptr) {
  if (!ptr) {
    return nullptr;
  }

  auto battery_result = health::mojom::BatteryResult::New();

  if (ptr->is_error()) {
    battery_result->set_error(ConvertProbeError(std::move(ptr->get_error())));
  } else if (ptr->is_battery_info()) {
    battery_result->set_battery_info(
        ConvertBatteryInfo(std::move(ptr->get_battery_info())));
  }

  return battery_result;
}

}  // namespace

ProbeService::ProbeService(
    mojo::PendingReceiver<health::mojom::ProbeService> receiver)
    : receiver_(this, std::move(receiver)) {}

ProbeService::~ProbeService() = default;

void ProbeService::ProbeTelemetryInfo(
    const std::vector<health::mojom::ProbeCategoryEnum>& categories,
    ProbeTelemetryInfoCallback callback) {
  GetService()->ProbeTelemetryInfo(
      ConvertCategories(categories),
      base::BindOnce(
          [](health::mojom::ProbeService::ProbeTelemetryInfoCallback callback,
             cros_healthd::mojom::TelemetryInfoPtr ptr) {
            auto info = health::mojom::TelemetryInfo::New();
            info->battery_result =
                ConvertBatteryResult(std::move(ptr->battery_result));
            std::move(callback).Run(std::move(info));
          },
          std::move(callback)));
}

cros_healthd::mojom::CrosHealthdProbeService* ProbeService::GetService() {
  if (!service_ || !service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->GetProbeService(
        service_.BindNewPipeAndPassReceiver());
    service_.set_disconnect_handler(
        base::BindOnce(&ProbeService::OnDisconnect, base::Unretained(this)));
  }
  return service_.get();
}

void ProbeService::OnDisconnect() {
  service_.reset();
}

}  // namespace chromeos
