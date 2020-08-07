// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_connections_manager_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/nearby_sharing/mock_nearby_connections.h"
#include "chrome/browser/nearby_sharing/mock_nearby_process_manager.h"
#include "chrome/browser/nearby_sharing/nearby_connection_impl.h"
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
const char kRemoteEndpointId[] = "remote_endpoint_id";
const char kEndpointInfo[] = {0x0d, 0x07, 0x07, 0x07, 0x07};
const char kRemoteEndpointInfo[] = {0x0d, 0x07, 0x06, 0x08, 0x09};
const char kAuthenticationToken[] = "authentication_token";
const char kRawAuthenticationToken[] = {0x00, 0x05, 0x04, 0x03, 0x02};
const char kBytePayload[] = {0x08, 0x09, 0x06, 0x04, 0x0f};
const char kBytePayload2[] = {0x0a, 0x0b, 0x0c, 0x0d, 0x0e};

}  // namespace

using Status = location::nearby::connections::mojom::Status;
using DiscoveredEndpointInfo =
    location::nearby::connections::mojom::DiscoveredEndpointInfo;
using ConnectionInfo = location::nearby::connections::mojom::ConnectionInfo;

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
 public:
  void SetUp() override {
    EXPECT_CALL(nearby_process_manager_,
                GetOrStartNearbyConnections(testing::Eq(&profile_)))
        .WillRepeatedly(testing::Return(&nearby_connections_));
  }

 protected:
  void StartDiscovery(
      mojo::Remote<EndpointDiscoveryListener>& listener_remote,
      testing::NiceMock<MockDiscoveryListener>& discovery_listener) {
    EXPECT_CALL(nearby_connections_, StartDiscovery)
        .WillOnce([&listener_remote](
                      const std::string& service_id,
                      DiscoveryOptionsPtr options,
                      mojo::PendingRemote<EndpointDiscoveryListener> listener,
                      NearbyConnectionsMojom::StartDiscoveryCallback callback) {
          EXPECT_EQ(kServiceId, service_id);
          EXPECT_EQ(kStrategy, options->strategy);

          listener_remote.Bind(std::move(listener));
          std::move(callback).Run(Status::kSuccess);
        });
    base::MockCallback<NearbyConnectionsManager::ConnectionsCallback> callback;
    EXPECT_CALL(callback, Run(testing::Eq(Status::kSuccess)));
    nearby_connections_manager_.StartDiscovery(&discovery_listener,
                                               callback.Get());
  }

  enum class ConnectionResponse { kAccepted, kRejceted, kDisconnected };

  NearbyConnection* Connect(
      mojo::Remote<ConnectionLifecycleListener>& listener_remote,
      ConnectionResponse connection_response) {
    const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                   std::end(kEndpointInfo));
    const std::vector<uint8_t> remote_endpoint_info(
        std::begin(kRemoteEndpointInfo), std::end(kRemoteEndpointInfo));
    const std::vector<uint8_t> raw_authentication_token(
        std::begin(kRawAuthenticationToken), std::end(kRawAuthenticationToken));

    EXPECT_CALL(nearby_connections_, RequestConnection)
        .WillOnce(
            [&](const std::vector<uint8_t>& endpoint_info,
                const std::string& endpoint_id,
                mojo::PendingRemote<ConnectionLifecycleListener> listener,
                NearbyConnectionsMojom::RequestConnectionCallback callback) {
              EXPECT_EQ(local_endpoint_info, endpoint_info);
              EXPECT_EQ(kRemoteEndpointId, endpoint_id);

              listener_remote.Bind(std::move(listener));
              std::move(callback).Run(Status::kSuccess);
            });

    base::RunLoop run_loop;
    NearbyConnection* nearby_connection;
    nearby_connections_manager_.Connect(
        local_endpoint_info, kRemoteEndpointId,
        /*bluetooth_mac_address=*/base::nullopt, DataUsage::kOffline,
        base::BindLambdaForTesting([&](NearbyConnection* connection) {
          nearby_connection = connection;
          run_loop.Quit();
        }));

    listener_remote->OnConnectionInitiated(
        kRemoteEndpointId,
        ConnectionInfo::New(kAuthenticationToken, raw_authentication_token,
                            remote_endpoint_info,
                            /*is_incoming_connection=*/false));

    switch (connection_response) {
      case ConnectionResponse::kAccepted:
        listener_remote->OnConnectionAccepted(kRemoteEndpointId);
        break;
      case ConnectionResponse::kRejceted:
        listener_remote->OnConnectionRejected(kRemoteEndpointId,
                                              Status::kConnectionRejected);
        break;
      case ConnectionResponse::kDisconnected:
        listener_remote->OnDisconnected(kRemoteEndpointId);
        break;
    }
    run_loop.Run();

    return nearby_connection;
  }

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

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(listener_remote, discovery_listener);

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
  StartDiscovery(listener_remote, discovery_listener);

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

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(listener_remote, discovery_listener);

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

TEST_F(NearbyConnectionsManagerImplTest, ConnectRejected) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> listener_remote;
  NearbyConnection* nearby_connection =
      Connect(listener_remote, ConnectionResponse::kRejceted);
  EXPECT_FALSE(nearby_connection);
  EXPECT_FALSE(
      nearby_connections_manager_.GetRawAuthenticationToken(kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectDisconnted) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> listener_remote;
  NearbyConnection* nearby_connection =
      Connect(listener_remote, ConnectionResponse::kDisconnected);
  EXPECT_FALSE(nearby_connection);
  EXPECT_FALSE(
      nearby_connections_manager_.GetRawAuthenticationToken(kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectAccepted) {
  const std::vector<uint8_t> raw_authentication_token(
      std::begin(kRawAuthenticationToken), std::end(kRawAuthenticationToken));

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> listener_remote;
  NearbyConnection* nearby_connection =
      Connect(listener_remote, ConnectionResponse::kAccepted);
  EXPECT_TRUE(nearby_connection);
  EXPECT_EQ(
      raw_authentication_token,
      nearby_connections_manager_.GetRawAuthenticationToken(kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectReadBeforeAppend) {
  const std::vector<uint8_t> byte_payload(std::begin(kBytePayload),
                                          std::end(kBytePayload));

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> listener_remote;
  NearbyConnection* nearby_connection =
      Connect(listener_remote, ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Read before message is appended should also succeed.
  base::RunLoop read_run_loop;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](base::Optional<std::vector<uint8_t>> bytes) {
        EXPECT_EQ(byte_payload, bytes);
        read_run_loop.Quit();
      }));
  // TOOD(crbug/1076008): Call from mojo instead of casting.
  static_cast<NearbyConnectionImpl*>(nearby_connection)
      ->WriteMessage(byte_payload);
  read_run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectReadAfterAppend) {
  const std::vector<uint8_t> byte_payload(std::begin(kBytePayload),
                                          std::end(kBytePayload));
  const std::vector<uint8_t> byte_payload_2(std::begin(kBytePayload2),
                                            std::end(kBytePayload2));

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> listener_remote;
  NearbyConnection* nearby_connection =
      Connect(listener_remote, ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Read after message is appended should succeed.
  // TOOD(crbug/1076008): Call from mojo instead of casting.
  static_cast<NearbyConnectionImpl*>(nearby_connection)
      ->WriteMessage(byte_payload);
  static_cast<NearbyConnectionImpl*>(nearby_connection)
      ->WriteMessage(byte_payload_2);

  base::RunLoop read_run_loop;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](base::Optional<std::vector<uint8_t>> bytes) {
        EXPECT_EQ(byte_payload, bytes);
        read_run_loop.Quit();
      }));
  read_run_loop.Run();

  base::RunLoop read_run_loop_2;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](base::Optional<std::vector<uint8_t>> bytes) {
        EXPECT_EQ(byte_payload_2, bytes);
        read_run_loop_2.Quit();
      }));
  read_run_loop_2.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectWrite) {
  const std::vector<uint8_t> byte_payload(std::begin(kBytePayload),
                                          std::end(kBytePayload));
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> listener_remote;
  NearbyConnection* nearby_connection =
      Connect(listener_remote, ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  nearby_connection->Write(byte_payload);
  // TOOD(crbug/1076008): Veriy that nearby_connections_.SendPayload is called.
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectClosed) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> listener_remote;
  NearbyConnection* nearby_connection =
      Connect(listener_remote, ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Close should invoke disconnection callback and read callback.
  base::RunLoop close_run_loop;
  nearby_connection->SetDisconnectionListener(
      base::BindLambdaForTesting([&]() { close_run_loop.Quit(); }));
  base::RunLoop read_run_loop_3;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](base::Optional<std::vector<uint8_t>> bytes) {
        EXPECT_FALSE(bytes);
        read_run_loop_3.Quit();
      }));

  EXPECT_CALL(nearby_connections_, DisconnectFromEndpoint)
      .WillOnce(
          [&](const std::string& endpoint_id,
              NearbyConnectionsMojom::DisconnectFromEndpointCallback callback) {
            EXPECT_EQ(kRemoteEndpointId, endpoint_id);
            std::move(callback).Run(Status::kSuccess);
          });
  nearby_connection->Close();
  close_run_loop.Run();
  read_run_loop_3.Run();

  EXPECT_FALSE(
      nearby_connections_manager_.GetRawAuthenticationToken(kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectClosedByRemote) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> listener_remote;
  NearbyConnection* nearby_connection =
      Connect(listener_remote, ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Remote closing should invoke disconnection callback and read callback.
  base::RunLoop close_run_loop;
  nearby_connection->SetDisconnectionListener(
      base::BindLambdaForTesting([&]() { close_run_loop.Quit(); }));
  base::RunLoop read_run_loop;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](base::Optional<std::vector<uint8_t>> bytes) {
        EXPECT_FALSE(bytes);
        read_run_loop.Quit();
      }));

  listener_remote->OnDisconnected(kRemoteEndpointId);
  close_run_loop.Run();
  read_run_loop.Run();

  EXPECT_FALSE(
      nearby_connections_manager_.GetRawAuthenticationToken(kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectClosedByClient) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> listener_remote;
  NearbyConnection* nearby_connection =
      Connect(listener_remote, ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Remote closing should invoke disconnection callback and read callback.
  base::RunLoop close_run_loop;
  nearby_connection->SetDisconnectionListener(
      base::BindLambdaForTesting([&]() { close_run_loop.Quit(); }));
  base::RunLoop read_run_loop;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](base::Optional<std::vector<uint8_t>> bytes) {
        EXPECT_FALSE(bytes);
        read_run_loop.Quit();
      }));

  EXPECT_CALL(nearby_connections_, DisconnectFromEndpoint)
      .WillOnce(
          [&](const std::string& endpoint_id,
              NearbyConnectionsMojom::DisconnectFromEndpointCallback callback) {
            EXPECT_EQ(kRemoteEndpointId, endpoint_id);
            std::move(callback).Run(Status::kSuccess);
          });
  nearby_connections_manager_.Disconnect(kRemoteEndpointId);
  close_run_loop.Run();
  read_run_loop.Run();

  EXPECT_FALSE(
      nearby_connections_manager_.GetRawAuthenticationToken(kRemoteEndpointId));
}
