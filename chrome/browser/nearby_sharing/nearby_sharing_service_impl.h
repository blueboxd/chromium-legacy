// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/nearby_sharing/nearby_connections_manager.h"
#include "chrome/browser/nearby_sharing/nearby_constants.h"
#include "chrome/browser/nearby_sharing/nearby_notification_manager.h"
#include "chrome/browser/nearby_sharing/nearby_process_manager.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class FastInitiationManager;
class NearbyConnectionsManager;
class PrefService;
class Profile;

class NearbySharingServiceImpl
    : public NearbySharingService,
      public KeyedService,
      public NearbyProcessManager::Observer,
      public device::BluetoothAdapter::Observer,
      public NearbyConnectionsManager::IncomingConnectionListener {
 public:
  explicit NearbySharingServiceImpl(
      PrefService* prefs,
      Profile* profile,
      std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager);
  ~NearbySharingServiceImpl() override;

  // NearbySharingService:
  void RegisterSendSurface(TransferUpdateCallback* transfer_callback,
                           ShareTargetDiscoveredCallback* discovery_callback,
                           StatusCodesCallback status_codes_callback) override;
  void UnregisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback,
      StatusCodesCallback status_codes_callback) override;
  StatusCodes RegisterReceiveSurface(TransferUpdateCallback* transfer_callback,
                                     ReceiveSurfaceState state) override;
  StatusCodes UnregisterReceiveSurface(
      TransferUpdateCallback* transfer_callback) override;
  void SendText(const ShareTarget& share_target,
                std::string text,
                StatusCodesCallback status_codes_callback) override;
  void SendFiles(const ShareTarget& share_target,
                 const std::vector<base::FilePath>& files,
                 StatusCodesCallback status_codes_callback) override;
  void Accept(const ShareTarget& share_target,
              StatusCodesCallback status_codes_callback) override;
  void Reject(const ShareTarget& share_target,
              StatusCodesCallback status_codes_callback) override;
  void Cancel(const ShareTarget& share_target,
              StatusCodesCallback status_codes_callback) override;
  void Open(const ShareTarget& share_target,
            StatusCodesCallback status_codes_callback) override;

  // NearbyProcessManager::Observer:
  void OnNearbyProfileChanged(Profile* profile) override;
  void OnNearbyProcessStarted() override;
  void OnNearbyProcessStopped() override;

  // NearbyConnectionsManager::IncomingConnectionListener:
  void OnIncomingConnection(
      const std::string& endpoint_id,
      const std::vector<uint8_t>& endpoint_info,
      std::unique_ptr<NearbyConnection> connection) override;

 private:
  bool IsEnabled();
  void OnEnabledPrefChanged();
  Visibility GetVisibilityPref();
  bool IsVisibleInBackground(Visibility visibility);
  void OnVisibilityPrefChanged();
  DataUsage GetDataUsagePref();
  void OnDataUsagePrefChanged();
  void StartFastInitiationAdvertising();
  void StopFastInitiationAdvertising();
  void GetBluetoothAdapter();
  void OnGetBluetoothAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void OnStartFastInitiationAdvertising();
  void OnStartFastInitiationAdvertisingError();
  void OnStopFastInitiationAdvertising();
  bool IsBluetoothPresent() const;
  bool IsBluetoothPowered() const;
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void InvalidateReceiveSurfaceState();
  void InvalidateAdvertisingState();
  void StopAdvertising();

  PrefService* prefs_;
  Profile* profile_;
  std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager_;
  PrefChangeRegistrar pref_change_registrar_;
  ScopedObserver<NearbyProcessManager, NearbyProcessManager::Observer>
      nearby_process_observer_{this};
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  std::unique_ptr<FastInitiationManager> fast_initiation_manager_;
  StatusCodesCallback register_send_surface_callback_;
  StatusCodesCallback unregister_send_surface_callback_;
  NearbyNotificationManager nearby_notification_manager_;

  // A list of foreground receivers.
  base::ObserverList<TransferUpdateCallback> foreground_receive_callbacks_;
  // A list of foreground receivers.
  base::ObserverList<TransferUpdateCallback> background_receive_callbacks_;

  // Registers the most recent TransferMetadata and ShareTarget used for
  // transitioning notifications between foreground surfaces and background
  // surfaces. Empty if no metadata is available.
  base::Optional<std::pair<ShareTarget, TransferMetadata>>
      last_incoming_metadata_;
  // The most recent outgoing TransferMetadata and ShareTarget.
  base::Optional<std::pair<ShareTarget, TransferMetadata>>
      last_outgoing_metadata_;

  // The current advertising power level. PowerLevel::kUnknown while not
  // advertising.
  PowerLevel advertising_power_level_ = PowerLevel::kUnknown;
  // The current advertising data usage preference. We need to restart scan
  // (Fast Init) or advertise (Nearby Connections or Fast Init) when online
  // preference changes. DataUsage::kUnknown while not advertising.
  DataUsage advertising_data_usage_preference_ = DataUsage::kUnknown;
  // The current visibility preference. We need to restart advertising if
  // the visibility changes.
  Visibility advertising_visibilty_preference_ = Visibility::kUnknown;
  // True if we are currently scanning for remote devices.
  bool is_scanning_ = false;
  // True if we're currently sending or receiving a file.
  bool is_transferring_files_ = false;

  base::WeakPtrFactory<NearbySharingServiceImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_
