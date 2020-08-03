// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_DIAGNOSTICS_SERVICE_CONVERTERS_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_DIAGNOSTICS_SERVICE_CONVERTERS_H_

#if defined(OFFICIAL_BUILD)
#error Diagnostics service should only be included in unofficial builds.
#endif

#include <vector>

#include "chromeos/components/telemetry_extension_ui/mojom/diagnostics_service.mojom-forward.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom-forward.h"

namespace chromeos {
namespace diagnostics_service_converters {

// This file contains helper functions used by DiagnosticsService to convert its
// types to/from cros_healthd DiagnosticsService types.

std::vector<health::mojom::DiagnosticRoutineEnum> Convert(
    const std::vector<cros_healthd::mojom::DiagnosticRoutineEnum>& input);

}  // namespace diagnostics_service_converters
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_DIAGNOSTICS_SERVICE_CONVERTERS_H_
