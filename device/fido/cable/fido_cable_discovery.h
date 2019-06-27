// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_FIDO_CABLE_DISCOVERY_H_
#define DEVICE_FIDO_CABLE_FIDO_CABLE_DISCOVERY_H_

#include <stdint.h>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/ble/fido_ble_discovery_base.h"
#include "device/fido/cable/cable_discovery_data.h"

namespace device {

class FidoCableDevice;
class BluetoothDevice;
class BluetoothAdvertisement;
class FidoCableHandshakeHandler;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoCableDiscovery
    : public FidoBleDiscoveryBase {
 public:
  FidoCableDiscovery(std::vector<CableDiscoveryData> discovery_data);
  ~FidoCableDiscovery() override;

 protected:
  virtual std::unique_ptr<FidoCableHandshakeHandler> CreateHandshakeHandler(
      FidoCableDevice* device,
      base::span<const uint8_t, kSessionPreKeySize> session_pre_key,
      base::span<const uint8_t, 8> nonce);

 private:
  FRIEND_TEST_ALL_PREFIXES(FidoCableDiscoveryTest,
                           TestDiscoveryWithAdvertisementFailures);
  FRIEND_TEST_ALL_PREFIXES(FidoCableDiscoveryTest,
                           TestUnregisterAdvertisementUponDestruction);

  // BluetoothAdapter::Observer:
  void DeviceAdded(BluetoothAdapter* adapter, BluetoothDevice* device) override;
  void DeviceChanged(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void DeviceRemoved(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void AdapterPoweredChanged(BluetoothAdapter* adapter, bool powered) override;

  // FidoBleDiscoveryBase:
  void OnSetPowered() override;
  void OnStartDiscoverySessionWithFilter(
      std::unique_ptr<BluetoothDiscoverySession>) override;

  void StartCableDiscovery();
  void StartAdvertisement();
  void OnAdvertisementRegistered(
      const EidArray& client_eid,
      scoped_refptr<BluetoothAdvertisement> advertisement);
  void OnAdvertisementRegisterError(
      BluetoothAdvertisement::ErrorCode error_code);
  // Keeps a counter of success/failure of advertisements done by the client.
  // If all advertisements fail, then immediately stop discovery process and
  // invoke NotifyDiscoveryStarted(false). Otherwise kick off discovery session
  // once all advertisements has been processed.
  void RecordAdvertisementResult(bool is_success);
  // Attempt to stop all on-going advertisements in best-effort basis.
  // Once all the callbacks for Unregister() function is received, invoke
  // |callback|.
  void StopAdvertisements(base::OnceClosure callback);
  void CableDeviceFound(BluetoothAdapter* adapter, BluetoothDevice* device);
  void ConductEncryptionHandshake(
      std::unique_ptr<FidoCableDevice> device,
      base::span<const uint8_t, kSessionPreKeySize> session_pre_key,
      base::span<const uint8_t, 8> nonce);
  void ValidateAuthenticatorHandshakeMessage(
      std::unique_ptr<FidoCableDevice> cable_device,
      FidoCableHandshakeHandler* handshake_handler,
      base::Optional<std::vector<uint8_t>> handshake_response);

  const CableDiscoveryData* GetCableDiscoveryData(
      const BluetoothDevice* device) const;
  const CableDiscoveryData* GetCableDiscoveryDataFromServiceData(
      const BluetoothDevice* device) const;
  const CableDiscoveryData* GetCableDiscoveryDataFromServiceUUIDs(
      const BluetoothDevice* device) const;

  std::vector<CableDiscoveryData> discovery_data_;
  // active_authenticator_eids_ contains authenticator EIDs for which a
  // handshake is currently running. Further advertisements for the same EIDs
  // will be ignored.
  std::set<EidArray> active_authenticator_eids_;
  // active_devices_ contains the BLE addresses of devices for which a handshake
  // is already running. Further advertisements from these devices will be
  // ignored. However, devices may rotate their BLE address at will so this is
  // not completely effective.
  std::set<std::string> active_devices_;
  size_t advertisement_success_counter_ = 0;
  size_t advertisement_failure_counter_ = 0;
  std::map<EidArray, scoped_refptr<BluetoothAdvertisement>> advertisements_;
  std::vector<std::unique_ptr<FidoCableHandshakeHandler>>
      cable_handshake_handlers_;
  base::WeakPtrFactory<FidoCableDiscovery> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FidoCableDiscovery);
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_FIDO_CABLE_DISCOVERY_H_
