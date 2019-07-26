// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_
#define DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "build/build_config.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_device_discovery.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_transport_protocol.h"

namespace service_manager {
class Connector;
}

namespace device {

// FidoDiscoveryFactory offers methods to construct instances of
// FidoDiscoveryBase for a given |transport| protocol.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoDiscoveryFactory {
 public:
  FidoDiscoveryFactory() = default;
  virtual ~FidoDiscoveryFactory() = default;

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
#if defined(OS_WIN)
  // Instantiates a FidoDiscovery for the native Windows WebAuthn
  // API where available. Returns nullptr otherwise.
  std::unique_ptr<FidoDiscoveryBase> MaybeCreateWinWebAuthnApiDiscovery();
#endif  // defined(OS_WIN)

 private:
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_
