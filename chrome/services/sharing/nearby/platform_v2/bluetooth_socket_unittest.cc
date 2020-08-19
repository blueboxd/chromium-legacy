// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform_v2/bluetooth_socket.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/platform_v2/bluetooth_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace location {
namespace nearby {
namespace chrome {

namespace {

const char kDeviceAddress1[] = "DeviceAddress1";
const char kDeviceName1[] = "DeviceName1";

class FakeSocket : public bluetooth::mojom::Socket {
 public:
  FakeSocket() = default;
  ~FakeSocket() override {
    EXPECT_TRUE(called_disconnect_);
    if (on_destroy_callback_)
      std::move(on_destroy_callback_).Run();
  }

  void SetOnDestroyCallback(base::OnceClosure callback) {
    on_destroy_callback_ = std::move(callback);
  }

 private:
  // bluetooth::mojom::Socket:
  void Disconnect(DisconnectCallback callback) override {
    called_disconnect_ = true;
    std::move(callback).Run();
  }

  bool called_disconnect_ = false;
  base::OnceClosure on_destroy_callback_;
};

}  // namespace

class BluetoothSocketTest : public testing::Test {
 public:
  BluetoothSocketTest() = default;
  ~BluetoothSocketTest() override = default;
  BluetoothSocketTest(const BluetoothSocketTest&) = delete;
  BluetoothSocketTest& operator=(const BluetoothSocketTest&) = delete;

  void SetUp() override {
    bluetooth_device_ = std::make_unique<chrome::BluetoothDevice>(
        CreateDeviceInfo(kDeviceAddress1, kDeviceName1));
    mojo::ScopedDataPipeProducerHandle receive_pipe_producer_handle;
    mojo::ScopedDataPipeConsumerHandle receive_pipe_consumer_handle;
    ASSERT_EQ(
        MOJO_RESULT_OK,
        mojo::CreateDataPipe(/*options=*/nullptr, &receive_pipe_producer_handle,
                             &receive_pipe_consumer_handle));

    mojo::ScopedDataPipeProducerHandle send_pipe_producer_handle;
    mojo::ScopedDataPipeConsumerHandle send_pipe_consumer_handle;
    ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(/*options=*/nullptr,
                                                   &send_pipe_producer_handle,
                                                   &send_pipe_consumer_handle));

    auto fake_socket = std::make_unique<FakeSocket>();
    fake_socket_ = fake_socket.get();

    mojo::PendingRemote<bluetooth::mojom::Socket> pending_socket;

    mojo::MakeSelfOwnedReceiver(
        std::move(fake_socket),
        pending_socket.InitWithNewPipeAndPassReceiver());

    bluetooth_socket_ = std::make_unique<BluetoothSocket>(
        *bluetooth_device_, std::move(pending_socket),
        std::move(receive_pipe_consumer_handle),
        std::move(send_pipe_producer_handle));

    receive_stream_ = std::move(receive_pipe_producer_handle);
    send_stream_ = std::move(send_pipe_consumer_handle);
  }

  void TearDown() override {
    // Destroy here, not in BluetoothSocketTest's destructor, because this is
    // blocking.
    bluetooth_socket_.reset();
  }

 protected:
  std::unique_ptr<chrome::BluetoothDevice> bluetooth_device_;
  FakeSocket* fake_socket_ = nullptr;

  std::unique_ptr<BluetoothSocket> bluetooth_socket_;

  mojo::ScopedDataPipeProducerHandle receive_stream_;
  mojo::ScopedDataPipeConsumerHandle send_stream_;

 private:
  bluetooth::mojom::DeviceInfoPtr CreateDeviceInfo(const std::string& address,
                                                   const std::string& name) {
    auto device_info = bluetooth::mojom::DeviceInfo::New();
    device_info->address = address;
    device_info->name = name;
    device_info->name_for_display = name;
    return device_info;
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(BluetoothSocketTest, TestGetRemoteDevice) {
  EXPECT_EQ(bluetooth_device_.get(), bluetooth_socket_->GetRemoteDevice());
}

TEST_F(BluetoothSocketTest, TestClose) {
  ASSERT_TRUE(fake_socket_);

  base::RunLoop run_loop;
  fake_socket_->SetOnDestroyCallback(run_loop.QuitClosure());
  EXPECT_TRUE(bluetooth_socket_->Close().Ok());
  run_loop.Run();
}

TEST_F(BluetoothSocketTest, TestDestroy) {
  ASSERT_TRUE(fake_socket_);

  base::RunLoop run_loop;
  fake_socket_->SetOnDestroyCallback(run_loop.QuitClosure());
  bluetooth_socket_.reset();
  run_loop.Run();
}

TEST_F(BluetoothSocketTest, TestInputStream) {
  InputStream& input_stream = bluetooth_socket_->GetInputStream();

  std::string message = "ReceivedMessage";
  uint32_t message_size = message.size();
  EXPECT_EQ(MOJO_RESULT_OK,
            receive_stream_->WriteData(message.data(), &message_size,
                                       MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(message.size(), message_size);

  ExceptionOr<ByteArray> exception_or_byte_array =
      input_stream.Read(message_size);
  ASSERT_TRUE(exception_or_byte_array.ok());

  ByteArray& byte_array = exception_or_byte_array.result();
  std::string received_string(byte_array);
  EXPECT_EQ(message, received_string);

  EXPECT_EQ(Exception::kSuccess, input_stream.Close().value);
}

TEST_F(BluetoothSocketTest, TestOutputStream) {
  OutputStream& output_stream = bluetooth_socket_->GetOutputStream();

  std::string message = "SentMessage";
  ByteArray byte_array(message);
  EXPECT_EQ(Exception::kSuccess, output_stream.Write(byte_array).value);

  const uint32_t max_buffer_size = 1024;
  uint32_t buffer_size = max_buffer_size;
  std::vector<char> buffer(max_buffer_size);
  EXPECT_EQ(MOJO_RESULT_OK, send_stream_->ReadData(buffer.data(), &buffer_size,
                                                   MOJO_READ_DATA_FLAG_NONE));

  std::string sent_string(buffer.data(), buffer_size);
  EXPECT_EQ(message, sent_string);

  EXPECT_EQ(Exception::kSuccess, output_stream.Flush().value);
  EXPECT_EQ(Exception::kSuccess, output_stream.Close().value);
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
