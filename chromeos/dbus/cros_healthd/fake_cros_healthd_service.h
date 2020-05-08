// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_SERVICE_H_
#define CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_SERVICE_H_

#include <vector>

#include "base/macros.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {
namespace cros_healthd {

// This class serves as a fake for all four of cros_healthd's mojo interfaces.
// The factory methods bind to receivers held within FakeCrosHealtdService, and
// all requests on each of the interfaces are fulfilled by
// FakeCrosHealthdService.
class FakeCrosHealthdService final
    : public mojom::CrosHealthdServiceFactory,
      public mojom::CrosHealthdDiagnosticsService,
      public mojom::CrosHealthdEventService,
      public mojom::CrosHealthdProbeService {
 public:
  FakeCrosHealthdService();
  ~FakeCrosHealthdService() override;

  // CrosHealthdServiceFactory overrides:
  void GetProbeService(mojom::CrosHealthdProbeServiceRequest service) override;
  void GetDiagnosticsService(
      mojom::CrosHealthdDiagnosticsServiceRequest service) override;
  void GetEventService(mojom::CrosHealthdEventServiceRequest service) override;

  // CrosHealthdDiagnosticsService overrides:
  void GetAvailableRoutines(GetAvailableRoutinesCallback callback) override;
  void GetRoutineUpdate(int32_t id,
                        mojom::DiagnosticRoutineCommandEnum command,
                        bool include_output,
                        GetRoutineUpdateCallback callback) override;
  void RunUrandomRoutine(uint32_t length_seconds,
                         RunUrandomRoutineCallback callback) override;
  void RunBatteryCapacityRoutine(
      uint32_t low_mah,
      uint32_t high_mah,
      RunBatteryCapacityRoutineCallback callback) override;
  void RunBatteryHealthRoutine(
      uint32_t maximum_cycle_count,
      uint32_t percent_battery_wear_allowed,
      RunBatteryHealthRoutineCallback callback) override;
  void RunSmartctlCheckRoutine(
      RunSmartctlCheckRoutineCallback callback) override;
  void RunAcPowerRoutine(mojom::AcPowerStatusEnum expected_status,
                         const base::Optional<std::string>& expected_power_type,
                         RunAcPowerRoutineCallback callback) override;
  void RunCpuCacheRoutine(uint32_t length_seconds,
                          RunCpuCacheRoutineCallback callback) override;
  void RunCpuStressRoutine(uint32_t length_seconds,
                           RunCpuStressRoutineCallback callback) override;
  void RunFloatingPointAccuracyRoutine(
      uint32_t length_seconds,
      RunFloatingPointAccuracyRoutineCallback callback) override;
  void RunNvmeWearLevelRoutine(
      uint32_t wear_level_threshold,
      RunNvmeWearLevelRoutineCallback callback) override;
  void RunNvmeSelfTestRoutine(mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
                              RunNvmeSelfTestRoutineCallback callback) override;
  void RunDiskReadRoutine(mojom::DiskReadRoutineTypeEnum type,
                          uint32_t length_seconds,
                          uint32_t file_size_mb,
                          RunDiskReadRoutineCallback callback) override;
  void RunPrimeSearchRoutine(uint32_t length_seconds,
                             uint64_t max_num,
                             RunPrimeSearchRoutineCallback callback) override;
  void RunBatteryDischargeRoutine(
      uint32_t length_seconds,
      uint32_t maximum_discharge_percent_allowed,
      RunBatteryDischargeRoutineCallback callback) override;

  // CrosHealthdEventService overrides:
  void AddBluetoothObserver(
      mojom::CrosHealthdBluetoothObserverPtr observer) override;
  void AddLidObserver(mojom::CrosHealthdLidObserverPtr observer) override;
  void AddPowerObserver(mojom::CrosHealthdPowerObserverPtr observer) override;

  // CrosHealthdProbeService overrides:
  void ProbeTelemetryInfo(
      const std::vector<mojom::ProbeCategoryEnum>& categories,
      ProbeTelemetryInfoCallback callback) override;

  // Set the list of routines that will be used in the response to any
  // GetAvailableRoutines IPCs received.
  void SetAvailableRoutinesForTesting(
      const std::vector<mojom::DiagnosticRoutineEnum>& available_routines);

  // Set the RunRoutine response that will be used in the response to any
  // RunSomeRoutine IPCs received.
  void SetRunRoutineResponseForTesting(mojom::RunRoutineResponsePtr& response);

  // Set the GetRoutineUpdate response that will be used in the response to any
  // GetRoutineUpdate IPCs received.
  void SetGetRoutineUpdateResponseForTesting(mojom::RoutineUpdatePtr& response);

  // Set the TelemetryInfoPtr that will be used in the response to any
  // ProbeTelemetryInfo IPCs received.
  void SetProbeTelemetryInfoResponseForTesting(
      mojom::TelemetryInfoPtr& response_info);

  // Calls the power event OnAcInserted for all registered power observers.
  void EmitAcInsertedEventForTesting();

  // Calls the Bluetooth event OnAdapterAdded for all registered Bluetooth
  // observers.
  void EmitAdapterAddedEventForTesting();

  // Calls the lid event OnLidClosed for all registered lid observers.
  void EmitLidClosedEventForTesting();

 private:
  // Used as the response to any GetAvailableRoutines IPCs received.
  std::vector<mojom::DiagnosticRoutineEnum> available_routines_;
  // Used as the response to any RunSomeRoutine IPCs received.
  mojom::RunRoutineResponsePtr run_routine_response_{
      mojom::RunRoutineResponse::New()};
  // Used as the response to any GetRoutineUpdate IPCs received.
  mojom::RoutineUpdatePtr routine_update_response_{mojom::RoutineUpdate::New()};
  // Used as the response to any ProbeTelemetryInfo IPCs received.
  mojom::TelemetryInfoPtr telemetry_response_info_{mojom::TelemetryInfo::New()};

  // Allows the remote end to call the probe, diagnostics and event service
  // methods.
  mojo::ReceiverSet<mojom::CrosHealthdProbeService> probe_receiver_set_;
  mojo::ReceiverSet<mojom::CrosHealthdDiagnosticsService>
      diagnostics_receiver_set_;
  mojo::ReceiverSet<mojom::CrosHealthdEventService> event_receiver_set_;

  // Collection of registered Bluetooth observers.
  mojo::RemoteSet<mojom::CrosHealthdBluetoothObserver> bluetooth_observers_;
  // Collection of registered lid observers.
  mojo::RemoteSet<mojom::CrosHealthdLidObserver> lid_observers_;
  // Collection of registered power observers.
  mojo::RemoteSet<mojom::CrosHealthdPowerObserver> power_observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeCrosHealthdService);
};

}  // namespace cros_healthd
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_SERVICE_H_
