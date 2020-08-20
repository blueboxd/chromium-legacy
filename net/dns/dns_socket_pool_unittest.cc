// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_socket_pool.h"

#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/socket_test_util.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

const IPEndPoint kEndpoint0(IPAddress(1, 2, 3, 4), 578);
const IPEndPoint kEndpoint1(IPAddress(2, 3, 4, 5), 678);

class DnsSocketPoolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    pool_ = std::make_unique<DnsSocketPool>(&socket_factory_, nameservers_,
                                            nullptr /* net_log */);
  }

  MockClientSocketFactory socket_factory_;
  std::vector<IPEndPoint> nameservers_ = {kEndpoint0, kEndpoint1};
  std::unique_ptr<DnsSocketPool> pool_;
};

TEST_F(DnsSocketPoolTest, CreateConnectedUdpSocket) {
  // Prep socket factory for a single do-nothing socket.
  StaticSocketDataProvider data_provider;
  socket_factory_.AddSocketDataProvider(&data_provider);

  std::unique_ptr<DatagramClientSocket> socket =
      pool_->CreateConnectedUdpSocket(1 /* server_index */);

  ASSERT_TRUE(socket);

  IPEndPoint peer_address;
  ASSERT_THAT(socket->GetPeerAddress(&peer_address), test::IsOk());
  EXPECT_EQ(peer_address, kEndpoint1);
}

TEST_F(DnsSocketPoolTest, CreateConnectedUdpSocket_ConnectError) {
  // Prep socket factory for a single socket with connection failure.
  MockConnect connect_data;
  connect_data.result = ERR_INSUFFICIENT_RESOURCES;
  StaticSocketDataProvider data_provider;
  data_provider.set_connect_data(connect_data);
  socket_factory_.AddSocketDataProvider(&data_provider);

  std::unique_ptr<DatagramClientSocket> socket =
      pool_->CreateConnectedUdpSocket(0 /* server_index */);

  EXPECT_FALSE(socket);
}

TEST_F(DnsSocketPoolTest, CreateTcpSocket) {
  // Prep socket factory for a single do-nothing socket.
  StaticSocketDataProvider data_provider;
  socket_factory_.AddSocketDataProvider(&data_provider);

  std::unique_ptr<StreamSocket> socket =
      pool_->CreateTcpSocket(1 /* server_index */, NetLogSource());

  EXPECT_TRUE(socket);
}

}  // namespace
}  // namespace net
