// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLUETOOTH_SERVER_SOCKET_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLUETOOTH_SERVER_SOCKET_H_

#include <memory>

#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/nearby/src/cpp/platform_v2/api/bluetooth_classic.h"

namespace location {
namespace nearby {
namespace chrome {

// Concrete BluetoothServerSocket implementation.
class BluetoothServerSocket : public api::BluetoothServerSocket {
 public:
  explicit BluetoothServerSocket(
      mojo::PendingRemote<bluetooth::mojom::ServerSocket> server_socket);
  ~BluetoothServerSocket() override;

  BluetoothServerSocket(const BluetoothServerSocket&) = delete;
  BluetoothServerSocket& operator=(const BluetoothServerSocket&) = delete;

  // api::BluetoothServerSocket:
  std::unique_ptr<api::BluetoothSocket> Accept() override;
  Exception Close() override;

 private:
  mojo::Remote<bluetooth::mojom::ServerSocket> server_socket_;
};

}  // namespace chrome
}  // namespace nearby
}  // namespace location

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLUETOOTH_SERVER_SOCKET_H_
