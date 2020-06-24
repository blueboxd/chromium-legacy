// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/probe_service_converters.h"

#include <utility>

#include "base/notreached.h"
#include "chromeos/components/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"

namespace chromeos {
namespace probe_service_converters {

cros_healthd::mojom::ProbeCategoryEnum Convert(
    health::mojom::ProbeCategoryEnum input) {
  switch (input) {
    case health::mojom::ProbeCategoryEnum::kBattery:
      return cros_healthd::mojom::ProbeCategoryEnum::kBattery;
  }
  NOTREACHED();
}

std::vector<cros_healthd::mojom::ProbeCategoryEnum> Convert(
    const std::vector<health::mojom::ProbeCategoryEnum>& input) {
  std::vector<cros_healthd::mojom::ProbeCategoryEnum> output;
  for (auto category : input) {
    output.push_back(Convert(category));
  }
  return output;
}

health::mojom::ErrorType Convert(cros_healthd::mojom::ErrorType input) {
  switch (input) {
    case cros_healthd::mojom::ErrorType::kFileReadError:
      return health::mojom::ErrorType::kFileReadError;
    case cros_healthd::mojom::ErrorType::kParseError:
      return health::mojom::ErrorType::kParseError;
    case cros_healthd::mojom::ErrorType::kSystemUtilityError:
      return health::mojom::ErrorType::kSystemUtilityError;
  }
  NOTREACHED();
}

health::mojom::ProbeErrorPtr Convert(cros_healthd::mojom::ProbeErrorPtr input) {
  if (!input) {
    return nullptr;
  }
  return health::mojom::ProbeError::New(Convert(input->type),
                                        std::move(input->msg));
}

health::mojom::DoubleValuePtr Convert(double input) {
  return health::mojom::DoubleValue::New(input);
}

health::mojom::Int64ValuePtr Convert(int64_t input) {
  return health::mojom::Int64Value::New(input);
}

health::mojom::UInt64ValuePtr Convert(
    cros_healthd::mojom::UInt64ValuePtr input) {
  if (!input) {
    return nullptr;
  }
  return health::mojom::UInt64Value::New(input->value);
}

health::mojom::BatteryInfoPtr Convert(
    cros_healthd::mojom::BatteryInfoPtr input) {
  if (!input) {
    return nullptr;
  }

  auto output = health::mojom::BatteryInfo::New();

  output->cycle_count = Convert(input->cycle_count);
  output->voltage_now = Convert(input->voltage_now);
  output->vendor = std::move(input->vendor);
  output->serial_number = std::move(input->serial_number);
  output->charge_full_design = Convert(input->charge_full_design);
  output->charge_full = Convert(input->charge_full);
  output->voltage_min_design = Convert(input->voltage_min_design);
  output->model_name = std::move(input->model_name);
  output->charge_now = Convert(input->charge_now);
  output->current_now = Convert(input->current_now);
  output->technology = std::move(input->technology);
  output->status = std::move(input->status);

  if (input->manufacture_date.has_value()) {
    output->manufacture_date = std::move(input->manufacture_date.value());
  }

  output->temperature = Convert(std::move(input->temperature));

  return output;
}

health::mojom::BatteryResultPtr Convert(
    cros_healthd::mojom::BatteryResultPtr input) {
  if (!input) {
    return nullptr;
  }

  auto output = health::mojom::BatteryResult::New();

  if (input->is_error()) {
    output->set_error(Convert(std::move(input->get_error())));
  } else if (input->is_battery_info()) {
    output->set_battery_info(Convert(std::move(input->get_battery_info())));
  }

  return output;
}

health::mojom::TelemetryInfoPtr Convert(
    cros_healthd::mojom::TelemetryInfoPtr input) {
  if (!input) {
    return nullptr;
  }

  return health::mojom::TelemetryInfo::New(
      Convert(std::move(input->battery_result)));
}

}  // namespace probe_service_converters
}  // namespace chromeos
