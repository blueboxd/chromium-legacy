// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/nearby_sharing/fast_initiation_manager.h"
#include "chrome/browser/nearby_sharing/nearby_connections_manager.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_prefs.h"
#include "components/prefs/pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

NearbySharingServiceImpl::NearbySharingServiceImpl(
    PrefService* prefs,
    Profile* profile,
    std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager)
    : prefs_(prefs),
      profile_(profile),
      nearby_connections_manager_(std::move(nearby_connections_manager)) {
  DCHECK(prefs_);
  DCHECK(profile_);
  DCHECK(nearby_connections_manager_);

  NearbyProcessManager& process_manager = NearbyProcessManager::GetInstance();
  nearby_process_observer_.Add(&process_manager);

  if (process_manager.IsActiveProfile(profile_)) {
    // TODO(crbug.com/1084576): Initialize NearbyConnectionsManager with
    // NearbyConnectionsMojom from |process_manager|:
    // process_manager.GetOrStartNearbyConnections(profile_)
  }

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kNearbySharingEnabledPrefName,
      base::BindRepeating(&NearbySharingServiceImpl::OnEnabledPrefChanged,
                          base::Unretained(this)));

  GetBluetoothAdapter();
}

NearbySharingServiceImpl::~NearbySharingServiceImpl() {
  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
}

void NearbySharingServiceImpl::RegisterSendSurface(
    TransferUpdateCallback* transferCallback,
    ShareTargetDiscoveredCallback* discoveryCallback,
    StatusCodesCallback status_codes_callback) {
  register_send_surface_callback_ = std::move(status_codes_callback);
  StartFastInitiationAdvertising();
}

void NearbySharingServiceImpl::UnregisterSendSurface(
    TransferUpdateCallback* transferCallback,
    ShareTargetDiscoveredCallback* discoveryCallback,
    StatusCodesCallback status_codes_callback) {
  unregister_send_surface_callback_ = std::move(status_codes_callback);
  StopFastInitiationAdvertising();
}

void NearbySharingServiceImpl::RegisterReceiveSurface(
    TransferUpdateCallback* transferCallback,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::UnregisterReceiveSurface(
    TransferUpdateCallback* transferCallback,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::NearbySharingServiceImpl::SendText(
    const ShareTarget& share_target,
    std::string text,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::SendFiles(
    const ShareTarget& share_target,
    const std::vector<base::FilePath>& files,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::Accept(
    const ShareTarget& share_target,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::Reject(
    const ShareTarget& share_target,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::Cancel(
    const ShareTarget& share_target,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::Open(const ShareTarget& share_target,
                                    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::OnNearbyProfileChanged(Profile* profile) {
  // TODO(crbug.com/1084576): Notify UI about the new active profile.
}

void NearbySharingServiceImpl::OnNearbyProcessStarted() {
  NearbyProcessManager& process_manager = NearbyProcessManager::GetInstance();
  if (process_manager.IsActiveProfile(profile_))
    VLOG(1) << __func__ << ": Nearby process started!";
}

void NearbySharingServiceImpl::OnNearbyProcessStopped() {
  NearbyProcessManager& process_manager = NearbyProcessManager::GetInstance();
  if (process_manager.IsActiveProfile(profile_)) {
    // TODO(crbug.com/1084576): Check if process should be running and restart
    // it after a delay.
  }
}

bool NearbySharingServiceImpl::IsEnabled() {
  return prefs_->GetBoolean(prefs::kNearbySharingEnabledPrefName);
}

void NearbySharingServiceImpl::OnEnabledPrefChanged() {
  if (IsEnabled()) {
    VLOG(1) << __func__ << ": Nearby sharing enabled!";
  } else {
    VLOG(1) << __func__ << ": Nearby sharing disabled!";
    // TODO(crbug/1084647): Stop advertising.
    // TODO(crbug/1085067): Stop discovery.
    nearby_connections_manager_->Shutdown();
  }
}

void NearbySharingServiceImpl::StartFastInitiationAdvertising() {
  if (!IsBluetoothPresent() || !IsBluetoothPowered()) {
    std::move(register_send_surface_callback_).Run(StatusCodes::kError);
    return;
  }

  if (fast_initiation_manager_) {
    // TODO(hansenmichael): Do not invoke
    // |register_send_surface_callback_| until Nearby Connections
    // scanning is kicked off.
    std::move(register_send_surface_callback_).Run(StatusCodes::kOk);
    return;
  }

  fast_initiation_manager_ =
      FastInitiationManager::Factory::Create(bluetooth_adapter_);
  fast_initiation_manager_->StartAdvertising(
      base::BindOnce(
          &NearbySharingServiceImpl::OnStartFastInitiationAdvertising,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &NearbySharingServiceImpl::OnStartFastInitiationAdvertisingError,
          weak_ptr_factory_.GetWeakPtr()));
}

void NearbySharingServiceImpl::StopFastInitiationAdvertising() {
  if (!fast_initiation_manager_) {
    if (unregister_send_surface_callback_)
      std::move(unregister_send_surface_callback_).Run(StatusCodes::kOk);
    return;
  }

  fast_initiation_manager_->StopAdvertising(
      base::BindOnce(&NearbySharingServiceImpl::OnStopFastInitiationAdvertising,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbySharingServiceImpl::GetBluetoothAdapter() {
  auto* adapter_factory = device::BluetoothAdapterFactory::Get();
  if (!adapter_factory->IsBluetoothSupported())
    return;

  // Because this will be called from the constructor, GetAdapter() may call
  // OnGetBluetoothAdapter() immediately which can cause problems during tests
  // since the class is not fully constructed yet.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &device::BluetoothAdapterFactory::GetAdapter,
          base::Unretained(adapter_factory),
          base::BindOnce(&NearbySharingServiceImpl::OnGetBluetoothAdapter,
                         weak_ptr_factory_.GetWeakPtr())));
}

void NearbySharingServiceImpl::OnGetBluetoothAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  bluetooth_adapter_->AddObserver(this);
}

void NearbySharingServiceImpl::OnStartFastInitiationAdvertising() {
  // TODO(hansenmichael): Do not invoke
  // |register_send_surface_callback_| until Nearby Connections
  // scanning is kicked off.
  std::move(register_send_surface_callback_).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::OnStartFastInitiationAdvertisingError() {
  fast_initiation_manager_.reset();
  std::move(register_send_surface_callback_).Run(StatusCodes::kError);
}

void NearbySharingServiceImpl::OnStopFastInitiationAdvertising() {
  fast_initiation_manager_.reset();

  // TODO(hansenmichael): Do not invoke
  // |unregister_send_surface_callback_| until Nearby Connections
  // scanning is stopped.
  if (unregister_send_surface_callback_)
    std::move(unregister_send_surface_callback_).Run(StatusCodes::kOk);
}

bool NearbySharingServiceImpl::IsBluetoothPresent() const {
  return bluetooth_adapter_.get() && bluetooth_adapter_->IsPresent();
}

bool NearbySharingServiceImpl::IsBluetoothPowered() const {
  return IsBluetoothPresent() && bluetooth_adapter_->IsPowered();
}

void NearbySharingServiceImpl::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  if (!present)
    StopFastInitiationAdvertising();
}

void NearbySharingServiceImpl::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  if (!powered)
    StopFastInitiationAdvertising();
}
