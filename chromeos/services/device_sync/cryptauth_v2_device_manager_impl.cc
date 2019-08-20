// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_v2_device_manager_impl.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/device_sync/cryptauth_client.h"
#include "chromeos/services/device_sync/cryptauth_key_registry.h"

namespace chromeos {

namespace device_sync {

// static
CryptAuthV2DeviceManagerImpl::Factory*
    CryptAuthV2DeviceManagerImpl::Factory::test_factory_ = nullptr;

// static
CryptAuthV2DeviceManagerImpl::Factory*
CryptAuthV2DeviceManagerImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<CryptAuthV2DeviceManagerImpl::Factory> factory;
  return factory.get();
}

// static
void CryptAuthV2DeviceManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthV2DeviceManagerImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthV2DeviceManager>
CryptAuthV2DeviceManagerImpl::Factory::BuildInstance(
    CryptAuthDeviceRegistry* device_registry,
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    CryptAuthGCMManager* gcm_manager,
    CryptAuthScheduler* scheduler) {
  return base::WrapUnique(new CryptAuthV2DeviceManagerImpl(
      device_registry, key_registry, client_factory, gcm_manager, scheduler));
}

CryptAuthV2DeviceManagerImpl::CryptAuthV2DeviceManagerImpl(
    CryptAuthDeviceRegistry* device_registry,
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    CryptAuthGCMManager* gcm_manager,
    CryptAuthScheduler* scheduler)
    : device_registry_(device_registry),
      gcm_manager_(gcm_manager),
      scheduler_(scheduler) {
  gcm_manager_->AddObserver(this);
}

CryptAuthV2DeviceManagerImpl::~CryptAuthV2DeviceManagerImpl() {
  gcm_manager_->RemoveObserver(this);
}

void CryptAuthV2DeviceManagerImpl::Start() {
  scheduler_->StartDeviceSyncScheduling(
      scheduler_weak_ptr_factory_.GetWeakPtr());
}

const CryptAuthDeviceRegistry::InstanceIdToDeviceMap&
CryptAuthV2DeviceManagerImpl::GetSyncedDevices() const {
  return device_registry_->instance_id_to_device_map();
}

void CryptAuthV2DeviceManagerImpl::ForceDeviceSyncNow(
    const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
    const base::Optional<std::string>& session_id) {
  scheduler_->RequestDeviceSync(invocation_reason, session_id);
}

base::Optional<base::Time> CryptAuthV2DeviceManagerImpl::GetLastDeviceSyncTime()
    const {
  return scheduler_->GetLastSuccessfulDeviceSyncTime();
}

base::Optional<base::TimeDelta>
CryptAuthV2DeviceManagerImpl::GetTimeToNextAttempt() const {
  return scheduler_->GetTimeToNextDeviceSyncRequest();
}

bool CryptAuthV2DeviceManagerImpl::IsDeviceSyncInProgress() const {
  return scheduler_->IsWaitingForDeviceSyncResult();
}

bool CryptAuthV2DeviceManagerImpl::IsRecoveringFromFailure() const {
  return scheduler_->GetNumConsecutiveDeviceSyncFailures() > 0;
}

void CryptAuthV2DeviceManagerImpl::OnDeviceSyncRequested(
    const cryptauthv2::ClientMetadata& client_metadata) {
  NotifyDeviceSyncStarted(client_metadata);

  current_client_metadata_ = client_metadata;

  // TODO(nohle): Log invocation reason metric.

  // TODO(nohle): Start DeviceSync flow here.
}

void CryptAuthV2DeviceManagerImpl::OnResyncMessage(
    const base::Optional<std::string>& session_id,
    const base::Optional<CryptAuthFeatureType>& feature_type) {
  PA_LOG(VERBOSE) << "Received GCM message to re-sync devices (session ID: "
                  << session_id.value_or("[No session ID]") << ")";

  ForceDeviceSyncNow(cryptauthv2::ClientMetadata::SERVER_INITIATED, session_id);
}

void CryptAuthV2DeviceManagerImpl::OnDeviceSyncFinished(
    const CryptAuthDeviceSyncResult& device_sync_result) {
  // TODO(nohle): Invalidate callback weak pointers, if applicable, and reset
  // the DeviceSyncer instance.

  if (device_sync_result.IsSuccess()) {
    PA_LOG(INFO) << "DeviceSync attempt with invocation reason "
                 << current_client_metadata_->invocation_reason()
                 << " succeeded with result code "
                 << device_sync_result.result_code();

    // TODO(nohle): Log if the devices changed.
  } else {
    PA_LOG(WARNING) << "DeviceSync attempt with invocation reason "
                    << current_client_metadata_->invocation_reason()
                    << " failed with result code "
                    << device_sync_result.result_code();
  }

  current_client_metadata_.reset();

  // TODO(nohle): Log DeviceSync result metrics: success, result code, and if
  // devices changed.

  scheduler_->HandleDeviceSyncResult(device_sync_result);

  base::Optional<base::TimeDelta> time_to_next_attempt = GetTimeToNextAttempt();
  if (time_to_next_attempt) {
    PA_LOG(INFO) << "Time until next DeviceSync attempt: "
                 << *time_to_next_attempt;
  } else {
    PA_LOG(INFO) << "No future DeviceSync requests currently scheduled";
  }

  if (!device_sync_result.IsSuccess()) {
    PA_LOG(INFO) << "Number of consecutive Enrollment failures: "
                 << scheduler_->GetNumConsecutiveEnrollmentFailures();
  }

  NotifyDeviceSyncFinished(device_sync_result);
}

}  // namespace device_sync

}  // namespace chromeos
