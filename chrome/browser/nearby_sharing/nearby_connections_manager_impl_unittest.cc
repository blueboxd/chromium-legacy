// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_connections_manager_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/nearby_sharing/mock_nearby_connections.h"
#include "chrome/browser/nearby_sharing/mock_nearby_process_manager.h"
#include "chrome/services/sharing/public/mojom/nearby_connections_types.mojom.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kServiceId[] = "NearbySharing";
const location::nearby::connections::mojom::Strategy kStrategy =
    location::nearby::connections::mojom::Strategy::kP2pPointToPoint;
const char kEndpointId[] = "endpoint_id";
const char kEndpointInfo[] = {0x0d, 0x07, 0x07, 0x07, 0x07};

}  // namespace

using Status = location::nearby::connections::mojom::Status;
using DiscoveredEndpointInfo =
    location::nearby::connections::mojom::DiscoveredEndpointInfo;

class MockDiscoveryListener
    : public NearbyConnectionsManager::DiscoveryListener {
 public:
  MOCK_METHOD(void,
              OnEndpointDiscovered,
              (const std::string& endpoint_id,
               const std::vector<uint8_t>& endpoint_info),
              (override));
  MOCK_METHOD(void,
              OnEndpointLost,
              (const std::string& endpoint_id),
              (override));
};

class NearbyConnectionsManagerImplTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  testing::NiceMock<MockNearbyConnections> nearby_connections_;
  testing::NiceMock<MockNearbyProcessManager> nearby_process_manager_;
  NearbyConnectionsManagerImpl nearby_connections_manager_{
      &nearby_process_manager_, &profile_};
};

TEST_F(NearbyConnectionsManagerImplTest, DiscoveryFlow) {
  const std::vector<uint8_t> endpoint_info(std::begin(kEndpointInfo),
                                           std::end(kEndpointInfo));

  EXPECT_CALL(nearby_process_manager_,
              GetOrStartNearbyConnections(testing::Eq(&profile_)))
      .WillRepeatedly(testing::Return(&nearby_connections_));

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> listener_remote;
  EXPECT_CALL(nearby_connections_, StartDiscovery)
      .WillOnce([&listener_remote](
                    const std::string& service_id, DiscoveryOptionsPtr options,
                    mojo::PendingRemote<EndpointDiscoveryListener> listener,
                    NearbyConnectionsMojom::StartDiscoveryCallback callback) {
        EXPECT_EQ(kServiceId, service_id);
        EXPECT_EQ(kStrategy, options->strategy);

        listener_remote.Bind(std::move(listener));
        std::move(callback).Run(Status::kSuccess);
      });
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  base::MockCallback<NearbyConnectionsManager::ConnectionsCallback> callback;
  EXPECT_CALL(callback, Run(testing::Eq(Status::kSuccess)));
  nearby_connections_manager_.StartDiscovery(&discovery_listener,
                                             callback.Get());

  // Invoking OnEndpointFound over remote will invoke OnEndpointDiscovered.
  base::RunLoop discovered_run_loop;
  EXPECT_CALL(discovery_listener,
              OnEndpointDiscovered(testing::Eq(kEndpointId),
                                   testing::Eq(endpoint_info)))
      .WillOnce([&discovered_run_loop]() { discovered_run_loop.Quit(); });
  listener_remote->OnEndpointFound(
      kEndpointId, DiscoveredEndpointInfo::New(endpoint_info, kServiceId));
  discovered_run_loop.Run();

  // Invoking OnEndpointFound over remote on same endpointId will do nothing.
  EXPECT_CALL(discovery_listener, OnEndpointDiscovered(testing::_, testing::_))
      .Times(0);
  listener_remote->OnEndpointFound(
      kEndpointId, DiscoveredEndpointInfo::New(endpoint_info, kServiceId));

  // Invoking OnEndpointLost over remote will invoke OnEndpointLost.
  base::RunLoop lost_run_loop;
  EXPECT_CALL(discovery_listener, OnEndpointLost(testing::Eq(kEndpointId)))
      .WillOnce([&lost_run_loop]() { lost_run_loop.Quit(); });
  listener_remote->OnEndpointLost(kEndpointId);
  lost_run_loop.Run();

  // Invoking OnEndpointLost over remote on same endpointId will do nothing.
  EXPECT_CALL(discovery_listener, OnEndpointLost(testing::_)).Times(0);
  listener_remote->OnEndpointLost(kEndpointId);

  // After OnEndpointLost the same endpotinId can be discovered again.
  base::RunLoop discovered_run_loop_2;
  EXPECT_CALL(discovery_listener,
              OnEndpointDiscovered(testing::Eq(kEndpointId),
                                   testing::Eq(endpoint_info)))
      .WillOnce([&discovered_run_loop_2]() { discovered_run_loop_2.Quit(); });
  listener_remote->OnEndpointFound(
      kEndpointId, DiscoveredEndpointInfo::New(endpoint_info, kServiceId));
  discovered_run_loop_2.Run();

  // Stop discvoery will call through mojo.
  EXPECT_CALL(nearby_connections_, StopDiscovery).Times(1);
  nearby_connections_manager_.StopDiscovery();

  // StartDiscovery again will succeed.
  listener_remote.reset();
  EXPECT_CALL(nearby_connections_, StartDiscovery)
      .WillOnce([&listener_remote](
                    const std::string& service_id, DiscoveryOptionsPtr options,
                    mojo::PendingRemote<EndpointDiscoveryListener> listener,
                    NearbyConnectionsMojom::StartDiscoveryCallback callback) {
        listener_remote.Bind(std::move(listener));
        std::move(callback).Run(Status::kSuccess);
      });
  EXPECT_CALL(callback, Run(testing::Eq(Status::kSuccess)));
  nearby_connections_manager_.StartDiscovery(&discovery_listener,
                                             callback.Get());

  // Same endpotinId can be discovered again.
  base::RunLoop discovered_run_loop_3;
  EXPECT_CALL(discovery_listener,
              OnEndpointDiscovered(testing::Eq(kEndpointId),
                                   testing::Eq(endpoint_info)))
      .WillOnce([&discovered_run_loop_3]() { discovered_run_loop_3.Quit(); });
  listener_remote->OnEndpointFound(
      kEndpointId, DiscoveredEndpointInfo::New(endpoint_info, kServiceId));
  discovered_run_loop_3.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, DiscoveryProcessStopped) {
  const std::vector<uint8_t> endpoint_info(std::begin(kEndpointInfo),
                                           std::end(kEndpointInfo));

  EXPECT_CALL(nearby_process_manager_,
              GetOrStartNearbyConnections(testing::Eq(&profile_)))
      .WillRepeatedly(testing::Return(&nearby_connections_));

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> listener_remote;
  EXPECT_CALL(nearby_connections_, StartDiscovery)
      .WillOnce([&listener_remote](
                    const std::string& service_id, DiscoveryOptionsPtr options,
                    mojo::PendingRemote<EndpointDiscoveryListener> listener,
                    NearbyConnectionsMojom::StartDiscoveryCallback callback) {
        EXPECT_EQ(kServiceId, service_id);
        EXPECT_EQ(kStrategy, options->strategy);

        listener_remote.Bind(std::move(listener));
        std::move(callback).Run(Status::kSuccess);
      });
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  base::MockCallback<NearbyConnectionsManager::ConnectionsCallback> callback;
  EXPECT_CALL(callback, Run(testing::Eq(Status::kSuccess)));
  nearby_connections_manager_.StartDiscovery(&discovery_listener,
                                             callback.Get());

  nearby_connections_manager_.OnNearbyProcessStopped();

  // Invoking OnEndpointFound will do nothing.
  EXPECT_CALL(discovery_listener, OnEndpointDiscovered(testing::_, testing::_))
      .Times(0);
  listener_remote->OnEndpointFound(
      kEndpointId, DiscoveredEndpointInfo::New(endpoint_info, kServiceId));
}

TEST_F(NearbyConnectionsManagerImplTest, StopDiscoveryBeforeStart) {
  EXPECT_CALL(nearby_connections_, StopDiscovery).Times(0);
  nearby_connections_manager_.StopDiscovery();
}
