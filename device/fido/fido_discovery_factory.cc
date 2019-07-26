// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_discovery_factory.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "device/fido/ble/fido_ble_discovery.h"
#include "device/fido/cable/fido_cable_discovery.h"
#include "device/fido/features.h"
#include "device/fido/fido_discovery_base.h"

// HID is not supported on Android.
#if !defined(OS_ANDROID)
#include "device/fido/hid/fido_hid_discovery.h"
#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN)
#include <Winuser.h>
#include "device/fido/win/discovery.h"
#include "device/fido/win/webauthn_api.h"
#endif  // defined(OS_WIN)

namespace device {

namespace {

std::unique_ptr<FidoDiscoveryBase> CreateUsbFidoDiscovery(
    service_manager::Connector* connector) {
#if defined(OS_ANDROID)
  NOTREACHED() << "USB HID not supported on Android.";
  return nullptr;
#else

  DCHECK(connector);
  return std::make_unique<FidoHidDiscovery>(connector);
#endif  // !defined(OS_ANDROID)
}

}  // namespace

std::unique_ptr<FidoDiscoveryBase> FidoDiscoveryFactory::Create(
    FidoTransportProtocol transport,
    service_manager::Connector* connector) {
  switch (transport) {
    case FidoTransportProtocol::kUsbHumanInterfaceDevice:
      return CreateUsbFidoDiscovery(connector);
    case FidoTransportProtocol::kBluetoothLowEnergy:
      return std::make_unique<FidoBleDiscovery>();
    case FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy:
      NOTREACHED() << "Cable discovery is constructed using the dedicated "
                      "factory method.";
      return nullptr;
    case FidoTransportProtocol::kNearFieldCommunication:
      // TODO(https://crbug.com/825949): Add NFC support.
      return nullptr;
    case FidoTransportProtocol::kInternal:
      NOTREACHED() << "Internal authenticators should be handled separately.";
      return nullptr;
  }
  NOTREACHED() << "Unhandled transport type";
  return nullptr;
}

std::unique_ptr<FidoDiscoveryBase> FidoDiscoveryFactory::CreateCable(
    std::vector<CableDiscoveryData> cable_data) {
  return std::make_unique<FidoCableDiscovery>(std::move(cable_data));
}

#if defined(OS_WIN)
std::unique_ptr<FidoDiscoveryBase>
FidoDiscoveryFactory::MaybeCreateWinWebAuthnApiDiscovery() {
  if (!base::FeatureList::IsEnabled(device::kWebAuthUseNativeWinApi) ||
      !WinWebAuthnApi::GetDefault()->IsAvailable()) {
    return nullptr;
  }
  return std::make_unique<WinWebAuthnApiAuthenticatorDiscovery>(
      // TODO(martinkr): Inject the window from which the request
      // originated. Windows uses this parameter to center the
      // dialog over the parent. The dialog should be centered
      // over the originating Chrome Window; the foreground window
      // may have changed to something else since the request was
      // issued.
      GetForegroundWindow());
}
#endif  // defined(OS_WIN)

}  // namespace device
