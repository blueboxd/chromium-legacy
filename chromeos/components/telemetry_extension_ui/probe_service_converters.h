// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_PROBE_SERVICE_CONVERTERS_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_PROBE_SERVICE_CONVERTERS_H_

#if defined(OFFICIAL_BUILD)
#error Probe service convertors should only be included in unofficial builds.
#endif

#include <vector>

#include "chromeos/components/telemetry_extension_ui/mojom/probe_service.mojom-forward.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-forward.h"

namespace chromeos {
namespace probe_service_converters {

// This file contains helper functions used by ProbeService to convert its
// types to/from cros_healthd ProbeService types.

cros_healthd::mojom::ProbeCategoryEnum Convert(
    health::mojom::ProbeCategoryEnum input);

std::vector<cros_healthd::mojom::ProbeCategoryEnum> Convert(
    const std::vector<health::mojom::ProbeCategoryEnum>& input);

health::mojom::ErrorType Convert(cros_healthd::mojom::ErrorType type);

health::mojom::ProbeErrorPtr Convert(cros_healthd::mojom::ProbeErrorPtr input);

health::mojom::DoubleValuePtr Convert(double input);

health::mojom::Int64ValuePtr Convert(int64_t input);

health::mojom::UInt64ValuePtr Convert(
    cros_healthd::mojom::UInt64ValuePtr input);

health::mojom::BatteryInfoPtr Convert(
    cros_healthd::mojom::BatteryInfoPtr input);

health::mojom::BatteryResultPtr Convert(
    cros_healthd::mojom::BatteryResultPtr input);

health::mojom::TelemetryInfoPtr Convert(
    cros_healthd::mojom::TelemetryInfoPtr input);

}  // namespace probe_service_converters
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_PROBE_SERVICE_CONVERTERS_H_
