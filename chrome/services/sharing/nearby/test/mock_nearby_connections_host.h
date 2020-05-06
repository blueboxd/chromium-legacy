// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_TEST_MOCK_NEARBY_CONNECTIONS_HOST_H_
#define CHROME_SERVICES_SHARING_NEARBY_TEST_MOCK_NEARBY_CONNECTIONS_HOST_H_

#include "chrome/services/sharing/public/mojom/nearby_connections.mojom.h"

#include "mojo/public/cpp/bindings/receiver.h"

namespace location {
namespace nearby {
namespace connections {

// Mock NearbyConnectionsHost interface used in tests.
class MockNearbyConnectionsHost : public mojom::NearbyConnectionsHost {
 public:
  MockNearbyConnectionsHost();
  MockNearbyConnectionsHost(const MockNearbyConnectionsHost&) = delete;
  MockNearbyConnectionsHost& operator=(const MockNearbyConnectionsHost&) =
      delete;
  ~MockNearbyConnectionsHost() override;

  mojo::Receiver<mojom::NearbyConnectionsHost> host{this};
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CHROME_SERVICES_SHARING_NEARBY_TEST_MOCK_NEARBY_CONNECTIONS_HOST_H_
