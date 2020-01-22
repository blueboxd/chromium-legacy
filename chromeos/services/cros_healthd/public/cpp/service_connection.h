// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CROS_HEALTHD_PUBLIC_CPP_SERVICE_CONNECTION_H_
#define CHROMEOS_SERVICES_CROS_HEALTHD_PUBLIC_CPP_SERVICE_CONNECTION_H_

#include <cstdint>

#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"

namespace chromeos {
namespace cros_healthd {

// Encapsulates a connection to the Chrome OS cros_healthd daemon via its Mojo
// interface.
// Sequencing: Must be used on a single sequence (may be created on another).
class ServiceConnection {
 public:
  static ServiceConnection* GetInstance();

  // Retrieve a list of available diagnostic routines. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void GetAvailableRoutines(
      mojom::CrosHealthdDiagnosticsService::GetAvailableRoutinesCallback
          callback) = 0;

  // Send a command to an existing routine. Also returns status information
  // for the routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void GetRoutineUpdate(
      int32_t id,
      mojom::DiagnosticRoutineCommandEnum command,
      bool include_output,
      mojom::CrosHealthdDiagnosticsService::GetRoutineUpdateCallback
          callback) = 0;

  // Requests that cros_healthd runs the urandom routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunUrandomRoutine(
      uint32_t length_seconds,
      mojom::CrosHealthdDiagnosticsService::RunUrandomRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the battery capacity routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunBatteryCapacityRoutine(
      uint32_t low_mah,
      uint32_t high_mah,
      mojom::CrosHealthdDiagnosticsService::RunBatteryCapacityRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the battery health routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunBatteryHealthRoutine(
      uint32_t maximum_cycle_count,
      uint32_t percent_battery_wear_allowed,
      mojom::CrosHealthdDiagnosticsService::RunBatteryHealthRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the smartcl check routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunSmartctlCheckRoutine(
      mojom::CrosHealthdDiagnosticsService::RunSmartctlCheckRoutineCallback
          callback) = 0;

  // Gather pieces of information about the platform. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void ProbeTelemetryInfo(
      const std::vector<mojom::ProbeCategoryEnum>& categories_to_test,
      mojom::CrosHealthdProbeService::ProbeTelemetryInfoCallback callback) = 0;

  // Binds |service| to an implementation of CrosHealthdDiagnosticsService. In
  // production, this implementation is provided by cros_healthd. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void GetDiagnosticsService(
      mojom::CrosHealthdDiagnosticsServiceRequest service) = 0;

 protected:
  ServiceConnection() = default;
  virtual ~ServiceConnection() = default;
};

}  // namespace cros_healthd
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CROS_HEALTHD_PUBLIC_CPP_SERVICE_CONNECTION_H_
