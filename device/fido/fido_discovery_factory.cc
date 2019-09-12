// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_discovery_factory.h"

#include "base/logging.h"
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

#if defined(OS_MACOSX)
#include "device/fido/mac/discovery.h"
#endif  // defined(OSMACOSX)

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

FidoDiscoveryFactory::FidoDiscoveryFactory() = default;
FidoDiscoveryFactory::~FidoDiscoveryFactory() = default;

std::unique_ptr<FidoDiscoveryBase> FidoDiscoveryFactory::Create(
    FidoTransportProtocol transport,
    service_manager::Connector* connector) {
  switch (transport) {
    case FidoTransportProtocol::kUsbHumanInterfaceDevice:
      return CreateUsbFidoDiscovery(connector);
    case FidoTransportProtocol::kBluetoothLowEnergy:
      return std::make_unique<FidoBleDiscovery>();
    case FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy:
      if (cable_data_.has_value() || qr_generator_key_.has_value()) {
        return std::make_unique<FidoCableDiscovery>(
            cable_data_.value_or(std::vector<CableDiscoveryData>()),
            qr_generator_key_);
      }
      return nullptr;
    case FidoTransportProtocol::kNearFieldCommunication:
      // TODO(https://crbug.com/825949): Add NFC support.
      return nullptr;
    case FidoTransportProtocol::kInternal:
#if defined(OS_MACOSX)
      return mac_touch_id_config_
                 ? std::make_unique<fido::mac::FidoTouchIdDiscovery>(
                       *mac_touch_id_config_)
                 : nullptr;
#else
      return nullptr;
#endif  // defined(OS_MACOSX)
  }
  NOTREACHED() << "Unhandled transport type";
  return nullptr;
}

void FidoDiscoveryFactory::set_cable_data(
    std::vector<CableDiscoveryData> cable_data,
    base::Optional<QRGeneratorKey> qr_generator_key) {
  cable_data_ = std::move(cable_data);
  qr_generator_key_ = std::move(qr_generator_key);
}

#if defined(OS_WIN)
void FidoDiscoveryFactory::set_win_webauthn_api(WinWebAuthnApi* api) {
  win_webauthn_api_ = api;
}

WinWebAuthnApi* FidoDiscoveryFactory::win_webauthn_api() const {
  return win_webauthn_api_;
}

std::unique_ptr<FidoDiscoveryBase>
FidoDiscoveryFactory::MaybeCreateWinWebAuthnApiDiscovery() {
  // TODO(martinkr): Inject the window from which the request originated.
  // Windows uses this parameter to center the dialog over the parent. The
  // dialog should be centered over the originating Chrome Window; the
  // foreground window may have changed to something else since the request
  // was issued.
  return win_webauthn_api_ && win_webauthn_api_->IsAvailable()
             ? std::make_unique<WinWebAuthnApiAuthenticatorDiscovery>(
                   GetForegroundWindow(), win_webauthn_api_)
             : nullptr;
}
#endif  // defined(OS_WIN)

}  // namespace device
