// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_H_
#define CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/sharing/public/mojom/nearby_connections.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace location {
namespace nearby {
namespace connections {

// Implementation of the NearbyConnections mojo interface.
// This class acts as a bridge to the NearbyConnections library which is pulled
// in as a third_party dependency. It handles the translation from mojo calls to
// native callbacks and types that the library expects. This class runs in a
// sandboxed process and is called from the browser process. The passed |host|
// interface is implemented in the browser process and is used to fetch runtime
// dependencies to other mojo interfaces like Bluetooth or WiFi LAN.
class NearbyConnections : public mojom::NearbyConnections {
 public:
  // Creates a new instance of the NearbyConnections library. This will allocate
  // and initialize a new instance and hold on to the passed mojo pipes.
  // |on_disconnect| is called when either mojo interface disconnects and should
  // destroy this instamce.
  NearbyConnections(
      mojo::PendingReceiver<mojom::NearbyConnections> nearby_connections,
      mojo::PendingRemote<mojom::NearbyConnectionsHost> host,
      base::OnceClosure on_disconnect);
  NearbyConnections(const NearbyConnections&) = delete;
  NearbyConnections& operator=(const NearbyConnections&) = delete;
  ~NearbyConnections() override;

 private:
  void OnDisconnect();

  mojo::Receiver<mojom::NearbyConnections> nearby_connections_;
  mojo::Remote<mojom::NearbyConnectionsHost> host_;
  base::OnceClosure on_disconnect_;

  base::WeakPtrFactory<NearbyConnections> weak_ptr_factory_{this};
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_H_
