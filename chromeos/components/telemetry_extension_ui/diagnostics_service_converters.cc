// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/diagnostics_service_converters.h"

#include "base/notreached.h"
#include "base/optional.h"
#include "chromeos/components/telemetry_extension_ui/mojom/diagnostics_service.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"

namespace chromeos {
namespace diagnostics_service_converters {

namespace {

base::Optional<health::mojom::DiagnosticRoutineEnum> Convert(
    cros_healthd::mojom::DiagnosticRoutineEnum input) {
  switch (input) {
    case cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity:
      return health::mojom::DiagnosticRoutineEnum::kBatteryCapacity;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth:
      return health::mojom::DiagnosticRoutineEnum::kBatteryHealth;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kUrandom:
      return health::mojom::DiagnosticRoutineEnum::kUrandom;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kSmartctlCheck:
      return health::mojom::DiagnosticRoutineEnum::kSmartctlCheck;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower:
      return health::mojom::DiagnosticRoutineEnum::kAcPower;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache:
      return health::mojom::DiagnosticRoutineEnum::kCpuCache;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress:
      return health::mojom::DiagnosticRoutineEnum::kCpuStress;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy:
      return health::mojom::DiagnosticRoutineEnum::kFloatingPointAccuracy;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeWearLevel:
      return health::mojom::DiagnosticRoutineEnum::kNvmeWearLevel;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeSelfTest:
      return health::mojom::DiagnosticRoutineEnum::kNvmeSelfTest;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead:
      return health::mojom::DiagnosticRoutineEnum::kDiskRead;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch:
      return health::mojom::DiagnosticRoutineEnum::kPrimeSearch;
    case cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge:
      return health::mojom::DiagnosticRoutineEnum::kBatteryDischarge;
  }
  NOTREACHED();
  return base::nullopt;
}

}  // namespace

std::vector<health::mojom::DiagnosticRoutineEnum> Convert(
    const std::vector<cros_healthd::mojom::DiagnosticRoutineEnum>& input) {
  std::vector<health::mojom::DiagnosticRoutineEnum> output;
  for (const auto element : input) {
    base::Optional<health::mojom::DiagnosticRoutineEnum> converted =
        Convert(element);
    if (converted.has_value()) {
      output.push_back(converted.value());
    }
  }
  return output;
}

}  // namespace diagnostics_service_converters
}  // namespace chromeos
