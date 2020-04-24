// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_transport_protocol.h"

#include "base/notreached.h"

namespace device {

const char kUsbHumanInterfaceDevice[] = "usb";
const char kNearFieldCommunication[] = "nfc";
const char kBluetoothLowEnergy[] = "ble";
const char kCloudAssistedBluetoothLowEnergy[] = "cable";
const char kInternal[] = "internal";

base::flat_set<FidoTransportProtocol> GetAllTransportProtocols() {
  return {FidoTransportProtocol::kUsbHumanInterfaceDevice,
          FidoTransportProtocol::kBluetoothLowEnergy,
          FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
          FidoTransportProtocol::kNearFieldCommunication,
          FidoTransportProtocol::kInternal};
}

base::Optional<FidoTransportProtocol> ConvertToFidoTransportProtocol(
    base::StringPiece protocol) {
  if (protocol == kUsbHumanInterfaceDevice)
    return FidoTransportProtocol::kUsbHumanInterfaceDevice;
  else if (protocol == kNearFieldCommunication)
    return FidoTransportProtocol::kNearFieldCommunication;
  else if (protocol == kBluetoothLowEnergy)
    return FidoTransportProtocol::kBluetoothLowEnergy;
  else if (protocol == kCloudAssistedBluetoothLowEnergy)
    return FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy;
  else if (protocol == kInternal)
    return FidoTransportProtocol::kInternal;
  else
    return base::nullopt;
}

COMPONENT_EXPORT(DEVICE_FIDO)
std::string ToString(FidoTransportProtocol protocol) {
  switch (protocol) {
    case FidoTransportProtocol::kUsbHumanInterfaceDevice:
      return kUsbHumanInterfaceDevice;
    case FidoTransportProtocol::kNearFieldCommunication:
      return kNearFieldCommunication;
    case FidoTransportProtocol::kBluetoothLowEnergy:
      return kBluetoothLowEnergy;
    case FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy:
      return kCloudAssistedBluetoothLowEnergy;
    case FidoTransportProtocol::kInternal:
      return kInternal;
  }
  NOTREACHED();
  return "";
}

}  // namespace device
