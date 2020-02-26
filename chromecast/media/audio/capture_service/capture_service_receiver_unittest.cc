// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/capture_service/capture_service_receiver.h"

#include <cstddef>
#include <memory>

#include "base/big_endian.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "chromecast/media/audio/capture_service/message_parsing_utils.h"
#include "chromecast/media/audio/capture_service/packet_header.h"
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

constexpr StreamInfo kStreamInfo =
    StreamInfo{StreamType::kSoftwareEchoCancelled, 1,
               SampleFormat::PLANAR_FLOAT, 16000, 160};
constexpr PacketHeader kPacketHeader =
    PacketHeader{0,
                 static_cast<uint8_t>(MessageType::kAudio),
                 static_cast<uint8_t>(kStreamInfo.stream_type),
                 kStreamInfo.num_channels,
                 static_cast<uint8_t>(kStreamInfo.sample_format),
                 kStreamInfo.sample_rate,
                 kStreamInfo.frames_per_buffer};

void FillHeader(char* buf, uint16_t size, const PacketHeader& header) {
  base::WriteBigEndian(buf, size);
  memcpy(buf + sizeof(size),
         reinterpret_cast<const char*>(&header) +
             offsetof(struct PacketHeader, message_type),
         sizeof(header) - offsetof(struct PacketHeader, message_type));
}

class MockStreamSocket : public chromecast::MockStreamSocket {
 public:
  MockStreamSocket() = default;
  ~MockStreamSocket() override = default;
};

class CaptureServiceReceiverTest : public ::testing::Test {
 public:
  CaptureServiceReceiverTest()
      : receiver_(StreamType::kSoftwareEchoCancelled,
                  kStreamInfo.sample_rate,
                  kStreamInfo.num_channels,
                  kStreamInfo.frames_per_buffer) {
    receiver_.SetTaskRunnerForTest(base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_BLOCKING}));
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
        EXPECT_EQ(buf_len, static_cast<int>(sizeof(PacketHeader)));
        const char* data = buf->data();
        uint16_t size;
        base::ReadBigEndian(data, &size);
        EXPECT_EQ(size, sizeof(PacketHeader) - sizeof(size));
        PacketHeader header;
        memcpy(&header, data, sizeof(PacketHeader));
        EXPECT_EQ(header.message_type, static_cast<uint8_t>(MessageType::kAck));
        EXPECT_EQ(header.stream_type, kPacketHeader.stream_type);
        EXPECT_EQ(header.num_channels, kPacketHeader.num_channels);
        EXPECT_EQ(header.sample_format, kPacketHeader.sample_format);
        EXPECT_EQ(header.sample_rate, kPacketHeader.sample_rate);
        EXPECT_EQ(header.timestamp_or_frames,
                  kPacketHeader.timestamp_or_frames);
        return buf_len;
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
      .WillOnce(Invoke([](net::IOBuffer* buf, int buf_len,
                          net::CompletionOnceCallback) {
        int total_size = sizeof(PacketHeader) + DataSizeInBytes(kStreamInfo);
        EXPECT_GE(buf_len, total_size);
        uint16_t size = total_size - sizeof(uint16_t);
        PacketHeader header = kPacketHeader;
        header.timestamp_or_frames = 0;  // Timestamp.
        FillHeader(buf->data(), size, header);
        return total_size;  // No need to fill audio frames.
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

TEST_F(CaptureServiceReceiverTest, ReceiveAckMessage) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Write(_, _, _, _)).WillOnce(Return(16));
  EXPECT_CALL(*socket, Read(_, _, _))
      .WillOnce(Invoke(
          [](net::IOBuffer* buf, int buf_len, net::CompletionOnceCallback) {
            int total_size = sizeof(PacketHeader);
            EXPECT_GE(buf_len, total_size);
            uint16_t size = total_size - sizeof(uint16_t);
            PacketHeader header = kPacketHeader;
            header.message_type = static_cast<uint8_t>(MessageType::kAck);
            header.sample_format =
                static_cast<uint8_t>(SampleFormat::INTERLEAVED_INT16);
            FillHeader(buf->data(), size, header);
            return total_size;
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
      .WillOnce(Invoke([](net::IOBuffer* buf, int buf_len,
                          net::CompletionOnceCallback) {
        int total_size = sizeof(PacketHeader) + DataSizeInBytes(kStreamInfo);
        EXPECT_GE(buf_len, total_size);
        uint16_t size = total_size - sizeof(uint16_t);
        PacketHeader header = kPacketHeader;
        header.sample_format = static_cast<uint8_t>(SampleFormat::LAST_FORMAT) +
                               1;        // Invalid format.
        header.timestamp_or_frames = 0;  // Timestamp.
        FillHeader(buf->data(), size, header);
        return total_size;  // No need to fill audio frames.
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
