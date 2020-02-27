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
#include "device/fido/ble/fido_ble_device.h"
#include "device/fido/ble/fido_ble_discovery_base.h"
#include "device/fido/cable/cable_discovery_data.h"

namespace device {

class FidoCableDevice;
class BluetoothDevice;
class BluetoothAdvertisement;
class FidoCableHandshakeHandler;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoCableDiscovery
    : public FidoBleDiscoveryBase,
      public FidoBleDevice::Observer {
 public:
  FidoCableDiscovery(
      std::vector<CableDiscoveryData> discovery_data,
      base::Optional<QRGeneratorKey> qr_generator_key,
      base::Optional<
          base::RepeatingCallback<void(std::unique_ptr<CableDiscoveryData>)>>
          pairing_callback);
  ~FidoCableDiscovery() override;

  // FidoDeviceDiscovery:
  bool MaybeStop() override;

  const std::map<CableEidArray, scoped_refptr<BluetoothAdvertisement>>&
  AdvertisementsForTesting() const {
    return advertisements_;
  }

 protected:
  virtual base::Optional<std::unique_ptr<FidoCableHandshakeHandler>>
  CreateHandshakeHandler(FidoCableDevice* device,
                         const CableDiscoveryData& discovery_data,
                         const CableNonce& nonce,
                         const CableEidArray& eid);

 private:
  enum class CableV1DiscoveryEvent : int;

  // Result represents a successful match of a received EID against a specific
  // |FidoDiscoveryData|.
  struct Result {
    Result();
    Result(const CableDiscoveryData& in_discovery_data,
           const CableNonce& in_nonce,
           const CableEidArray& in_eid,
           base::Optional<int> ticks_back);
    Result(const Result&);
    ~Result();

    CableDiscoveryData discovery_data;
    CableNonce nonce;
    CableEidArray eid;
    // ticks_back is either |base::nullopt|, if the Result is from established
    // discovery pairings, or else contains the number of QR ticks back in time
    // against which the match was found.
    base::Optional<int> ticks_back;
  };

  // ObservedDeviceData contains potential EIDs observed from a BLE device. This
  // information is kept in order to de-duplicate device-log entries and make
  // debugging easier.
  struct ObservedDeviceData {
    ObservedDeviceData();
    ~ObservedDeviceData();

    base::Optional<CableEidArray> service_data;
    std::vector<CableEidArray> uuids;
    base::Optional<CableDiscoveryData> maybe_discovery_data;
  };

  // ResultDebugString returns a string containing a hex dump of |eid| and a
  // description of |result|, if present.
  static std::string ResultDebugString(const CableEidArray& eid,
                                       const base::Optional<Result>& result);
  static base::Optional<CableEidArray> MaybeGetEidFromServiceData(
      const BluetoothDevice* device);
  static std::vector<CableEidArray> GetUUIDs(const BluetoothDevice* device);

  void StartCableDiscovery();
  void OnStartDiscoverySession(std::unique_ptr<BluetoothDiscoverySession>);
  void OnStartDiscoverySessionError();
  void StartAdvertisement();
  void OnAdvertisementRegistered(
      const CableEidArray& client_eid,
      scoped_refptr<BluetoothAdvertisement> advertisement);

  // Attempt to stop all on-going advertisements in best-effort basis.
  // Once all the callbacks for Unregister() function is received, invoke
  // |callback|.
  void StopAdvertisements(base::OnceClosure callback);
  void OnAdvertisementsStopped(base::OnceClosure callback);
  void CableDeviceFound(BluetoothAdapter* adapter, BluetoothDevice* device);
  void ConductEncryptionHandshake(std::unique_ptr<FidoCableDevice> cable_device,
                                  Result discovery_data);
  void ValidateAuthenticatorHandshakeMessage(
      std::unique_ptr<FidoCableDevice> cable_device,
      CableDiscoveryData::Version cable_version,
      FidoCableHandshakeHandler* handshake_handler,
      base::Optional<std::vector<uint8_t>> handshake_response);

  base::Optional<Result> GetCableDiscoveryData(
      const BluetoothDevice* device) const;
  base::Optional<Result> GetCableDiscoveryDataFromAuthenticatorEid(
      CableEidArray authenticator_eid) const;
  bool IsObservedV1Device(const std::string& address) const;
  void RecordCableV1DiscoveryEventOnce(CableV1DiscoveryEvent event);

  // BluetoothAdapter::Observer:
  void DeviceAdded(BluetoothAdapter* adapter, BluetoothDevice* device) override;
  void DeviceChanged(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void DeviceRemoved(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void AdapterPoweredChanged(BluetoothAdapter* adapter, bool powered) override;

  // FidoBleDiscoveryBase:
  void OnGetAdapter(scoped_refptr<BluetoothAdapter> adapter) override;
  void OnSetPowered() override;

  // FidoeBleDevice::Observer:
  void FidoBleDeviceConnected(FidoBleDevice* device, bool success) override;
  void FidoBleDeviceTimeout(FidoBleDevice* device) override;

  std::vector<CableDiscoveryData> discovery_data_;
  // active_authenticator_eids_ contains authenticator EIDs for which a
  // handshake is currently running. Further advertisements for the same EIDs
  // will be ignored.
  std::set<CableEidArray> active_authenticator_eids_;
  // active_devices_ contains the BLE addresses of devices for which a handshake
  // is already running. Further advertisements from these devices will be
  // ignored. However, devices may rotate their BLE address at will so this is
  // not completely effective.
  std::set<std::string> active_devices_;
  base::Optional<QRGeneratorKey> qr_generator_key_;

  // Note that on Windows, |advertisements_| is the only reference holder of
  // BluetoothAdvertisement.
  std::map<CableEidArray, scoped_refptr<BluetoothAdvertisement>>
      advertisements_;

  std::vector<std::unique_ptr<FidoCableHandshakeHandler>>
      cable_handshake_handlers_;
  base::Optional<
      base::RepeatingCallback<void(std::unique_ptr<CableDiscoveryData>)>>
      pairing_callback_;

  // observed_devices_ caches the information from observed caBLE devices so
  // that the device-log isn't spammed.
  mutable base::flat_map<std::string, std::unique_ptr<ObservedDeviceData>>
      observed_devices_;
  // noted_obsolete_eids_ remembers QR-code EIDs that have been logged as
  // valid-but-expired in order to avoid spamming the device-log.
  mutable base::flat_set<CableEidArray> noted_obsolete_eids_;

  bool has_v1_discovery_data_ = false;
  base::flat_set<CableV1DiscoveryEvent> recorded_events_;

  base::WeakPtrFactory<FidoCableDiscovery> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FidoCableDiscovery);
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_FIDO_CABLE_DISCOVERY_H_
