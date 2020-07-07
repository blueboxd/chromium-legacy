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
#include "chrome/services/sharing/public/cpp/advertisement.h"
#include "chrome/services/sharing/public/mojom/nearby_connections_types.mojom.h"
#include "components/prefs/pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "ui/base/idle/idle.h"

namespace {

std::string ReceiveSurfaceStateToString(
    NearbySharingService::ReceiveSurfaceState state) {
  switch (state) {
    case NearbySharingService::ReceiveSurfaceState::kForeground:
      return "FOREGROUND";
    case NearbySharingService::ReceiveSurfaceState::kBackground:
      return "BACKGROUND";
    case NearbySharingService::ReceiveSurfaceState::kUnknown:
      return "UNKNOWN";
  }
}

std::string DataUsageToString(DataUsage usage) {
  switch (usage) {
    case DataUsage::kOffline:
      return "OFFLINE";
    case DataUsage::kOnline:
      return "ONLINE";
    case DataUsage::kWifiOnly:
      return "WIFI_ONLY";
    case DataUsage::kUnknown:
      return "UNKNOWN";
  }
}

std::string PowerLevelToString(PowerLevel level) {
  switch (level) {
    case PowerLevel::kLowPower:
      return "LOW_POWER";
    case PowerLevel::kMediumPower:
      return "MEDIUM_POWER";
    case PowerLevel::kHighPower:
      return "HIGH_POWER";
    case PowerLevel::kUnknown:
      return "UNKNOWN";
  }
}

std::string VisibilityToString(Visibility visibility) {
  switch (visibility) {
    case Visibility::kNoOne:
      return "NO_ONE";
    case Visibility::kAllContacts:
      return "ALL_CONTACTS";
    case Visibility::kSelectedContacts:
      return "SELECTED_CONTACTS";
    case Visibility::kUnknown:
      return "UNKNOWN";
  }
}

std::string ConnectionsStatusToString(
    NearbyConnectionsManager::ConnectionsStatus status) {
  switch (status) {
    case NearbyConnectionsManager::ConnectionsStatus::kSuccess:
      return "SUCCESS";
    case NearbyConnectionsManager::ConnectionsStatus::kError:
      return "ERROR";
    case NearbyConnectionsManager::ConnectionsStatus::kOutOfOrderApiCall:
      return "OUT_OF_ORDER_API_CALL";
    case NearbyConnectionsManager::ConnectionsStatus::
        kAlreadyHaveActiveStrategy:
      return "ALREADY_HAVE_ACTIVE_STRATEGY";
    case NearbyConnectionsManager::ConnectionsStatus::kAlreadyAdvertising:
      return "ALREADY_ADVERTISING";
    case NearbyConnectionsManager::ConnectionsStatus::kAlreadyDiscovering:
      return "ALREADY_DISCOVERING";
    case NearbyConnectionsManager::ConnectionsStatus::kEndpointIOError:
      return "ENDPOINT_IO_ERROR";
    case NearbyConnectionsManager::ConnectionsStatus::kEndpointUnknown:
      return "ENDPOINT_UNKNOWN";
    case NearbyConnectionsManager::ConnectionsStatus::kConnectionRejected:
      return "CONNECTION_REJECTED";
    case NearbyConnectionsManager::ConnectionsStatus::
        kAlreadyConnectedToEndpoint:
      return "ALREADY_CONNECTED_TO_ENDPOINT";
    case NearbyConnectionsManager::ConnectionsStatus::kNotConnectedToEndpoint:
      return "NOT_CONNECTED_TO_ENDPOINT";
    case NearbyConnectionsManager::ConnectionsStatus::kRadioError:
      return "RADIO_ERROR";
    case NearbyConnectionsManager::ConnectionsStatus::kPayloadUnknown:
      return "PAYLOAD_UNKNOWN";
  }
}

}  // namespace

NearbySharingServiceImpl::NearbySharingServiceImpl(
    PrefService* prefs,
    Profile* profile,
    std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager)
    : prefs_(prefs),
      profile_(profile),
      nearby_connections_manager_(std::move(nearby_connections_manager)),
      nearby_notification_manager_(profile) {
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
  pref_change_registrar_.Add(
      prefs::kNearbySharingBackgroundVisibilityName,
      base::BindRepeating(&NearbySharingServiceImpl::OnVisibilityPrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kNearbySharingDataUsageName,
      base::BindRepeating(&NearbySharingServiceImpl::OnDataUsagePrefChanged,
                          base::Unretained(this)));

  GetBluetoothAdapter();
}

NearbySharingServiceImpl::~NearbySharingServiceImpl() {
  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
}

void NearbySharingServiceImpl::RegisterSendSurface(
    TransferUpdateCallback* transfer_callback,
    ShareTargetDiscoveredCallback* discovery_callback,
    StatusCodesCallback status_codes_callback) {
  register_send_surface_callback_ = std::move(status_codes_callback);
  StartFastInitiationAdvertising();
}

void NearbySharingServiceImpl::UnregisterSendSurface(
    TransferUpdateCallback* transfer_callback,
    ShareTargetDiscoveredCallback* discovery_callback,
    StatusCodesCallback status_codes_callback) {
  unregister_send_surface_callback_ = std::move(status_codes_callback);
  StopFastInitiationAdvertising();
}

NearbySharingService::StatusCodes
NearbySharingServiceImpl::RegisterReceiveSurface(
    TransferUpdateCallback* transfer_callback,
    ReceiveSurfaceState state) {
  DCHECK(transfer_callback);
  DCHECK_NE(state, ReceiveSurfaceState::kUnknown);
  if (foreground_receive_callbacks_.HasObserver(transfer_callback) ||
      background_receive_callbacks_.HasObserver(transfer_callback)) {
    VLOG(1) << __func__
            << ": registerReceiveSurface failed. Already registered.";
    return StatusCodes::kError;
  }

  // If the receive surface to be registered is a foreground surface, let it
  // catch up with most recent transfer metadata immediately.
  if (state == ReceiveSurfaceState::kForeground && last_incoming_metadata_) {
    transfer_callback->OnTransferUpdate(last_incoming_metadata_->first,
                                        last_incoming_metadata_->second);
  }

  if (state == ReceiveSurfaceState::kForeground) {
    foreground_receive_callbacks_.AddObserver(transfer_callback);
  } else {
    background_receive_callbacks_.AddObserver(transfer_callback);
  }

  VLOG(1) << __func__ << ": A ReceiveSurface("
          << ReceiveSurfaceStateToString(state) << ") has been registered";
  InvalidateReceiveSurfaceState();
  return StatusCodes::kOk;
}

NearbySharingService::StatusCodes
NearbySharingServiceImpl::UnregisterReceiveSurface(
    TransferUpdateCallback* transfer_callback) {
  DCHECK(transfer_callback);
  bool is_foreground =
      foreground_receive_callbacks_.HasObserver(transfer_callback);
  bool is_background =
      background_receive_callbacks_.HasObserver(transfer_callback);
  if (!is_foreground && !is_background) {
    VLOG(1)
        << __func__
        << ": unregisterReceiveSurface failed. Unknown TransferUpdateCallback.";
    return StatusCodes::kError;
  }

  if (foreground_receive_callbacks_.might_have_observers() &&
      last_incoming_metadata_ &&
      last_incoming_metadata_->second.is_final_status()) {
    // We already saw the final status in the foreground.
    // Nullify it so the next time the user opens sharing, it starts the UI from
    // the beginning
    last_incoming_metadata_.reset();
  }

  if (is_foreground) {
    foreground_receive_callbacks_.RemoveObserver(transfer_callback);
  } else {
    background_receive_callbacks_.RemoveObserver(transfer_callback);
  }

  // Displays the most recent payload status processed by foreground surfaces on
  // background surface.
  if (!foreground_receive_callbacks_.might_have_observers() &&
      last_incoming_metadata_) {
    for (TransferUpdateCallback& background_callback :
         background_receive_callbacks_) {
      background_callback.OnTransferUpdate(last_incoming_metadata_->first,
                                           last_incoming_metadata_->second);
    }
  }

  VLOG(1) << __func__ << ": A ReceiveSurface("
          << (is_foreground ? "foreground" : "background")
          << ") has been unregistered";

  InvalidateReceiveSurfaceState();
  return StatusCodes::kOk;
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

void NearbySharingServiceImpl::OnIncomingConnection(
    const std::string& endpoint_id,
    const std::vector<uint8_t>& endpoint_info,
    std::unique_ptr<NearbyConnection> connection) {
  // TODO(crbug/1085068): Handle incoming connection; use CertificateManager
}

bool NearbySharingServiceImpl::IsEnabled() {
  return prefs_->GetBoolean(prefs::kNearbySharingEnabledPrefName);
}

void NearbySharingServiceImpl::OnEnabledPrefChanged() {
  if (IsEnabled()) {
    VLOG(1) << __func__ << ": Nearby sharing enabled!";
  } else {
    VLOG(1) << __func__ << ": Nearby sharing disabled!";
    StopAdvertising();
    // TODO(crbug/1085067): Stop discovery.
    nearby_connections_manager_->Shutdown();
  }
}

bool NearbySharingServiceImpl::IsVisibleInBackground(Visibility visibility) {
  return visibility == Visibility::kAllContacts ||
         visibility == Visibility::kSelectedContacts;
}

Visibility NearbySharingServiceImpl::GetVisibilityPref() {
  int visibility =
      prefs_->GetInteger(prefs::kNearbySharingBackgroundVisibilityName);
  if (visibility < 0 || visibility > static_cast<int>(Visibility::kMaxValue))
    return Visibility::kUnknown;

  return static_cast<Visibility>(visibility);
}

void NearbySharingServiceImpl::OnVisibilityPrefChanged() {
  Visibility new_visibility = GetVisibilityPref();
  if (advertising_visibilty_preference_ == new_visibility) {
    VLOG(1) << __func__ << ": Nearby sharing visibility pref is unchanged";
    return;
  }

  advertising_visibilty_preference_ = new_visibility;
  VLOG(1) << __func__ << ": Nearby sharing visibility changed to "
          << VisibilityToString(advertising_visibilty_preference_);

  if (advertising_power_level_ != PowerLevel::kUnknown) {
    StopAdvertising();
  }

  InvalidateReceiveSurfaceState();
}

DataUsage NearbySharingServiceImpl::GetDataUsagePref() {
  int usage = prefs_->GetInteger(prefs::kNearbySharingDataUsageName);
  if (usage < 0 || usage > static_cast<int>(DataUsage::kMaxValue))
    return DataUsage::kUnknown;

  return static_cast<DataUsage>(usage);
}

void NearbySharingServiceImpl::OnDataUsagePrefChanged() {
  DataUsage new_data_usage = GetDataUsagePref();
  if (advertising_data_usage_preference_ == new_data_usage) {
    VLOG(1) << __func__ << ": Nearby sharing data usage pref is unchanged";
    return;
  }

  VLOG(1) << __func__ << ": Nearby sharing data usage changed.";
  if (advertising_power_level_ != PowerLevel::kUnknown) {
    StopAdvertising();
  }

  InvalidateReceiveSurfaceState();
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

  // TODO(crbug.com/1100686): Determine whether to call StartAdvertising() with
  // kNotify or kSilent.
  fast_initiation_manager_->StartAdvertising(
      FastInitiationManager::FastInitType::kNotify,
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

void NearbySharingServiceImpl::InvalidateReceiveSurfaceState() {
  InvalidateAdvertisingState();
  // TODO(crbug/154846208) InvalidateFastInitScan();
}

void NearbySharingServiceImpl::InvalidateAdvertisingState() {
  // Screen is off. Do no work.
  if (ui::CheckIdleStateIsLocked()) {
    StopAdvertising();
    VLOG(1) << __func__
            << ": Stopping advertising because the screen is locked.";
    return;
  }

  // Check if Wifi or Ethernet LAN is off.  Advertisements won't work, so
  // disable them, unless bluetooth is known to be enabled. Not all platforms
  // have bluetooth, so wifi LAN is a platform-agnostic check.
  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();
  if (!IsBluetoothPresent() &&
      !(connection_type ==
            net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI ||
        connection_type ==
            net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET)) {
    StopAdvertising();
    VLOG(1) << __func__
            << ": Stopping advertising because both bluetooth and wifi LAN are "
               "disabled.";
    return;
  }

  // Nearby Sharing is disabled. Don't advertise.
  if (!IsEnabled()) {
    StopAdvertising();
    VLOG(1) << __func__
            << ": Stopping advertising because Nearby Sharing is disabled.";
    return;
  }

  // We're scanning for other nearby devices. Don't advertise.
  if (is_scanning_) {
    StopAdvertising();
    VLOG(1)
        << __func__
        << ": Stopping advertising because we're scanning for other devices.";
    return;
  }

  if (is_transferring_files_) {
    StopAdvertising();
    VLOG(1) << __func__
            << ": Stopping advertising because we're currently in the midst of "
               "a transfer.";
    return;
  }

  if (!foreground_receive_callbacks_.might_have_observers() &&
      !background_receive_callbacks_.might_have_observers()) {
    StopAdvertising();
    VLOG(1)
        << __func__
        << ": Stopping advertising because no receive surface is registered.";
    return;
  }

  if (!IsVisibleInBackground(advertising_visibilty_preference_) &&
      !foreground_receive_callbacks_.might_have_observers()) {
    StopAdvertising();
    VLOG(1) << __func__
            << ": Stopping advertising because no high power receive surface "
               "is registered and device is visible to NO_ONE.";
    return;
  }

  PowerLevel power_level;
  if (foreground_receive_callbacks_.might_have_observers()) {
    power_level = PowerLevel::kHighPower;
    // TODO(crbug/1100367) handle fast init
    // } else if (isFastInitDeviceNearby) {
    //   power_level = PowerLevel::kMediumPower;
  } else {
    power_level = PowerLevel::kLowPower;
  }

  DataUsage data_usage = GetDataUsagePref();
  if (advertising_power_level_ != PowerLevel::kUnknown) {
    if (power_level == advertising_power_level_ &&
        data_usage == advertising_data_usage_preference_) {
      VLOG(1) << __func__
              << "Failed to advertise because we're already advertising with "
                 "power level "
              << PowerLevelToString(advertising_power_level_)
              << " and data usage preference "
              << DataUsageToString(advertising_data_usage_preference_);
      return;
    }

    StopAdvertising();
    VLOG(1) << __func__ << ": Restart advertising with power level "
            << PowerLevelToString(power_level) << " and data usage preference "
            << DataUsageToString(data_usage);
  }

  // Starts advertising through Nearby Connections. Caller is expected to ensure
  // |listener| remains valid until StopAdvertising is called.

  // TODO(nmusgrave) fill values from CertificateManager
  std::vector<uint8_t> salt(sharing::Advertisement::kSaltSize, 0);
  std::vector<uint8_t> encrypted_metadata_key(
      sharing::Advertisement::kMetadataEncryptionKeyHashByteSize, 0);

  // TODO(nmusgrave) fill value from local device data manager
  base::Optional<std::string> device_name = "todo_device_name";
  std::vector<uint8_t> endpoint_info =
      sharing::Advertisement::NewInstance(std::move(salt),
                                          std::move(encrypted_metadata_key),
                                          std::move(device_name))
          ->ToEndpointInfo();
  nearby_connections_manager_->StartAdvertising(
      std::move(endpoint_info),
      /* listener= */ this, power_level, data_usage,
      base::BindOnce([](NearbyConnectionsManager::ConnectionsStatus status) {
        VLOG(1)
            << __func__
            << ": Advertising attempted over Nearby Connections with result "
            << ConnectionsStatusToString(status);
      }));

  advertising_power_level_ = power_level;
  advertising_data_usage_preference_ = data_usage;
  VLOG(1) << __func__ << ": Advertising has started over Nearby Connections: "
          << " power level " << PowerLevelToString(power_level)
          << " visibility "
          << VisibilityToString(advertising_visibilty_preference_)
          << " data usage " << DataUsageToString(data_usage);
  return;
}

void NearbySharingServiceImpl::StopAdvertising() {
  if (advertising_power_level_ == PowerLevel::kUnknown) {
    VLOG(1) << __func__
            << ": Failed to stop advertising because we weren't advertising";
    return;
  }

  nearby_connections_manager_->StopAdvertising();

  advertising_data_usage_preference_ = DataUsage::kUnknown;
  advertising_power_level_ = PowerLevel::kUnknown;
  VLOG(1) << __func__ << ": Advertising has stopped";
}
