// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/device_service.h"

#include "base/no_destructor.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "content/public/browser/system_connector.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/constants.mojom.h"

namespace content {

device::mojom::DeviceService& GetDeviceService() {
  static base::NoDestructor<base::SequenceLocalStorageSlot<
      mojo::Remote<device::mojom::DeviceService>>>
      remote_slot;
  mojo::Remote<device::mojom::DeviceService>& remote =
      remote_slot->GetOrCreateValue();
  if (!remote) {
    auto receiver = remote.BindNewPipeAndPassReceiver();

    // TODO(https://crbug.com/977637): Start the service directly inside this
    // implementation once all clients are moved off of Service Manager APIs.
    // Note that in some test environments |GetSystemConnector()| may return
    // null. The Device Service is not expected to function in this tests.
    auto* connector = GetSystemConnector();
    if (connector)
      connector->Connect(device::mojom::kServiceName, std::move(receiver));
  }
  return *remote.get();
}

}  // namespace content
