// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_
#define DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_device_discovery.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"

#if defined(OS_MACOSX)
#include "device/fido/mac/authenticator_config.h"
#endif  // defined(OS_MACOSX)

namespace service_manager {
class Connector;
}

namespace device {

// FidoDiscoveryFactory offers methods to construct instances of
// FidoDiscoveryBase for a given |transport| protocol.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoDiscoveryFactory {
 public:
  FidoDiscoveryFactory();
  virtual ~FidoDiscoveryFactory();

  // Instantiates a FidoDiscoveryBase for all protocols except caBLE and
  // internal/platform.
  //
  // FidoTransportProtocol::kUsbHumanInterfaceDevice requires specifying a
  // valid |connector| on Desktop, and is not valid on Android.
  virtual std::unique_ptr<FidoDiscoveryBase> Create(
      FidoTransportProtocol transport,
      ::service_manager::Connector* connector);
  // Instantiates a FidoDiscovery for caBLE.
  virtual std::unique_ptr<FidoDiscoveryBase> CreateCable(
      std::vector<CableDiscoveryData> cable_data);

#if defined(OS_MACOSX)
  // Configures the Touch ID authenticator. Set to base::nullopt to disable it.
  void set_mac_touch_id_info(
      base::Optional<fido::mac::AuthenticatorConfig> mac_touch_id_config) {
    mac_touch_id_config_ = std::move(mac_touch_id_config);
  }
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
  // Instantiates a FidoDiscovery for the native Windows WebAuthn
  // API where available. Returns nullptr otherwise.
  std::unique_ptr<FidoDiscoveryBase> MaybeCreateWinWebAuthnApiDiscovery();
#endif  // defined(OS_WIN)

 private:
#if defined(OS_MACOSX)
  base::Optional<fido::mac::AuthenticatorConfig> mac_touch_id_config_;
#endif  // defined(OS_MACOSX)
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_
