// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/openscreen/platform/chrome_tls_connection_factory.h"

#include <iostream>
#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "chrome/browser/media/router/providers/openscreen/platform/chrome_task_runner.h"
#include "chrome/browser/media/router/providers/openscreen/platform/chrome_tls_client_connection.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::StrictMock;

using openscreen::Error;
using openscreen::platform::TlsConnection;
using openscreen::platform::TlsConnectionFactory;

namespace media_router {

namespace {

const openscreen::IPEndpoint kValidOpenscreenEndpoint{
    openscreen::IPAddress{192, 168, 0, 1}, 80};

class MockTlsConnectionFactoryClient : public TlsConnectionFactory::Client {
 public:
  MOCK_METHOD(void,
              OnAccepted,
              (TlsConnectionFactory*,
               std::vector<uint8_t>,
               std::unique_ptr<TlsConnection>),
              (override));
  MOCK_METHOD(void,
              OnConnected,
              (TlsConnectionFactory*,
               std::vector<uint8_t>,
               std::unique_ptr<TlsConnection>),
              (override));
  MOCK_METHOD(void,
              OnConnectionFailed,
              (TlsConnectionFactory*, const openscreen::IPEndpoint&),
              (override));
  MOCK_METHOD(void, OnError, (TlsConnectionFactory*, Error), (override));
};

class FakeNetworkContext : public network::TestNetworkContext {
 public:
  void CreateTCPConnectedSocket(
      const base::Optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      CreateTCPConnectedSocketCallback callback) override {
    ++times_called_;
    callback_ = std::move(callback);
  }

  int times_called() { return times_called_; }

  void ExecuteCreateCallback(int32_t net_result) {
    std::move(callback_).Run(net_result, base::nullopt, base::nullopt,
                             mojo::ScopedDataPipeConsumerHandle{},
                             mojo::ScopedDataPipeProducerHandle{});
  }

 private:
  CreateTCPConnectedSocketCallback callback_;
  int times_called_ = 0;
};

}  // namespace

class ChromeTlsConnectionFactoryTest : public ::testing::Test {
 public:
  void SetUp() override {
    task_environment_ = std::make_unique<base::test::TaskEnvironment>();

    task_runner = std::make_unique<ChromeTaskRunner>(
        task_environment_->GetMainThreadTaskRunner());

    mock_network_context = std::make_unique<FakeNetworkContext>();
  }

  std::unique_ptr<ChromeTaskRunner> task_runner;
  std::unique_ptr<FakeNetworkContext> mock_network_context;

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};

TEST_F(ChromeTlsConnectionFactoryTest, CallsNetworkContextCreateMethod) {
  StrictMock<MockTlsConnectionFactoryClient> mock_client;
  ChromeTlsConnectionFactory factory(&mock_client, task_runner.get(),
                                     mock_network_context.get());

  factory.Connect(kValidOpenscreenEndpoint,
                  openscreen::platform::TlsConnectOptions{});

  mock_network_context->ExecuteCreateCallback(net::OK);
  EXPECT_EQ(1, mock_network_context->times_called());
}

TEST_F(ChromeTlsConnectionFactoryTest,
       CallsOnConnectionFailedWhenNetworkContextReportsError) {
  StrictMock<MockTlsConnectionFactoryClient> mock_client;
  ChromeTlsConnectionFactory factory(&mock_client, task_runner.get(),
                                     mock_network_context.get());
  EXPECT_CALL(mock_client,
              OnConnectionFailed(&factory, kValidOpenscreenEndpoint));

  factory.Connect(kValidOpenscreenEndpoint,
                  openscreen::platform::TlsConnectOptions{});

  mock_network_context->ExecuteCreateCallback(net::ERR_FAILED);
  EXPECT_EQ(1, mock_network_context->times_called());
  base::RunLoop().RunUntilIdle();
}

}  // namespace media_router
