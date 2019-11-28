// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/remote_device_provider_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/device_sync/remote_device_loader.h"
#include "chromeos/services/device_sync/remote_device_v2_loader_impl.h"

namespace chromeos {

namespace {

// Converts |user_account_id| to a string that can be used as a user id when
// creating a remote device loader.
std::string ToRemoteDeviceUserId(const CoreAccountId& user_account_id) {
  // TODO(msarda): For now simply return the underlying id of |user_account_id|.
  return user_account_id.ToString();
}

}  // namespace

namespace device_sync {

// static
RemoteDeviceProviderImpl::Factory*
    RemoteDeviceProviderImpl::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<RemoteDeviceProvider>
RemoteDeviceProviderImpl::Factory::NewInstance(
    CryptAuthDeviceManager* v1_device_manager,
    CryptAuthV2DeviceManager* v2_device_manager,
    const CoreAccountId& user_account_id,
    const std::string& user_private_key) {
  if (!factory_instance_) {
    factory_instance_ = new Factory();
  }

  return factory_instance_->BuildInstance(v1_device_manager, v2_device_manager,
                                          user_account_id, user_private_key);
}

// static
void RemoteDeviceProviderImpl::Factory::SetInstanceForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

RemoteDeviceProviderImpl::Factory::~Factory() = default;

std::unique_ptr<RemoteDeviceProvider>
RemoteDeviceProviderImpl::Factory::BuildInstance(
    CryptAuthDeviceManager* v1_device_manager,
    CryptAuthV2DeviceManager* v2_device_manager,
    const CoreAccountId& user_account_id,
    const std::string& user_private_key) {
  return base::WrapUnique(new RemoteDeviceProviderImpl(
      v1_device_manager, v2_device_manager, user_account_id, user_private_key));
}

RemoteDeviceProviderImpl::RemoteDeviceProviderImpl(
    CryptAuthDeviceManager* v1_device_manager,
    CryptAuthV2DeviceManager* v2_device_manager,
    const CoreAccountId& user_account_id,
    const std::string& user_private_key)
    : v1_device_manager_(v1_device_manager),
      v2_device_manager_(v2_device_manager),
      user_account_id_(user_account_id),
      user_private_key_(user_private_key) {
  if (!features::ShouldDeprecateV1DeviceSync()) {
    DCHECK(v1_device_manager_);
    v1_device_manager_->AddObserver(this);
    LoadV1RemoteDevices();
  }

  if (features::ShouldUseV2DeviceSync()) {
    DCHECK(v2_device_manager_);
    v2_device_manager_->AddObserver(this);
    LoadV2RemoteDevices();
  }
}

RemoteDeviceProviderImpl::~RemoteDeviceProviderImpl() {
  if (v1_device_manager_)
    v1_device_manager_->RemoveObserver(this);

  if (v2_device_manager_)
    v2_device_manager_->RemoveObserver(this);
}

void RemoteDeviceProviderImpl::OnSyncFinished(
    CryptAuthDeviceManager::SyncResult sync_result,
    CryptAuthDeviceManager::DeviceChangeResult device_change_result) {
  DCHECK(!features::ShouldDeprecateV1DeviceSync());

  if (sync_result == CryptAuthDeviceManager::SyncResult::SUCCESS &&
      device_change_result ==
          CryptAuthDeviceManager::DeviceChangeResult::CHANGED) {
    LoadV1RemoteDevices();
  }
}

void RemoteDeviceProviderImpl::OnDeviceSyncFinished(
    const CryptAuthDeviceSyncResult& device_sync_result) {
  DCHECK(features::ShouldUseV2DeviceSync());

  if (device_sync_result.IsSuccess() &&
      device_sync_result.did_device_registry_change()) {
    LoadV2RemoteDevices();
  }
}

void RemoteDeviceProviderImpl::LoadV1RemoteDevices() {
  remote_device_v1_loader_ = RemoteDeviceLoader::Factory::NewInstance(
      v1_device_manager_->GetSyncedDevices(),
      ToRemoteDeviceUserId(user_account_id_), user_private_key_,
      multidevice::SecureMessageDelegateImpl::Factory::NewInstance());
  remote_device_v1_loader_->Load(
      base::Bind(&RemoteDeviceProviderImpl::OnV1RemoteDevicesLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

void RemoteDeviceProviderImpl::LoadV2RemoteDevices() {
  remote_device_v2_loader_ =
      RemoteDeviceV2LoaderImpl::Factory::Get()->BuildInstance();
  remote_device_v2_loader_->Load(
      v2_device_manager_->GetSyncedDevices(),
      ToRemoteDeviceUserId(user_account_id_), user_private_key_,
      base::Bind(&RemoteDeviceProviderImpl::OnV2RemoteDevicesLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

void RemoteDeviceProviderImpl::OnV1RemoteDevicesLoaded(
    const multidevice::RemoteDeviceList& synced_v1_remote_devices) {
  // If we are only using v1 DeviceSync, the complete list of RemoteDevices
  // is |synced_v1_remote_devices|.
  if (!features::ShouldUseV2DeviceSync()) {
    synced_remote_devices_ = synced_v1_remote_devices;
    remote_device_v1_loader_.reset();

    // Notify observers of change. Note that there is no need to check if
    // |synced_remote_devices_| has changed here because the fetch is only
    // started if the change result passed to OnSyncFinished() is CHANGED.
    RemoteDeviceProvider::NotifyObserversDeviceListChanged();
    return;
  }

  synced_v1_remote_devices_to_be_merged_ = synced_v1_remote_devices;
  remote_device_v1_loader_.reset();
  MergeV1andV2SyncedDevices();
}

void RemoteDeviceProviderImpl::OnV2RemoteDevicesLoaded(
    const multidevice::RemoteDeviceList& synced_v2_remote_devices) {
  // If we are only using v2 DeviceSync, the complete list of RemoteDevices
  // is |synced_v2_remote_devices|.
  if (features::ShouldDeprecateV1DeviceSync()) {
    synced_remote_devices_ = synced_v2_remote_devices;
    remote_device_v2_loader_.reset();

    // Notify observers of change. Note that there is no need to check if
    // |synced_remote_devices_| has changed here because the fetch is only
    // started if the DeviceSync result passed to OnDeviceSyncFinished()
    // indicates that the device registry changed.
    RemoteDeviceProvider::NotifyObserversDeviceListChanged();
    return;
  }

  synced_v2_remote_devices_to_be_merged_ = synced_v2_remote_devices;
  remote_device_v2_loader_.reset();
  MergeV1andV2SyncedDevices();
}

void RemoteDeviceProviderImpl::MergeV1andV2SyncedDevices() {
  DCHECK(features::ShouldUseV2DeviceSync());
  DCHECK(!features::ShouldDeprecateV1DeviceSync());

  multidevice::RemoteDeviceList previous_synced_remote_devices =
      synced_remote_devices_;

  synced_remote_devices_ = synced_v1_remote_devices_to_be_merged_;
  for (const auto& v2_device : synced_v2_remote_devices_to_be_merged_) {
    // Ignore v2 devices without a decrypted public key.
    if (v2_device.public_key.empty())
      continue;

    std::string v2_public_key = v2_device.public_key;
    auto it = std::find_if(
        synced_remote_devices_.begin(), synced_remote_devices_.end(),
        [&v2_public_key](const multidevice::RemoteDevice& v1_device) {
          return v1_device.public_key == v2_public_key;
        });

    // If a v1 device has the same public key as the v2 device, replace the
    // v1 device with the v2 device; otherwise, append the v2 device to the
    // synced-device list.
    if (it != synced_remote_devices_.end())
      *it = v2_device;
    else
      synced_remote_devices_.push_back(v2_device);
  }

  // We need to explicitly check for changes to the synced-device list. It
  // is possible that the v1 and/or v2 device lists changed but the merged
  // list didn't change, for example, if a new v2 device appears in the
  // device registry but it doesn't have a decrypted public key.
  if (synced_remote_devices_ != previous_synced_remote_devices)
    RemoteDeviceProvider::NotifyObserversDeviceListChanged();
}

const multidevice::RemoteDeviceList&
RemoteDeviceProviderImpl::GetSyncedDevices() const {
  return synced_remote_devices_;
}

}  // namespace device_sync

}  // namespace chromeos
