// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SOCKET_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SOCKET_H_

#include <cstdint>
#include <memory>
#include <queue>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"
#include "chromecast/net/small_message_socket.h"

namespace net {
class IOBuffer;
class StreamSocket;
}  // namespace net

namespace chromecast {
namespace media {
namespace mixer_service {

// Base class for sending and receiving messages to/from the mixer service.
// Not thread-safe; all usage of a given instance must be on the same sequence.
class MixerSocket : public SmallMessageSocket {
 public:
  explicit MixerSocket(std::unique_ptr<net::StreamSocket> socket);
  ~MixerSocket() override;

  // 16-bit type and 64-bit timestamp.
  static constexpr size_t kAudioHeaderSize = sizeof(int16_t) + sizeof(int64_t);
  // Includes additional 16-bit size field for SmallMessageSocket.
  static constexpr size_t kAudioMessageHeaderSize =
      sizeof(uint16_t) + kAudioHeaderSize;

  // Fills in the audio message header for |buffer|, so it can later be sent via
  // SendPreparedAudioBuffer(). |buffer| should have |kAudioMessageHeaderSize|
  // bytes reserved at the start of the buffer, followed by |filled_bytes| of
  // audio data.
  static void PrepareAudioBuffer(net::IOBuffer* buffer,
                                 int filled_bytes,
                                 int64_t timestamp);

  // Prepares |audio_buffer| and then sends it across the connection.
  void SendAudioBuffer(scoped_refptr<net::IOBuffer> audio_buffer,
                       int filled_bytes,
                       int64_t timestamp);

  // Sends |audio_buffer| across the connection. |audio_buffer| should have
  // previously been prepared using PrepareAudioBuffer().
  void SendPreparedAudioBuffer(scoped_refptr<net::IOBuffer> audio_buffer);

  // Sends an arbitrary protobuf across the connection.
  void SendProto(const google::protobuf::MessageLite& message);

 protected:
  // Called when metadata is received from the other side of the connection.
  virtual bool HandleMetadata(const Generic& message) = 0;

  // Called when audio data is received from the other side of the connection.
  virtual bool HandleAudioData(char* data, int size, int64_t timestamp) = 0;

  // Called when the connection is lost; no further data will be sent or
  // received after OnConnectionError() is called. It is safe to delete |this|
  // inside the OnConnectionError() implementation.
  virtual void OnConnectionError() = 0;

 private:
  // SmallMessageSocket implementation:
  void OnSendUnblocked() override;
  void OnError(int error) override;
  void OnEndOfStream() override;
  bool OnMessage(char* data, int size) override;

  bool ParseMetadata(char* data, int size);
  bool ParseAudio(char* data, int size);

  std::queue<scoped_refptr<net::IOBuffer>> write_queue_;

  DISALLOW_COPY_AND_ASSIGN(MixerSocket);
};

// Handles client-side sending messages to the mixer service and receiving them
// from the mixer service. Also not thread-safe.
class MixerClientSocket : public MixerSocket {
 public:
  class Delegate {
   public:
    virtual void HandleMetadata(const Generic& message) {}
    virtual void HandleAudioData(char* data, int size, int64_t timestamp) {}
    virtual void OnConnectionError() {}

   protected:
    virtual ~Delegate() = default;
  };

  MixerClientSocket(std::unique_ptr<net::StreamSocket> socket,
                    Delegate* delegate);
  ~MixerClientSocket() override;

 private:
  // MixerSocket implementation:
  bool HandleMetadata(const Generic& message) override;
  bool HandleAudioData(char* data, int size, int64_t timestamp) override;
  void OnConnectionError() override;

  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(MixerClientSocket);
};

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SOCKET_H_
