// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/capture_service/capture_service_receiver.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/media/audio/capture_service/constants.h"
#include "chromecast/media/audio/capture_service/message_parsing_utils.h"
#include "chromecast/media/audio/mixer_service/audio_socket_service.h"
#include "chromecast/net/small_message_socket.h"
#include "media/base/limits.h"
#include "net/base/io_buffer.h"
#include "net/socket/stream_socket.h"

// Helper macro to post tasks to the io thread. It is safe to use unretained
// |this|, since |this| owns the thread.
#define ENSURE_ON_IO_THREAD(method, ...)                                   \
  if (!task_runner_->RunsTasksInCurrentSequence()) {                       \
    task_runner_->PostTask(                                                \
        FROM_HERE, base::BindOnce(&CaptureServiceReceiver::method,         \
                                  base::Unretained(this), ##__VA_ARGS__)); \
    return;                                                                \
  }

namespace chromecast {
namespace media {

class CaptureServiceReceiver::Socket : public SmallMessageSocket::Delegate {
 public:
  Socket(std::unique_ptr<net::StreamSocket> socket,
         capture_service::StreamType stream_type,
         int sample_rate,
         int channels,
         int frames_per_buffer,
         ::media::AudioInputStream::AudioInputCallback* input_callback);
  ~Socket() override;

 private:
  // SmallMessageSocket::Delegate implementation:
  void OnSendUnblocked() override;
  void OnError(int error) override;
  void OnEndOfStream() override;
  bool OnMessage(char* data, size_t size) override;

  bool SendRequest();
  void OnInactivityTimeout();
  bool HandleAudio(int64_t timestamp);
  void ReportErrorAndStop();

  SmallMessageSocket socket_;

  const capture_service::StreamType stream_type_;
  const int sample_rate_;
  const std::unique_ptr<::media::AudioBus> audio_bus_;

  ::media::AudioInputStream::AudioInputCallback* input_callback_;

  scoped_refptr<net::IOBufferWithSize> buffer_;

  DISALLOW_COPY_AND_ASSIGN(Socket);
};

CaptureServiceReceiver::Socket::Socket(
    std::unique_ptr<net::StreamSocket> socket,
    capture_service::StreamType stream_type,
    int sample_rate,
    int channels,
    int frames_per_buffer,
    ::media::AudioInputStream::AudioInputCallback* input_callback)
    : socket_(this, std::move(socket)),
      stream_type_(stream_type),
      sample_rate_(sample_rate),
      audio_bus_(::media::AudioBus::Create(channels, frames_per_buffer)),
      input_callback_(input_callback) {
  DCHECK(input_callback_);
  if (!SendRequest()) {
    ReportErrorAndStop();
    return;
  }
  socket_.ReceiveMessages();
}

CaptureServiceReceiver::Socket::~Socket() = default;

bool CaptureServiceReceiver::Socket::SendRequest() {
  capture_service::PacketInfo info;
  info.message_type = capture_service::MessageType::kAck;
  info.stream_info.stream_type = stream_type_;
  info.stream_info.sample_rate = sample_rate_;
  info.stream_info.num_channels = audio_bus_->channels();
  info.stream_info.frames_per_buffer = audio_bus_->frames();
  // Format and timestamp doesn't matter in the request.
  info.stream_info.sample_format = capture_service::SampleFormat::LAST_FORMAT;
  info.timestamp_us = 0;
  buffer_ =
      capture_service::MakeMessage(info, nullptr /* data */, 0 /* data_size */);
  if (!buffer_) {
    return false;
  }
  if (socket_.SendBuffer(buffer_.get(), buffer_->size())) {
    buffer_ = nullptr;
  }
  return true;
}

void CaptureServiceReceiver::Socket::OnSendUnblocked() {
  if (buffer_ && socket_.SendBuffer(buffer_.get(), buffer_->size())) {
    buffer_ = nullptr;
  }
}

void CaptureServiceReceiver::Socket::ReportErrorAndStop() {
  if (input_callback_) {
    input_callback_->OnError();
  }
  input_callback_ = nullptr;
}

void CaptureServiceReceiver::Socket::OnError(int error) {
  LOG(INFO) << "Socket error from " << this << ": " << error;
  ReportErrorAndStop();
}

void CaptureServiceReceiver::Socket::OnEndOfStream() {
  LOG(ERROR) << "Got EOS " << this;
  ReportErrorAndStop();
}

bool CaptureServiceReceiver::Socket::OnMessage(char* data, size_t size) {
  capture_service::PacketInfo info;
  if (!capture_service::ReadHeader(data, size, &info)) {
    ReportErrorAndStop();
    return false;
  }
  if (info.message_type != capture_service::MessageType::kAudio) {
    LOG(WARNING) << "Received non-audio message, ignored.";
    return true;
  }
  DCHECK_EQ(info.stream_info.stream_type, stream_type_);
  if (!capture_service::ReadDataToAudioBus(info.stream_info, data, size,
                                           audio_bus_.get())) {
    ReportErrorAndStop();
    return false;
  }
  return HandleAudio(info.timestamp_us);
}

bool CaptureServiceReceiver::Socket::HandleAudio(int64_t timestamp) {
  if (!input_callback_) {
    LOG(INFO) << "Received audio before stream metadata; ignoring";
    return false;
  }

  input_callback_->OnData(
      audio_bus_.get(),
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(timestamp),
      /* volume =*/1.0);
  return true;
}

// static
constexpr base::TimeDelta CaptureServiceReceiver::kConnectTimeout;

CaptureServiceReceiver::CaptureServiceReceiver(
    capture_service::StreamType stream_type,
    int sample_rate,
    int channels,
    int frames_per_buffer)
    : stream_type_(stream_type),
      sample_rate_(sample_rate),
      channels_(channels),
      frames_per_buffer_(frames_per_buffer),
      io_thread_(__func__) {
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  // TODO(b/137106361): Tweak the thread priority once the thread priority for
  // speech processing gets fixed.
  options.priority = base::ThreadPriority::DISPLAY;
  CHECK(io_thread_.StartWithOptions(options));
  task_runner_ = io_thread_.task_runner();
  DCHECK(task_runner_);
}

CaptureServiceReceiver::~CaptureServiceReceiver() {
  Stop();
  io_thread_.Stop();
}

void CaptureServiceReceiver::Start(
    ::media::AudioInputStream::AudioInputCallback* input_callback) {
  ENSURE_ON_IO_THREAD(Start, input_callback);

  std::string path = capture_service::kDefaultUnixDomainSocketPath;
  int port = capture_service::kDefaultTcpPort;

  StartWithSocket(input_callback, AudioSocketService::Connect(path, port));
}

void CaptureServiceReceiver::StartWithSocket(
    ::media::AudioInputStream::AudioInputCallback* input_callback,
    std::unique_ptr<net::StreamSocket> connecting_socket) {
  ENSURE_ON_IO_THREAD(StartWithSocket, input_callback,
                      std::move(connecting_socket));
  DCHECK(!connecting_socket_);
  DCHECK(!socket_);

  connecting_socket_ = std::move(connecting_socket);
  int result = connecting_socket_->Connect(
      base::BindOnce(&CaptureServiceReceiver::OnConnected,
                     base::Unretained(this), input_callback));
  if (result != net::ERR_IO_PENDING) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CaptureServiceReceiver::OnConnected,
                       base::Unretained(this), input_callback, result));
    return;
  }

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CaptureServiceReceiver::OnConnectTimeout,
                     base::Unretained(this), input_callback),
      kConnectTimeout);
}

void CaptureServiceReceiver::OnConnected(
    ::media::AudioInputStream::AudioInputCallback* input_callback,
    int result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(result, net::ERR_IO_PENDING);
  if (!connecting_socket_) {
    return;
  }

  if (result == net::OK) {
    socket_ = std::make_unique<Socket>(std::move(connecting_socket_),
                                       stream_type_, sample_rate_, channels_,
                                       frames_per_buffer_, input_callback);
  } else {
    LOG(INFO) << "Connecting failed: " << net::ErrorToString(result);
    input_callback->OnError();
    connecting_socket_.reset();
  }
}

void CaptureServiceReceiver::OnConnectTimeout(
    ::media::AudioInputStream::AudioInputCallback* input_callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!connecting_socket_) {
    return;
  }
  LOG(ERROR) << __func__;
  input_callback->OnError();
  connecting_socket_.reset();
}

void CaptureServiceReceiver::Stop() {
  base::WaitableEvent finished;
  StopOnTaskRunner(&finished);
  finished.Wait();
}

void CaptureServiceReceiver::StopOnTaskRunner(base::WaitableEvent* finished) {
  ENSURE_ON_IO_THREAD(StopOnTaskRunner, finished);
  connecting_socket_.reset();
  socket_.reset();
  finished->Signal();
}

void CaptureServiceReceiver::SetTaskRunnerForTest(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = std::move(task_runner);
}

}  // namespace media
}  // namespace chromecast
