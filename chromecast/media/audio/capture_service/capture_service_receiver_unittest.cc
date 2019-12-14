// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/capture_service/capture_service_receiver.h"

#include <memory>

#include "base/big_endian.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "chromecast/media/audio/mock_audio_input_callback.h"
#include "chromecast/net/mock_stream_socket.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace chromecast {
namespace media {
namespace capture_service {
namespace {

class MockStreamSocket : public chromecast::MockStreamSocket {
 public:
  MockStreamSocket() = default;
  ~MockStreamSocket() override = default;
};

class CaptureServiceReceiverTest : public ::testing::Test {
 public:
  CaptureServiceReceiverTest()
      : receiver_(StreamType::kSoftwareEchoCancelled,
                  16000,  // sample rate
                  1,      // channels
                  160) {  // frames per buffer
    receiver_.SetTaskRunnerForTest(base::CreateSequencedTaskRunner(
        {base::ThreadPool(), base::TaskPriority::USER_BLOCKING}));
  }
  ~CaptureServiceReceiverTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  chromecast::MockAudioInputCallback audio_;
  CaptureServiceReceiver receiver_;
};

TEST_F(CaptureServiceReceiverTest, StartStop) {
  auto socket1 = std::make_unique<MockStreamSocket>();
  auto socket2 = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket1, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket1, Write(_, _, _, _)).WillOnce(Return(16));
  EXPECT_CALL(*socket1, Read(_, _, _)).WillOnce(Return(net::ERR_IO_PENDING));
  EXPECT_CALL(*socket2, Connect(_)).WillOnce(Return(net::OK));

  // Sync.
  receiver_.StartWithSocket(&audio_, std::move(socket1));
  task_environment_.RunUntilIdle();
  receiver_.Stop();

  // Async.
  receiver_.StartWithSocket(&audio_, std::move(socket2));
  receiver_.Stop();
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ConnectFailed) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::ERR_FAILED));
  EXPECT_CALL(audio_, OnError());

  receiver_.StartWithSocket(&audio_, std::move(socket));
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ConnectTimeout) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::ERR_IO_PENDING));
  EXPECT_CALL(audio_, OnError());

  receiver_.StartWithSocket(&audio_, std::move(socket));
  task_environment_.FastForwardBy(CaptureServiceReceiver::kConnectTimeout);
}

TEST_F(CaptureServiceReceiverTest, SendRequest) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Write(_, _, _, _))
      .WillOnce(Invoke([](net::IOBuffer* buf, int buf_len,
                          net::CompletionOnceCallback,
                          const net::NetworkTrafficAnnotationTag&) {
        base::BigEndianReader data_reader(buf->data(), buf_len);
        uint16_t value_u16;
        EXPECT_TRUE(data_reader.ReadU16(&value_u16));
        EXPECT_EQ(value_u16, 14U);  // header - data[0].
        uint8_t value_u8;
        EXPECT_TRUE(data_reader.ReadU8(&value_u8));
        EXPECT_EQ(value_u8, 0U);  // No audio.
        EXPECT_TRUE(data_reader.ReadU8(&value_u8));
        EXPECT_EQ(value_u8, 1U);  // kSoftwareEchoCancelled.
        EXPECT_TRUE(data_reader.ReadU8(&value_u8));
        EXPECT_EQ(value_u8, 1U);  // Mono channel.
        EXPECT_TRUE(data_reader.ReadU8(&value_u8));
        EXPECT_EQ(value_u8, 5U);  // Planar float.
        EXPECT_TRUE(data_reader.ReadU16(&value_u16));
        EXPECT_EQ(value_u16, 16000U);  // 16kHz.
        uint64_t value_u64;
        EXPECT_TRUE(data_reader.ReadU64(&value_u64));
        EXPECT_EQ(value_u64, 160U);  // Frames per buffer.
        EXPECT_EQ(data_reader.remaining(), 0U);
        return 16;  // Size of header.
      }));
  EXPECT_CALL(*socket, Read(_, _, _)).WillOnce(Return(net::ERR_IO_PENDING));

  receiver_.StartWithSocket(&audio_, std::move(socket));
  task_environment_.RunUntilIdle();
  // Stop receiver to disconnect socket, since receiver doesn't own the IO
  // task runner in unittests.
  receiver_.Stop();
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ReceiveValidMessage) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Write(_, _, _, _)).WillOnce(Return(16));
  EXPECT_CALL(*socket, Read(_, _, _))
      .WillOnce(Invoke([](net::IOBuffer* buf, int,
                          net::CompletionOnceCallback) {
        std::vector<char> header(16);
        base::BigEndianWriter data_writer(header.data(), header.size());
        data_writer.WriteU16(334);  // 160 frames + header - data[0], in bytes.
        data_writer.WriteU8(1);     // Has audio.
        data_writer.WriteU8(1);     // kSoftwareEchoCancelled.
        data_writer.WriteU8(1);     // Mono channel.
        data_writer.WriteU8(0);     // Interleaved int16.
        data_writer.WriteU16(16000);  // 16kHz.
        data_writer.WriteU64(0);      // Timestamp.
        std::copy(header.data(), header.data() + header.size(), buf->data());
        // No need to fill audio frames.
        return 336;  // 160 frames + header, in bytes.
      }))
      .WillOnce(Return(net::ERR_IO_PENDING));
  EXPECT_CALL(audio_, OnData(_, _, 1.0 /* volume */));

  receiver_.StartWithSocket(&audio_, std::move(socket));
  task_environment_.RunUntilIdle();
  // Stop receiver to disconnect socket, since receiver doesn't own the IO
  // task runner in unittests.
  receiver_.Stop();
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ReceiveEmptyMessage) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Write(_, _, _, _)).WillOnce(Return(16));
  EXPECT_CALL(*socket, Read(_, _, _))
      .WillOnce(Invoke([](net::IOBuffer* buf, int,
                          net::CompletionOnceCallback) {
        std::vector<char> header(16, 0);
        base::BigEndianWriter data_writer(header.data(), header.size());
        data_writer.WriteU16(14);     // header - data[0], in bytes.
        data_writer.WriteU8(0);       // No audio.
        data_writer.WriteU8(1);       // kSoftwareEchoCancelled.
        data_writer.WriteU8(1);       // Mono channel.
        data_writer.WriteU8(0);       // Interleaved int16.
        data_writer.WriteU16(16000);  // 16kHz.
        data_writer.WriteU64(160);    // Frames per buffer.
        std::copy(header.data(), header.data() + header.size(), buf->data());
        return 16;
      }))
      .WillOnce(Return(net::ERR_IO_PENDING));
  // Neither OnError nor OnData will be called.
  EXPECT_CALL(audio_, OnError()).Times(0);
  EXPECT_CALL(audio_, OnData(_, _, _)).Times(0);

  receiver_.StartWithSocket(&audio_, std::move(socket));
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ReceiveInvalidMessage) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Write(_, _, _, _)).WillOnce(Return(16));
  EXPECT_CALL(*socket, Read(_, _, _))
      .WillOnce(Invoke([](net::IOBuffer* buf, int,
                          net::CompletionOnceCallback) {
        std::vector<char> header(16, 0);
        base::BigEndianWriter data_writer(header.data(), header.size());
        data_writer.WriteU16(334);  // 160 frames + header - data[0], in bytes.
        data_writer.WriteU8(1);     // Has audio.
        data_writer.WriteU8(1);     // kSoftwareEchoCancelled.
        data_writer.WriteU8(1);     // Mono channels.
        data_writer.WriteU8(6);     // Invalid format.
        data_writer.WriteU16(16000);  // 16kHz.
        data_writer.WriteU64(0);      // Timestamp.
        std::copy(header.data(), header.data() + header.size(), buf->data());
        // No need to fill audio frames.
        return 336;
      }));
  EXPECT_CALL(audio_, OnError());

  receiver_.StartWithSocket(&audio_, std::move(socket));
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ReceiveError) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Write(_, _, _, _)).WillOnce(Return(16));
  EXPECT_CALL(*socket, Read(_, _, _))
      .WillOnce(Return(net::ERR_CONNECTION_RESET));
  EXPECT_CALL(audio_, OnError());

  receiver_.StartWithSocket(&audio_, std::move(socket));
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ReceiveEosMessage) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Write(_, _, _, _)).WillOnce(Return(16));
  EXPECT_CALL(*socket, Read(_, _, _)).WillOnce(Return(0));
  EXPECT_CALL(audio_, OnError());

  receiver_.StartWithSocket(&audio_, std::move(socket));
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace capture_service
}  // namespace media
}  // namespace chromecast
