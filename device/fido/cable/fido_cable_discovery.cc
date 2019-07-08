// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_discovery.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/fido/ble/fido_ble_uuids.h"
#include "device/fido/cable/fido_cable_device.h"
#include "device/fido/cable/fido_cable_handshake_handler.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

namespace {

// Construct advertisement data with different formats depending on client's
// operating system. Ideally, we advertise EIDs as part of Service Data, but
// this isn't available on all platforms. On Windows we use Manufacturer Data
// instead, and on Mac our only option is to advertise an additional service
// with the EID as its UUID.
std::unique_ptr<BluetoothAdvertisement::Data> ConstructAdvertisementData(
    uint8_t version_number,
    base::span<const uint8_t, kEphemeralIdSize> client_eid) {
  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::AdvertisementType::ADVERTISEMENT_TYPE_BROADCAST);

#if defined(OS_MACOSX)
  auto list = std::make_unique<BluetoothAdvertisement::UUIDList>();
  list->emplace_back(kCableAdvertisementUUID16);
  list->emplace_back(fido_parsing_utils::ConvertBytesToUuid(client_eid));
  advertisement_data->set_service_uuids(std::move(list));

#elif defined(OS_WIN)
  // References:
  // https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
  // go/google-ble-manufacturer-data-format
  static constexpr uint16_t kGoogleManufacturerId = 0x00E0;
  static constexpr uint8_t kCableGoogleManufacturerDataType = 0x15;

  // Reference:
  // https://github.com/arnar/fido-2-specs/blob/fido-client-to-authenticator-protocol.bs#L4314
  static constexpr uint8_t kCableFlags = 0x20;

  static constexpr uint8_t kCableGoogleManufacturerDataLength =
      3u + kEphemeralIdSize;
  std::array<uint8_t, 4> kCableGoogleManufacturerDataHeader = {
      kCableGoogleManufacturerDataLength, kCableGoogleManufacturerDataType,
      kCableFlags, version_number};

  auto manufacturer_data =
      std::make_unique<BluetoothAdvertisement::ManufacturerData>();
  std::vector<uint8_t> manufacturer_data_value;
  fido_parsing_utils::Append(&manufacturer_data_value,
                             kCableGoogleManufacturerDataHeader);
  fido_parsing_utils::Append(&manufacturer_data_value, client_eid);
  manufacturer_data->emplace(kGoogleManufacturerId,
                             std::move(manufacturer_data_value));
  advertisement_data->set_manufacturer_data(std::move(manufacturer_data));

#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Reference:
  // https://github.com/arnar/fido-2-specs/blob/fido-client-to-authenticator-protocol.bs#L4314
  static constexpr uint8_t kCableFlags = 0x20;

  // Service data for ChromeOS and Linux is 1 byte corresponding to Cable flags,
  // followed by 1 byte corresponding to Cable version number, followed by 16
  // bytes corresponding to client EID.
  auto service_data = std::make_unique<BluetoothAdvertisement::ServiceData>();
  std::vector<uint8_t> service_data_value(18, 0);
  // Since the remainder of this service data field is a Cable EID, set the 5th
  // bit of the flag byte.
  service_data_value[0] = kCableFlags;
  service_data_value[1] = version_number;
  std::copy(client_eid.begin(), client_eid.end(),
            service_data_value.begin() + 2);
  service_data->emplace(kCableAdvertisementUUID128,
                        std::move(service_data_value));
  advertisement_data->set_service_data(std::move(service_data));
#endif

  return advertisement_data;
}

}  // namespace

// CableDiscoveryData -------------------------------------

CableDiscoveryData::CableDiscoveryData() = default;

CableDiscoveryData::CableDiscoveryData(
    uint8_t version,
    const EidArray& client_eid,
    const EidArray& authenticator_eid,
    const SessionPreKeyArray& session_pre_key)
    : version(version),
      client_eid(client_eid),
      authenticator_eid(authenticator_eid),
      session_pre_key(session_pre_key) {}

CableDiscoveryData::CableDiscoveryData(const CableDiscoveryData& data) =
    default;

CableDiscoveryData& CableDiscoveryData::operator=(
    const CableDiscoveryData& other) = default;

CableDiscoveryData::~CableDiscoveryData() = default;

bool CableDiscoveryData::operator==(const CableDiscoveryData& other) const {
  return version == other.version && client_eid == other.client_eid &&
         authenticator_eid == other.authenticator_eid &&
         session_pre_key == other.session_pre_key;
}

// FidoCableDiscovery ---------------------------------------------------------

FidoCableDiscovery::FidoCableDiscovery(
    std::vector<CableDiscoveryData> discovery_data)
    : FidoBleDiscoveryBase(
          FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy),
      discovery_data_(std::move(discovery_data)),
      weak_factory_(this) {
// Windows currently does not support multiple EIDs, thus we ignore any extra
// discovery data.
// TODO(https://crbug.com/837088): Add support for multiple EIDs on Windows.
#if defined(OS_WIN)
  if (discovery_data_.size() > 1u)
    discovery_data_.erase(discovery_data_.begin() + 1, discovery_data_.end());
#endif
}

// This is a workaround for https://crbug.com/846522
FidoCableDiscovery::~FidoCableDiscovery() {
  for (auto advertisement : advertisements_)
    advertisement.second->Unregister(base::DoNothing(), base::DoNothing());
}

std::unique_ptr<FidoCableHandshakeHandler>
FidoCableDiscovery::CreateHandshakeHandler(
    FidoCableDevice* device,
    base::span<const uint8_t, kSessionPreKeySize> session_pre_key,
    base::span<const uint8_t, 8> nonce) {
  return std::make_unique<FidoCableHandshakeHandler>(device, nonce,
                                                     session_pre_key);
}

void FidoCableDiscovery::DeviceAdded(BluetoothAdapter* adapter,
                                     BluetoothDevice* device) {
  if (!IsCableDevice(device))
    return;

  FIDO_LOG(DEBUG) << "Discovered caBLE device: " << device->GetAddress();
  CableDeviceFound(adapter, device);
}

void FidoCableDiscovery::DeviceChanged(BluetoothAdapter* adapter,
                                       BluetoothDevice* device) {
  if (!IsCableDevice(device))
    return;

  FIDO_LOG(DEBUG) << "Device changed for caBLE device: "
                  << device->GetAddress();
  CableDeviceFound(adapter, device);
}

void FidoCableDiscovery::DeviceRemoved(BluetoothAdapter* adapter,
                                       BluetoothDevice* device) {
  if (IsCableDevice(device) && GetCableDiscoveryData(device)) {
    const auto& device_address = device->GetAddress();
    FIDO_LOG(DEBUG) << "caBLE device removed: " << device_address;
    RemoveDevice(FidoBleDevice::GetIdForAddress(device_address));
  }
}

void FidoCableDiscovery::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                               bool powered) {
  // If Bluetooth adapter is powered on, resume scanning for nearby Cable
  // devices and start advertising client EIDs.
  if (powered) {
    StartCableDiscovery();
  } else {
    // In order to prevent duplicate client EIDs from being advertised when
    // BluetoothAdapter is powered back on, unregister all existing client
    // EIDs.
    StopAdvertisements(base::DoNothing());
  }
}

void FidoCableDiscovery::OnSetPowered() {
  DCHECK(adapter());

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FidoCableDiscovery::StartCableDiscovery,
                                weak_factory_.GetWeakPtr()));
}

void FidoCableDiscovery::StartCableDiscovery() {
  // Error callback OnStartDiscoverySessionError() is defined in the base class
  // FidoBleDiscoveryBase.
  adapter()->StartDiscoverySessionWithFilter(
      std::make_unique<BluetoothDiscoveryFilter>(
          BluetoothTransport::BLUETOOTH_TRANSPORT_LE),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&FidoCableDiscovery::OnStartDiscoverySessionWithFilter,
                         weak_factory_.GetWeakPtr())),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&FidoCableDiscovery::OnStartDiscoverySessionError,
                         weak_factory_.GetWeakPtr())));
}

void FidoCableDiscovery::OnStartDiscoverySessionWithFilter(
    std::unique_ptr<BluetoothDiscoverySession> session) {
  SetDiscoverySession(std::move(session));
  FIDO_LOG(DEBUG) << "Discovery session started.";
  StartAdvertisement();
}

void FidoCableDiscovery::StartAdvertisement() {
  DCHECK(adapter());
  FIDO_LOG(DEBUG) << "Starting to advertise clientEID.";
  for (const auto& data : discovery_data_) {
    adapter()->RegisterAdvertisement(
        ConstructAdvertisementData(data.version, data.client_eid),
        base::AdaptCallbackForRepeating(
            base::BindOnce(&FidoCableDiscovery::OnAdvertisementRegistered,
                           weak_factory_.GetWeakPtr(), data.client_eid)),
        base::AdaptCallbackForRepeating(
            base::BindOnce(&FidoCableDiscovery::OnAdvertisementRegisterError,
                           weak_factory_.GetWeakPtr())));
  }
}

void FidoCableDiscovery::StopAdvertisements(base::OnceClosure callback) {
  auto barrier_closure =
      base::BarrierClosure(advertisement_success_counter_, std::move(callback));
  for (auto advertisement : advertisements_) {
    advertisement.second->Unregister(barrier_closure, base::DoNothing());
    FIDO_LOG(DEBUG) << "Stopped caBLE advertisement.";
  }

#if !defined(OS_WIN)
  // On Windows the discovery is the only owner of the advertisements, meaning
  // the advertisements would be destroyed before |barrier_closure| could be
  // invoked.
  advertisements_.clear();
#endif  // !defined(OS_WIN)
}

void FidoCableDiscovery::OnAdvertisementRegistered(
    const EidArray& client_eid,
    scoped_refptr<BluetoothAdvertisement> advertisement) {
  FIDO_LOG(DEBUG) << "Advertisement registered.";
  advertisements_.emplace(client_eid, std::move(advertisement));
  RecordAdvertisementResult(true /* is_success */);
}

void FidoCableDiscovery::OnAdvertisementRegisterError(
    BluetoothAdvertisement::ErrorCode error_code) {
  FIDO_LOG(ERROR) << "Failed to register advertisement: " << error_code;
  RecordAdvertisementResult(false /* is_success */);
}

void FidoCableDiscovery::RecordAdvertisementResult(bool is_success) {
  // If at least one advertisement succeeds, then notify discovery start.
  if (is_success) {
    if (!advertisement_success_counter_++)
      NotifyDiscoveryStarted(true);
    return;
  }

  // No advertisements succeeded, no point in continuing with Cable discovery.
  if (++advertisement_failure_counter_ == discovery_data_.size())
    NotifyDiscoveryStarted(false);
}

void FidoCableDiscovery::CableDeviceFound(BluetoothAdapter* adapter,
                                          BluetoothDevice* device) {
  const auto* found_cable_device_data = GetCableDiscoveryData(device);
  const std::string device_address = device->GetAddress();
  if (!found_cable_device_data ||
      base::Contains(active_authenticator_eids_,
                     found_cable_device_data->authenticator_eid) ||
      base::Contains(active_devices_, device_address)) {
    return;
  }

  FIDO_LOG(EVENT) << "Found new caBLE device.";
  // Nonce is embedded as first 8 bytes of client EID.
  std::array<uint8_t, 8> nonce;
  bool extract_success = fido_parsing_utils::ExtractArray(
      found_cable_device_data->client_eid, 0, &nonce);
  if (!extract_success)
    return;

  auto cable_device =
      std::make_unique<FidoCableDevice>(adapter, device->GetAddress());
  active_authenticator_eids_.insert(found_cable_device_data->authenticator_eid);
  active_devices_.insert(device_address);
  StopAdvertisements(
      base::BindOnce(&FidoCableDiscovery::ConductEncryptionHandshake,
                     weak_factory_.GetWeakPtr(), std::move(cable_device),
                     found_cable_device_data->session_pre_key, nonce));
}

void FidoCableDiscovery::ConductEncryptionHandshake(
    std::unique_ptr<FidoCableDevice> cable_device,
    base::span<const uint8_t, kSessionPreKeySize> session_pre_key,
    base::span<const uint8_t, 8> nonce) {
  auto handshake_handler =
      CreateHandshakeHandler(cable_device.get(), session_pre_key, nonce);
  auto* const handshake_handler_ptr = handshake_handler.get();
  cable_handshake_handlers_.emplace_back(std::move(handshake_handler));

  handshake_handler_ptr->InitiateCableHandshake(
      base::BindOnce(&FidoCableDiscovery::ValidateAuthenticatorHandshakeMessage,
                     weak_factory_.GetWeakPtr(), std::move(cable_device),
                     handshake_handler_ptr));
}

void FidoCableDiscovery::ValidateAuthenticatorHandshakeMessage(
    std::unique_ptr<FidoCableDevice> cable_device,
    FidoCableHandshakeHandler* handshake_handler,
    base::Optional<std::vector<uint8_t>> handshake_response) {
  if (!handshake_response)
    return;

  if (handshake_handler->ValidateAuthenticatorHandshakeMessage(
          *handshake_response)) {
    FIDO_LOG(DEBUG) << "Authenticator handshake validated";
    AddDevice(std::move(cable_device));
  } else {
    FIDO_LOG(DEBUG) << "Authenticator handshake invalid";
  }
}

const CableDiscoveryData* FidoCableDiscovery::GetCableDiscoveryData(
    const BluetoothDevice* device) const {
  const auto* discovery_data = GetCableDiscoveryDataFromServiceData(device);
  if (discovery_data != nullptr) {
    FIDO_LOG(DEBUG) << "Found caBLE service data.";
    return discovery_data;
  }

  FIDO_LOG(DEBUG)
      << "caBLE service data not found. Searching for caBLE UUIDs instead.";
  // iOS devices cannot advertise service data. These devices instead put the
  // authenticator EID as a second UUID in addition to the caBLE UUID.
  return GetCableDiscoveryDataFromServiceUUIDs(device);
}

const CableDiscoveryData*
FidoCableDiscovery::GetCableDiscoveryDataFromServiceData(
    const BluetoothDevice* device) const {
  const auto* service_data =
      device->GetServiceDataForUUID(CableAdvertisementUUID());
  if (!service_data) {
    return nullptr;
  }
  DCHECK(service_data);

  // Received service data from authenticator must have a flag that signals that
  // the service data includes Cable EID.
  if (service_data->empty() || !(service_data->at(0) >> 5 & 1u))
    return nullptr;

  EidArray received_authenticator_eid;
  bool extract_success = fido_parsing_utils::ExtractArray(
      *service_data, 2, &received_authenticator_eid);
  if (!extract_success)
    return nullptr;

  auto discovery_data_iterator = std::find_if(
      discovery_data_.begin(), discovery_data_.end(),
      [&received_authenticator_eid](const auto& data) {
        return received_authenticator_eid == data.authenticator_eid;
      });

  return discovery_data_iterator != discovery_data_.end()
             ? &(*discovery_data_iterator)
             : nullptr;
}

const CableDiscoveryData*
FidoCableDiscovery::GetCableDiscoveryDataFromServiceUUIDs(
    const BluetoothDevice* device) const {
  const auto service_uuids = device->GetUUIDs();
  for (const auto& uuid : service_uuids) {
    if (uuid == CableAdvertisementUUID())
      continue;

    auto discovery_data_iterator = std::find_if(
        discovery_data_.begin(), discovery_data_.end(),
        [&uuid](const auto& data) {
          std::string received_eid_string =
              fido_parsing_utils::ConvertBytesToUuid(data.authenticator_eid);
          return uuid == BluetoothUUID(received_eid_string);
        });
    if (discovery_data_iterator != discovery_data_.end()) {
      return &(*discovery_data_iterator);
    }
  }
  return nullptr;
}

}  // namespace device
