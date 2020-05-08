// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cros_healthd/fake_cros_healthd_service.h"

#include <utility>

namespace chromeos {
namespace cros_healthd {

FakeCrosHealthdService::FakeCrosHealthdService() = default;
FakeCrosHealthdService::~FakeCrosHealthdService() = default;

void FakeCrosHealthdService::GetProbeService(
    mojom::CrosHealthdProbeServiceRequest service) {
  probe_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthdService::GetDiagnosticsService(
    mojom::CrosHealthdDiagnosticsServiceRequest service) {
  diagnostics_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthdService::GetEventService(
    mojom::CrosHealthdEventServiceRequest service) {
  event_receiver_set_.Add(this, std::move(service));
}

void FakeCrosHealthdService::GetAvailableRoutines(
    GetAvailableRoutinesCallback callback) {
  std::move(callback).Run(available_routines_);
}

void FakeCrosHealthdService::GetRoutineUpdate(
    int32_t id,
    mojom::DiagnosticRoutineCommandEnum command,
    bool include_output,
    GetRoutineUpdateCallback callback) {
  std::move(callback).Run(mojom::RoutineUpdate::New(
      routine_update_response_->progress_percent,
      std::move(routine_update_response_->output),
      std::move(routine_update_response_->routine_update_union)));
}

void FakeCrosHealthdService::RunUrandomRoutine(
    uint32_t length_seconds,
    RunUrandomRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunBatteryCapacityRoutine(
    uint32_t low_mah,
    uint32_t high_mah,
    RunBatteryCapacityRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunBatteryHealthRoutine(
    uint32_t maximum_cycle_count,
    uint32_t percent_battery_wear_allowed,
    RunBatteryHealthRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunSmartctlCheckRoutine(
    RunSmartctlCheckRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunAcPowerRoutine(
    mojom::AcPowerStatusEnum expected_status,
    const base::Optional<std::string>& expected_power_type,
    RunAcPowerRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunCpuCacheRoutine(
    uint32_t length_seconds,
    RunCpuCacheRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunCpuStressRoutine(
    uint32_t length_seconds,
    RunCpuStressRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunFloatingPointAccuracyRoutine(
    uint32_t length_seconds,
    RunFloatingPointAccuracyRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunNvmeWearLevelRoutine(
    uint32_t wear_level_threshold,
    RunNvmeWearLevelRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunNvmeSelfTestRoutine(
    mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
    RunNvmeSelfTestRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunDiskReadRoutine(
    mojom::DiskReadRoutineTypeEnum type,
    uint32_t length_seconds,
    uint32_t file_size_mb,
    RunDiskReadRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunPrimeSearchRoutine(
    uint32_t length_seconds,
    uint64_t max_num,
    RunPrimeSearchRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::RunBatteryDischargeRoutine(
    uint32_t length_seconds,
    uint32_t maximum_discharge_percent_allowed,
    RunBatteryDischargeRoutineCallback callback) {
  std::move(callback).Run(run_routine_response_.Clone());
}

void FakeCrosHealthdService::AddBluetoothObserver(
    mojom::CrosHealthdBluetoothObserverPtr observer) {
  bluetooth_observers_.Add(observer.PassInterface());
}

void FakeCrosHealthdService::AddLidObserver(
    mojom::CrosHealthdLidObserverPtr observer) {
  lid_observers_.Add(observer.PassInterface());
}

void FakeCrosHealthdService::AddPowerObserver(
    mojom::CrosHealthdPowerObserverPtr observer) {
  power_observers_.Add(observer.PassInterface());
}

void FakeCrosHealthdService::ProbeTelemetryInfo(
    const std::vector<mojom::ProbeCategoryEnum>& categories,
    ProbeTelemetryInfoCallback callback) {
  std::move(callback).Run(telemetry_response_info_.Clone());
}

void FakeCrosHealthdService::SetAvailableRoutinesForTesting(
    const std::vector<mojom::DiagnosticRoutineEnum>& available_routines) {
  available_routines_ = available_routines;
}

void FakeCrosHealthdService::SetRunRoutineResponseForTesting(
    mojom::RunRoutineResponsePtr& response) {
  run_routine_response_.Swap(&response);
}

void FakeCrosHealthdService::SetGetRoutineUpdateResponseForTesting(
    mojom::RoutineUpdatePtr& response) {
  routine_update_response_.Swap(&response);
}

void FakeCrosHealthdService::SetProbeTelemetryInfoResponseForTesting(
    mojom::TelemetryInfoPtr& response_info) {
  telemetry_response_info_.Swap(&response_info);
}

void FakeCrosHealthdService::EmitAcInsertedEventForTesting() {
  for (auto& observer : power_observers_)
    observer->OnAcInserted();
}

void FakeCrosHealthdService::EmitAdapterAddedEventForTesting() {
  for (auto& observer : bluetooth_observers_)
    observer->OnAdapterAdded();
}

void FakeCrosHealthdService::EmitLidClosedEventForTesting() {
  for (auto& observer : lid_observers_)
    observer->OnLidClosed();
}

}  // namespace cros_healthd
}  // namespace chromeos
