// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/openscreen/platform/chrome_tls_client_connection.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "chrome/browser/media/router/providers/openscreen/platform/chrome_task_runner.h"
#include "chrome/browser/media/router/providers/openscreen/platform/network_data_pump.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::StrictMock;

namespace media_router {

using openscreen::Error;
using openscreen::platform::TlsConnection;

namespace {
const openscreen::IPEndpoint kValidEndpointOne{
    openscreen::IPAddress{192, 168, 0, 1}, 80};
const openscreen::IPEndpoint kValidEndpointTwo{
    openscreen::IPAddress{10, 9, 8, 7}, 81};

class MockDataPump : public NetworkDataPump {
 public:
  MOCK_METHOD(void,
              Read,
              (scoped_refptr<net::IOBuffer> io_buffer,
               int io_buffer_size,
               net::CompletionOnceCallback callback),
              (override));
  MOCK_METHOD(void,
              Write,
              (scoped_refptr<net::IOBuffer> io_buffer,
               int io_buffer_size,
               net::CompletionOnceCallback callback),
              (override));

  MOCK_METHOD(bool, HasPendingRead, (), (const, override));
  MOCK_METHOD(bool, HasPendingWrite, (), (const, override));
};

class SimpleDataPump : public NetworkDataPump {
 public:
  SimpleDataPump() = default;
  ~SimpleDataPump() final = default;

  void Read(scoped_refptr<net::IOBuffer> io_buffer,
            int io_buffer_size,
            net::CompletionOnceCallback callback) override {
    read_callback_ = std::move(callback);
  }

  void Write(scoped_refptr<net::IOBuffer> io_buffer,
             int io_buffer_size,
             net::CompletionOnceCallback callback) override {
    write_callback_ = std::move(callback);
  }

  bool HasPendingRead() const override { return !!read_callback_; }
  bool HasPendingWrite() const override { return !!write_callback_; }

  void ExecuteReadCallback(int return_code) {
    EXPECT_FALSE(read_callback_.is_null());
    std::move(read_callback_).Run(return_code);
  }

  void ExecuteWriteCallback(int return_code) {
    EXPECT_FALSE(write_callback_.is_null());
    std::move(write_callback_).Run(return_code);
  }

 private:
  net::CompletionOnceCallback read_callback_;
  net::CompletionOnceCallback write_callback_;
};

class MockTlsConnectionClient : public TlsConnection::Client {
 public:
  MOCK_METHOD(void, OnWriteBlocked, (TlsConnection*), (override));
  MOCK_METHOD(void, OnWriteUnblocked, (TlsConnection*), (override));
  MOCK_METHOD(void, OnError, (TlsConnection*, Error), (override));
  MOCK_METHOD(void, OnRead, (TlsConnection*, std::vector<uint8_t>), (override));
};

}  // namespace

class ChromeTlsClientConnectionTest : public ::testing::Test {
 public:
  void SetUp() override {
    task_environment_ = std::make_unique<base::test::TaskEnvironment>();

    task_runner = std::make_unique<ChromeTaskRunner>(
        task_environment_->GetMainThreadTaskRunner());
  }

  std::unique_ptr<ChromeTaskRunner> task_runner;

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};

TEST_F(ChromeTlsClientConnectionTest, InitializesReadingUponConstruction) {
  auto* mock_pump = new StrictMock<MockDataPump>();
  EXPECT_CALL(*mock_pump, Read(_, _, _)).Times(1);

  ChromeTlsClientConnection connection(
      task_runner.get(), kValidEndpointOne, kValidEndpointTwo,
      std::unique_ptr<NetworkDataPump>(mock_pump),
      mojo::Remote<network::mojom::TCPConnectedSocket>{},
      mojo::Remote<network::mojom::TLSClientSocket>{});
  base::RunLoop().RunUntilIdle();
}

TEST_F(ChromeTlsClientConnectionTest, CallsClientOnReadWhenReadSuccessful) {
  auto pump = std::make_unique<SimpleDataPump>();
  // Transferring unique_ptr ownership to the ChromeTlsClientConnection
  // shouldn't invalidate our pointer. If the compiler or spec changes to do
  // so, this should fail these tests with an invalid memory access error.
  auto* raw_pump = pump.get();
  ChromeTlsClientConnection connection(
      task_runner.get(), kValidEndpointOne, kValidEndpointTwo, std::move(pump),
      mojo::Remote<network::mojom::TCPConnectedSocket>{},
      mojo::Remote<network::mojom::TLSClientSocket>{});

  StrictMock<MockTlsConnectionClient> client;
  EXPECT_CALL(client, OnRead(&connection, _)).Times(1);

  connection.SetClient(&client);
  raw_pump->ExecuteReadCallback(kMaxTlsPacketBufferSize);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ChromeTlsClientConnectionTest, QueuesAnotherReadWhenReadSuccessful) {
  auto pump = std::make_unique<SimpleDataPump>();
  auto* raw_pump = pump.get();
  ChromeTlsClientConnection connection(
      task_runner.get(), kValidEndpointOne, kValidEndpointTwo, std::move(pump),
      mojo::Remote<network::mojom::TCPConnectedSocket>{},
      mojo::Remote<network::mojom::TLSClientSocket>{});

  StrictMock<MockTlsConnectionClient> client;
  EXPECT_CALL(client, OnRead(&connection, _)).Times(2);

  connection.SetClient(&client);
  raw_pump->ExecuteReadCallback(kMaxTlsPacketBufferSize);
  base::RunLoop().RunUntilIdle();

  // This call will only succeed if the TlsConnection has placed another
  // callback in the simple pump, otherwise we will error out here.
  raw_pump->ExecuteReadCallback(kMaxTlsPacketBufferSize);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ChromeTlsClientConnectionTest, CallsClientOnErrorWhenReadFails) {
  auto pump = std::make_unique<SimpleDataPump>();
  auto* raw_pump = pump.get();
  ChromeTlsClientConnection connection(
      task_runner.get(), kValidEndpointOne, kValidEndpointTwo, std::move(pump),
      mojo::Remote<network::mojom::TCPConnectedSocket>{},
      mojo::Remote<network::mojom::TLSClientSocket>{});

  StrictMock<MockTlsConnectionClient> client;
  EXPECT_CALL(client, OnError(&connection, _)).Times(1);
  connection.SetClient(&client);

  raw_pump->ExecuteReadCallback(net::ERR_FAILED);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ChromeTlsClientConnectionTest, CallsClientOnErrorWhenWriteFails) {
  auto pump = std::make_unique<SimpleDataPump>();
  auto* raw_pump = pump.get();
  ChromeTlsClientConnection connection(
      task_runner.get(), kValidEndpointOne, kValidEndpointTwo, std::move(pump),
      mojo::Remote<network::mojom::TCPConnectedSocket>{},
      mojo::Remote<network::mojom::TLSClientSocket>{});

  StrictMock<MockTlsConnectionClient> client;
  EXPECT_CALL(client, OnError(&connection, _)).Times(1);
  EXPECT_CALL(client, OnWriteBlocked(&connection)).Times(1);
  EXPECT_CALL(client, OnWriteUnblocked(&connection)).Times(1);
  connection.SetClient(&client);

  constexpr uint8_t kWriteData[] = {50, 51, 52, 53, 54, 55, '\0'};
  constexpr size_t kWriteDataSize = 7;
  connection.Write(kWriteData, kWriteDataSize);
  raw_pump->ExecuteWriteCallback(net::ERR_FAILED);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ChromeTlsClientConnectionTest, TogglesClientWriteBlockedWhenWriting) {
  auto pump = std::make_unique<SimpleDataPump>();
  auto* raw_pump = pump.get();
  ChromeTlsClientConnection connection(
      task_runner.get(), kValidEndpointOne, kValidEndpointTwo, std::move(pump),
      mojo::Remote<network::mojom::TCPConnectedSocket>{},
      mojo::Remote<network::mojom::TLSClientSocket>{});

  StrictMock<MockTlsConnectionClient> client;
  EXPECT_CALL(client, OnWriteBlocked(&connection)).Times(1);
  EXPECT_CALL(client, OnWriteUnblocked(&connection)).Times(1);
  connection.SetClient(&client);

  constexpr uint8_t kWriteData[] = {66, 67, 68, 69, 70, 71, '\0'};
  constexpr size_t kWriteDataSize = 7;
  connection.Write(kWriteData, kWriteDataSize);
  raw_pump->ExecuteWriteCallback(kWriteDataSize);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ChromeTlsClientConnectionTest, CanRetrieveAddresses) {
  // We won't actually use the pump for this test, just need to supply one.
  auto pump = std::unique_ptr<NetworkDataPump>(new NiceMock<MockDataPump>());
  ChromeTlsClientConnection connection(
      task_runner.get(), kValidEndpointOne, kValidEndpointTwo, std::move(pump),
      mojo::Remote<network::mojom::TCPConnectedSocket>{},
      mojo::Remote<network::mojom::TLSClientSocket>{});

  EXPECT_EQ(kValidEndpointOne, connection.GetLocalEndpoint());
  EXPECT_EQ(kValidEndpointTwo, connection.GetRemoteEndpoint());
}

}  // namespace media_router
