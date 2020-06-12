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
    StreamInfo{StreamType::kSoftwareEchoCancelled,
               AudioCodec::kPcm,
               1,
               SampleFormat::PLANAR_FLOAT,
               16000,
               160};
constexpr PacketHeader kRequestPacketHeader =
    PacketHeader{0,
                 static_cast<uint8_t>(MessageType::kRequest),
                 static_cast<uint8_t>(kStreamInfo.stream_type),
                 static_cast<uint8_t>(kStreamInfo.audio_codec),
                 kStreamInfo.num_channels,
                 kStreamInfo.sample_rate,
                 kStreamInfo.frames_per_buffer};
constexpr PacketHeader kPcmAudioPacketHeader =
    PacketHeader{0,
                 static_cast<uint8_t>(MessageType::kPcmAudio),
                 static_cast<uint8_t>(kStreamInfo.stream_type),
                 static_cast<uint8_t>(kStreamInfo.sample_format),
                 kStreamInfo.num_channels,
                 kStreamInfo.sample_rate,
                 0};

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

class MockCaptureServiceReceiverDelegate
    : public chromecast::media::CaptureServiceReceiver::Delegate {
 public:
  MockCaptureServiceReceiverDelegate() = default;
  ~MockCaptureServiceReceiverDelegate() override = default;

  MOCK_METHOD(bool, OnCaptureData, (const char*, size_t), (override));
  MOCK_METHOD(void, OnCaptureError, (), (override));
};

class CaptureServiceReceiverTest : public ::testing::Test {
 public:
  CaptureServiceReceiverTest() : receiver_(kStreamInfo, &delegate_) {
    receiver_.SetTaskRunnerForTest(base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_BLOCKING}));
  }
  ~CaptureServiceReceiverTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockCaptureServiceReceiverDelegate delegate_;
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
  receiver_.StartWithSocket(std::move(socket1));
  task_environment_.RunUntilIdle();
  receiver_.Stop();

  // Async.
  receiver_.StartWithSocket(std::move(socket2));
  receiver_.Stop();
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ConnectFailed) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::ERR_FAILED));
  EXPECT_CALL(delegate_, OnCaptureError());

  receiver_.StartWithSocket(std::move(socket));
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ConnectTimeout) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::ERR_IO_PENDING));
  EXPECT_CALL(delegate_, OnCaptureError());

  receiver_.StartWithSocket(std::move(socket));
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
        EXPECT_EQ(header.message_type, kRequestPacketHeader.message_type);
        EXPECT_EQ(header.stream_type, kRequestPacketHeader.stream_type);
        EXPECT_EQ(header.codec_or_sample_format,
                  kRequestPacketHeader.codec_or_sample_format);
        EXPECT_EQ(header.num_channels, kRequestPacketHeader.num_channels);
        EXPECT_EQ(header.sample_rate, kRequestPacketHeader.sample_rate);
        EXPECT_EQ(header.timestamp_or_frames,
                  kRequestPacketHeader.timestamp_or_frames);
        return buf_len;
      }));
  EXPECT_CALL(*socket, Read(_, _, _)).WillOnce(Return(net::ERR_IO_PENDING));

  receiver_.StartWithSocket(std::move(socket));
  task_environment_.RunUntilIdle();
  // Stop receiver to disconnect socket, since receiver doesn't own the IO
  // task runner in unittests.
  receiver_.Stop();
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ReceivePcmAudioMessage) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Write(_, _, _, _)).WillOnce(Return(16));
  EXPECT_CALL(*socket, Read(_, _, _))
      .WillOnce(Invoke([](net::IOBuffer* buf, int buf_len,
                          net::CompletionOnceCallback) {
        int total_size = sizeof(PacketHeader) + DataSizeInBytes(kStreamInfo);
        EXPECT_GE(buf_len, total_size);
        uint16_t size = total_size - sizeof(uint16_t);
        PacketHeader header = kPcmAudioPacketHeader;
        FillHeader(buf->data(), size, header);
        return total_size;  // No need to fill audio frames.
      }))
      .WillOnce(Return(net::ERR_IO_PENDING));
  EXPECT_CALL(delegate_, OnCaptureData(_, _)).WillOnce(Return(true));

  receiver_.StartWithSocket(std::move(socket));
  task_environment_.RunUntilIdle();
  // Stop receiver to disconnect socket, since receiver doesn't own the IO
  // task runner in unittests.
  receiver_.Stop();
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ReceiveError) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Write(_, _, _, _)).WillOnce(Return(16));
  EXPECT_CALL(*socket, Read(_, _, _))
      .WillOnce(Return(net::ERR_CONNECTION_RESET));
  EXPECT_CALL(delegate_, OnCaptureError());

  receiver_.StartWithSocket(std::move(socket));
  task_environment_.RunUntilIdle();
}

TEST_F(CaptureServiceReceiverTest, ReceiveEosMessage) {
  auto socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*socket, Connect(_)).WillOnce(Return(net::OK));
  EXPECT_CALL(*socket, Write(_, _, _, _)).WillOnce(Return(16));
  EXPECT_CALL(*socket, Read(_, _, _)).WillOnce(Return(0));
  EXPECT_CALL(delegate_, OnCaptureError());

  receiver_.StartWithSocket(std::move(socket));
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace capture_service
}  // namespace media
}  // namespace chromecast
