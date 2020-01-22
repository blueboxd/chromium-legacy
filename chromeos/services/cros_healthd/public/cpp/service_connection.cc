// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace cros_healthd {

namespace {

// Production implementation of ServiceConnection.
class ServiceConnectionImpl : public ServiceConnection {
 public:
  ServiceConnectionImpl();

 protected:
  ~ServiceConnectionImpl() override = default;

 private:
  // ServiceConnection overrides:
  void GetAvailableRoutines(
      mojom::CrosHealthdDiagnosticsService::GetAvailableRoutinesCallback
          callback) override;
  void GetRoutineUpdate(
      int32_t id,
      mojom::DiagnosticRoutineCommandEnum command,
      bool include_output,
      mojom::CrosHealthdDiagnosticsService::GetRoutineUpdateCallback callback)
      override;
  void RunUrandomRoutine(
      uint32_t length_seconds,
      mojom::CrosHealthdDiagnosticsService::RunUrandomRoutineCallback callback)
      override;
  void RunBatteryCapacityRoutine(
      uint32_t low_mah,
      uint32_t high_mah,
      mojom::CrosHealthdDiagnosticsService::RunBatteryCapacityRoutineCallback
          callback) override;
  void RunBatteryHealthRoutine(
      uint32_t maximum_cycle_count,
      uint32_t percent_battery_wear_allowed,
      mojom::CrosHealthdDiagnosticsService::RunBatteryHealthRoutineCallback
          callback) override;
  void RunSmartctlCheckRoutine(
      mojom::CrosHealthdDiagnosticsService::RunSmartctlCheckRoutineCallback
          callback) override;
  void ProbeTelemetryInfo(
      const std::vector<mojom::ProbeCategoryEnum>& categories_to_test,
      mojom::CrosHealthdProbeService::ProbeTelemetryInfoCallback callback)
      override;
  void GetDiagnosticsService(
      mojom::CrosHealthdDiagnosticsServiceRequest service) override;

  // Binds the factory interface |cros_healthd_service_factory_| to an
  // implementation in the cros_healthd daemon, if it is not already bound. The
  // binding is accomplished via D-Bus bootstrap.
  void BindCrosHealthdServiceFactoryIfNeeded();

  // Uses |cros_healthd_service_factory_| to bind the diagnostics service remote
  // to an implementation in the cros_healethd daemon, if it is not already
  // bound.
  void BindCrosHealthdDiagnosticsServiceIfNeeded();

  // Uses |cros_healthd_service_factory_| to bind the probe service remote to an
  // implementation in the cros_healethd daemon, if it is not already bound.
  void BindCrosHealthdProbeServiceIfNeeded();

  // Mojo disconnect handler. Resets |cros_healthd_service_|, which will be
  // reconnected upon next use.
  void OnDisconnect();

  // Response callback for BootstrapMojoConnection.
  void OnBootstrapMojoConnectionResponse(bool success);

  mojo::Remote<mojom::CrosHealthdServiceFactory> cros_healthd_service_factory_;
  mojo::Remote<mojom::CrosHealthdProbeService> cros_healthd_probe_service_;
  mojo::Remote<mojom::CrosHealthdDiagnosticsService>
      cros_healthd_diagnostics_service_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ServiceConnectionImpl);
};

void ServiceConnectionImpl::GetAvailableRoutines(
    mojom::CrosHealthdDiagnosticsService::GetAvailableRoutinesCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->GetAvailableRoutines(std::move(callback));
}

void ServiceConnectionImpl::GetRoutineUpdate(
    int32_t id,
    mojom::DiagnosticRoutineCommandEnum command,
    bool include_output,
    mojom::CrosHealthdDiagnosticsService::GetRoutineUpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->GetRoutineUpdate(
      id, command, include_output, std::move(callback));
}

void ServiceConnectionImpl::RunUrandomRoutine(
    uint32_t length_seconds,
    mojom::CrosHealthdDiagnosticsService::RunUrandomRoutineCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunUrandomRoutine(length_seconds,
                                                       std::move(callback));
}

void ServiceConnectionImpl::RunBatteryCapacityRoutine(
    uint32_t low_mah,
    uint32_t high_mah,
    mojom::CrosHealthdDiagnosticsService::RunBatteryCapacityRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunBatteryCapacityRoutine(
      low_mah, high_mah, std::move(callback));
}

void ServiceConnectionImpl::RunBatteryHealthRoutine(
    uint32_t maximum_cycle_count,
    uint32_t percent_battery_wear_allowed,
    mojom::CrosHealthdDiagnosticsService::RunBatteryHealthRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunBatteryHealthRoutine(
      maximum_cycle_count, percent_battery_wear_allowed, std::move(callback));
}

void ServiceConnectionImpl::RunSmartctlCheckRoutine(
    mojom::CrosHealthdDiagnosticsService::RunSmartctlCheckRoutineCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdDiagnosticsServiceIfNeeded();
  cros_healthd_diagnostics_service_->RunSmartctlCheckRoutine(
      std::move(callback));
}

void ServiceConnectionImpl::ProbeTelemetryInfo(
    const std::vector<mojom::ProbeCategoryEnum>& categories_to_test,
    mojom::CrosHealthdProbeService::ProbeTelemetryInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdProbeServiceIfNeeded();
  cros_healthd_probe_service_->ProbeTelemetryInfo(categories_to_test,
                                                  std::move(callback));
}

void ServiceConnectionImpl::GetDiagnosticsService(
    mojom::CrosHealthdDiagnosticsServiceRequest service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindCrosHealthdServiceFactoryIfNeeded();
  cros_healthd_service_factory_->GetDiagnosticsService(std::move(service));
}

void ServiceConnectionImpl::BindCrosHealthdServiceFactoryIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_service_factory_.is_bound())
    return;

  cros_healthd_service_factory_ =
      CrosHealthdClient::Get()->BootstrapMojoConnection(base::BindOnce(
          &ServiceConnectionImpl::OnBootstrapMojoConnectionResponse,
          base::Unretained(this)));
  cros_healthd_service_factory_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionImpl::OnDisconnect, base::Unretained(this)));
}

void ServiceConnectionImpl::BindCrosHealthdDiagnosticsServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_diagnostics_service_.is_bound())
    return;

  BindCrosHealthdServiceFactoryIfNeeded();
  cros_healthd_service_factory_->GetDiagnosticsService(
      cros_healthd_diagnostics_service_.BindNewPipeAndPassReceiver());
  cros_healthd_diagnostics_service_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionImpl::OnDisconnect, base::Unretained(this)));
}

void ServiceConnectionImpl::BindCrosHealthdProbeServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cros_healthd_probe_service_.is_bound())
    return;

  BindCrosHealthdServiceFactoryIfNeeded();
  cros_healthd_service_factory_->GetProbeService(
      cros_healthd_probe_service_.BindNewPipeAndPassReceiver());
  cros_healthd_probe_service_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionImpl::OnDisconnect, base::Unretained(this)));
}

ServiceConnectionImpl::ServiceConnectionImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void ServiceConnectionImpl::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Connection errors are not expected, so log a warning.
  DLOG(WARNING) << "cros_healthd Mojo connection closed.";
  cros_healthd_service_factory_.reset();
  cros_healthd_probe_service_.reset();
  cros_healthd_diagnostics_service_.reset();
}

void ServiceConnectionImpl::OnBootstrapMojoConnectionResponse(
    const bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    DLOG(WARNING) << "BootstrapMojoConnection D-Bus call failed.";
    cros_healthd_service_factory_.reset();
  }
}

}  // namespace

ServiceConnection* ServiceConnection::GetInstance() {
  static base::NoDestructor<ServiceConnectionImpl> service_connection;
  return service_connection.get();
}

}  // namespace cros_healthd
}  // namespace chromeos
