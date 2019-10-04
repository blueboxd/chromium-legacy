// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/mixer_socket.h"

#include <cstring>
#include <limits>
#include <utility>

#include "base/big_endian.h"
#include "base/logging.h"
#include "chromecast/media/audio/mixer_service/constants.h"
#include "net/base/io_buffer.h"
#include "net/socket/stream_socket.h"

namespace chromecast {
namespace media {
namespace mixer_service {

namespace {

bool ReceiveProto(const char* data, int size, Generic* message) {
  int32_t padding_bytes;
  if (size < static_cast<int>(sizeof(padding_bytes))) {
    LOG(ERROR) << "Invalid metadata message size " << size;
    return false;
  }

  base::ReadBigEndian(data, &padding_bytes);
  data += sizeof(padding_bytes);
  size -= sizeof(padding_bytes);

  if (padding_bytes < 0 || padding_bytes > 3) {
    LOG(ERROR) << "Invalid padding bytes count: " << padding_bytes;
    return false;
  }

  if (size < padding_bytes) {
    LOG(ERROR) << "Size " << size << " is smaller than padding "
               << padding_bytes;
    return false;
  }

  if (!message->ParseFromArray(data, size - padding_bytes)) {
    LOG(ERROR) << "Failed to parse incoming metadata";
    return false;
  }
  return true;
}

}  // namespace

bool MixerSocket::Delegate::HandleMetadata(const Generic& message) {
  return true;
}

bool MixerSocket::Delegate::HandleAudioData(char* data,
                                            int size,
                                            int64_t timestamp) {
  return true;
}

bool MixerSocket::Delegate::HandleAudioBuffer(
    scoped_refptr<net::IOBuffer> buffer,
    char* data,
    int size,
    int64_t timestamp) {
  return HandleAudioData(data, size, timestamp);
}

// static
constexpr size_t MixerSocket::kAudioHeaderSize;
constexpr size_t MixerSocket::kAudioMessageHeaderSize;

MixerSocket::MixerSocket(std::unique_ptr<net::StreamSocket> socket,
                         Delegate* delegate)
    : SmallMessageSocket(std::move(socket)), delegate_(delegate) {
  DCHECK(delegate_);
}

MixerSocket::~MixerSocket() = default;

void MixerSocket::SetDelegate(Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

// static
void MixerSocket::PrepareAudioBuffer(net::IOBuffer* audio_buffer,
                                     int filled_bytes,
                                     int64_t timestamp) {
  // Audio message format:
  //   uint16_t size (for SmallMessageSocket)
  //   uint16_t type (audio or metadata)
  //   uint64_t timestamp
  //   ... audio data ...
  int payload_size = kAudioHeaderSize + filled_bytes;
  uint16_t size = static_cast<uint16_t>(payload_size);
  int16_t type = static_cast<int16_t>(MessageType::kAudio);
  char* ptr = audio_buffer->data();

  base::WriteBigEndian(ptr, size);
  ptr += sizeof(size);
  memcpy(ptr, &type, sizeof(type));
  ptr += sizeof(type);
  memcpy(ptr, &timestamp, sizeof(timestamp));
}

void MixerSocket::SendAudioBuffer(scoped_refptr<net::IOBuffer> audio_buffer,
                                  int filled_bytes,
                                  int64_t timestamp) {
  PrepareAudioBuffer(audio_buffer.get(), filled_bytes, timestamp);
  SendPreparedAudioBuffer(std::move(audio_buffer));
}

void MixerSocket::SendPreparedAudioBuffer(
    scoped_refptr<net::IOBuffer> audio_buffer) {
  uint16_t payload_size;
  base::ReadBigEndian(audio_buffer->data(), &payload_size);
  DCHECK_GE(payload_size, kAudioHeaderSize);
  if (!SmallMessageSocket::SendBuffer(audio_buffer.get(),
                                      sizeof(uint16_t) + payload_size)) {
    write_queue_.push(std::move(audio_buffer));
  }
}

void MixerSocket::SendProto(const google::protobuf::MessageLite& message) {
  int16_t type = static_cast<int16_t>(MessageType::kMetadata);
  int message_size = message.ByteSize();
  int32_t padding_bytes = (4 - (message_size % 4)) % 4;

  int total_size =
      sizeof(type) + sizeof(padding_bytes) + message_size + padding_bytes;
  scoped_refptr<net::IOBufferWithSize> storage;
  void* buffer = PrepareSend(total_size);
  char* ptr;
  if (buffer) {
    ptr = reinterpret_cast<char*>(buffer);
  } else {
    storage = base::MakeRefCounted<net::IOBufferWithSize>(sizeof(uint16_t) +
                                                          total_size);

    ptr = storage->data();
    base::WriteBigEndian(ptr, static_cast<uint16_t>(total_size));
    ptr += sizeof(uint16_t);
  }

  base::WriteBigEndian(ptr, type);
  ptr += sizeof(type);
  base::WriteBigEndian(ptr, padding_bytes);
  ptr += sizeof(padding_bytes);
  message.SerializeToArray(ptr, message_size);
  ptr += message_size;
  memset(ptr, 0, padding_bytes);

  if (buffer) {
    Send();
  }
  if (storage) {
    write_queue_.push(std::move(storage));
  }
}

void MixerSocket::OnSendUnblocked() {
  while (!write_queue_.empty()) {
    uint16_t message_size;
    base::ReadBigEndian(write_queue_.front()->data(), &message_size);
    if (!SmallMessageSocket::SendBuffer(write_queue_.front().get(),
                                        sizeof(uint16_t) + message_size)) {
      return;
    }
    write_queue_.pop();
  }
}

void MixerSocket::OnError(int error) {
  LOG(ERROR) << "Socket error from " << this << ": " << error;
  delegate_->OnConnectionError();
}

void MixerSocket::OnEndOfStream() {
  delegate_->OnConnectionError();
}

bool MixerSocket::OnMessage(char* data, int size) {
  int16_t type;
  if (size < static_cast<int>(sizeof(type))) {
    LOG(ERROR) << "Invalid message size " << size << " from " << this;
    delegate_->OnConnectionError();
    return false;
  }

  memcpy(&type, data, sizeof(type));
  data += sizeof(type);
  size -= sizeof(type);

  switch (static_cast<MessageType>(type)) {
    case MessageType::kMetadata:
      return ParseMetadata(data, size);
    case MessageType::kAudio:
      return ParseAudio(data, size);
    default:
      return true;  // Ignore unhandled message types.
  }
}

bool MixerSocket::OnMessageBuffer(scoped_refptr<net::IOBuffer> buffer,
                                  int size) {
  if (size < static_cast<int>(sizeof(uint16_t) + sizeof(int16_t))) {
    LOG(ERROR) << "Invalid buffer size " << size << " from " << this;
    delegate_->OnConnectionError();
    return false;
  }

  char* data = buffer->data() + sizeof(uint16_t);
  size -= sizeof(uint16_t);
  int16_t type;
  memcpy(&type, data, sizeof(type));
  data += sizeof(type);
  size -= sizeof(type);

  switch (static_cast<MessageType>(type)) {
    case MessageType::kMetadata:
      return ParseMetadata(data, size);
    case MessageType::kAudio:
      return ParseAudioBuffer(std::move(buffer), data, size);
    default:
      return true;  // Ignore unhandled message types.
  }
}

bool MixerSocket::ParseMetadata(char* data, int size) {
  Generic message;
  if (!ReceiveProto(data, size, &message)) {
    LOG(INFO) << "Invalid metadata message from " << this;
    delegate_->OnConnectionError();
    return false;
  }

  return delegate_->HandleMetadata(message);
}

bool MixerSocket::ParseAudio(char* data, int size) {
  int64_t timestamp;
  if (size < static_cast<int>(sizeof(timestamp))) {
    LOG(ERROR) << "Invalid audio packet size " << size << " from " << this;
    delegate_->OnConnectionError();
    return false;
  }

  memcpy(&timestamp, data, sizeof(timestamp));
  data += sizeof(timestamp);
  size -= sizeof(timestamp);

  return delegate_->HandleAudioData(data, size, timestamp);
}

bool MixerSocket::ParseAudioBuffer(scoped_refptr<net::IOBuffer> buffer,
                                   char* data,
                                   int size) {
  int64_t timestamp;
  if (size < static_cast<int>(sizeof(timestamp))) {
    LOG(ERROR) << "Invalid audio buffer size " << size << " from " << this;
    delegate_->OnConnectionError();
    return false;
  }

  memcpy(&timestamp, data, sizeof(timestamp));
  data += sizeof(timestamp);
  size -= sizeof(timestamp);

  return delegate_->HandleAudioBuffer(std::move(buffer), data, size, timestamp);
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
