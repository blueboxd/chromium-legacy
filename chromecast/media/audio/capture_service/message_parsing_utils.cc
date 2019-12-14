// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/capture_service/message_parsing_utils.h"

#include <algorithm>
#include <limits>

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "media/base/limits.h"

namespace chromecast {
namespace media {
namespace capture_service {
namespace {

// Size in bytes of the total/message header.
constexpr size_t kMessageHeaderBytes = 14;
constexpr size_t kTotalHeaderBytes = kMessageHeaderBytes + sizeof(uint16_t);

// Check if audio data is properly aligned and has valid frame size. Return the
// number of frames if they are all good, otherwise return 0 to indicate
// failure.
template <typename T>
int CheckAudioData(int channels, const char* data, size_t data_size) {
  if (reinterpret_cast<const uintptr_t>(data) % sizeof(T) != 0u) {
    LOG(ERROR) << "Misaligned audio data";
    return 0;
  }

  const int frame_size = channels * sizeof(T);
  if (frame_size == 0) {
    LOG(ERROR) << "Frame size is 0.";
    return 0;
  }
  const int frames = data_size / frame_size;
  if (data_size % frame_size != 0) {
    LOG(ERROR) << "Audio data size (" << data_size
               << ") is not an integer number of frames (" << frame_size
               << ").";
    return 0;
  }
  return frames;
}

template <typename Traits>
bool ConvertInterleavedData(int channels,
                            const char* data,
                            size_t data_size,
                            ::media::AudioBus* audio) {
  const int frames =
      CheckAudioData<typename Traits::ValueType>(channels, data, data_size);
  if (frames <= 0) {
    return false;
  }

  DCHECK_EQ(frames, audio->frames());
  audio->FromInterleaved<Traits>(
      reinterpret_cast<const typename Traits::ValueType*>(data), frames);
  return true;
}

template <typename Traits>
bool ConvertPlanarData(int channels,
                       const char* data,
                       size_t data_size,
                       ::media::AudioBus* audio) {
  const int frames =
      CheckAudioData<typename Traits::ValueType>(channels, data, data_size);
  if (frames <= 0) {
    return false;
  }

  DCHECK_EQ(frames, audio->frames());
  const typename Traits::ValueType* base_data =
      reinterpret_cast<const typename Traits::ValueType*>(data);
  for (int c = 0; c < channels; ++c) {
    const typename Traits::ValueType* source = base_data + c * frames;
    float* dest = audio->channel(c);
    for (int f = 0; f < frames; ++f) {
      dest[f] = Traits::ToFloat(source[f]);
    }
  }
  return true;
}

bool ConvertPlanarFloat(int channels,
                        const char* data,
                        size_t data_size,
                        ::media::AudioBus* audio) {
  const int frames = CheckAudioData<float>(channels, data, data_size);
  if (frames <= 0) {
    return false;
  }

  DCHECK_EQ(frames, audio->frames());
  const float* base_data = reinterpret_cast<const float*>(data);
  for (int c = 0; c < channels; ++c) {
    const float* source = base_data + c * frames;
    std::copy(source, source + frames, audio->channel(c));
  }
  return true;
}

bool ConvertData(int channels,
                 SampleFormat format,
                 const char* data,
                 size_t data_size,
                 ::media::AudioBus* audio) {
  switch (format) {
    case capture_service::SampleFormat::INTERLEAVED_INT16:
      return ConvertInterleavedData<::media::SignedInt16SampleTypeTraits>(
          channels, data, data_size, audio);
    case capture_service::SampleFormat::INTERLEAVED_INT32:
      return ConvertInterleavedData<::media::SignedInt32SampleTypeTraits>(
          channels, data, data_size, audio);
    case capture_service::SampleFormat::INTERLEAVED_FLOAT:
      return ConvertInterleavedData<::media::Float32SampleTypeTraits>(
          channels, data, data_size, audio);
    case capture_service::SampleFormat::PLANAR_INT16:
      return ConvertPlanarData<::media::SignedInt16SampleTypeTraits>(
          channels, data, data_size, audio);
    case capture_service::SampleFormat::PLANAR_INT32:
      return ConvertPlanarData<::media::SignedInt32SampleTypeTraits>(
          channels, data, data_size, audio);
    case capture_service::SampleFormat::PLANAR_FLOAT:
      return ConvertPlanarFloat(channels, data, data_size, audio);
  }
  LOG(ERROR) << "Unknown sample format " << static_cast<int>(format);
  return false;
}

}  // namespace

char* PopulateHeader(char* data, size_t size, const PacketInfo& packet_info) {
  // Currently doesn't support negative timestamps.
  DCHECK_GE(packet_info.timestamp_us, 0);
  base::BigEndianWriter data_writer(data, size);
  const StreamInfo& stream_info = packet_info.stream_info;
  // In audio message, the header contains a timestamp field, while in request
  // message, it instead contains a frames field.
  if (!data_writer.WriteU16(  // Deduct the size of |size| itself.
          static_cast<uint16_t>(size - sizeof(uint16_t))) ||
      !data_writer.WriteU8(static_cast<uint8_t>(packet_info.has_audio)) ||
      !data_writer.WriteU8(static_cast<uint8_t>(stream_info.stream_type)) ||
      !data_writer.WriteU8(static_cast<uint8_t>(stream_info.num_channels)) ||
      !data_writer.WriteU8(static_cast<uint8_t>(stream_info.sample_format)) ||
      !data_writer.WriteU16(static_cast<uint16_t>(stream_info.sample_rate)) ||
      !data_writer.WriteU64(static_cast<uint64_t>(
          packet_info.has_audio ? packet_info.timestamp_us
                                : stream_info.frames_per_buffer))) {
    LOG(ERROR) << "Fail to write message header.";
    return nullptr;
  }
  DCHECK_EQ(size, data_writer.remaining() + kTotalHeaderBytes);
  return data_writer.ptr();
}

bool ReadHeader(const char* data, size_t size, PacketInfo* packet_info) {
  DCHECK(packet_info);
  uint8_t has_audio, type, channels, format;
  uint16_t sample_rate;
  uint64_t timestamp_or_frames;
  base::BigEndianReader data_reader(data, size);
  if (!data_reader.ReadU8(&has_audio) || !data_reader.ReadU8(&type) ||
      type > static_cast<int>(StreamType::kLastType) ||
      !data_reader.ReadU8(&channels) || !data_reader.ReadU8(&format) ||
      format > static_cast<int>(SampleFormat::LAST_FORMAT) ||
      !data_reader.ReadU16(&sample_rate) ||
      !data_reader.ReadU64(&timestamp_or_frames)) {
    LOG(ERROR) << "Invalid message header.";
    return false;
  }
  DCHECK_EQ(size, data_reader.remaining() + kMessageHeaderBytes);
  if (channels > ::media::limits::kMaxChannels) {
    LOG(ERROR) << "Invalid number of channels: " << channels;
    return false;
  }
  if (has_audio &&
      !base::CheckedNumeric<uint64_t>(timestamp_or_frames).IsValid<int64_t>()) {
    LOG(ERROR) << "Invalid timestamp: " << timestamp_or_frames;
    return false;
  }
  if (!has_audio &&
      !base::CheckedNumeric<uint64_t>(timestamp_or_frames).IsValid<int>()) {
    LOG(ERROR) << "Invalid number of frames: " << timestamp_or_frames;
    return false;
  }
  packet_info->has_audio = has_audio;
  packet_info->stream_info.stream_type = static_cast<StreamType>(type);
  packet_info->stream_info.num_channels = channels;
  packet_info->stream_info.sample_format = static_cast<SampleFormat>(format);
  packet_info->stream_info.sample_rate = sample_rate;
  if (has_audio) {
    packet_info->timestamp_us = timestamp_or_frames;
  } else {
    packet_info->stream_info.frames_per_buffer = timestamp_or_frames;
  }
  return true;
}

scoped_refptr<net::IOBufferWithSize> MakeMessage(const PacketInfo& packet_info,
                                                 const char* data,
                                                 size_t data_size) {
  const size_t total_size = kTotalHeaderBytes + data_size;
  DCHECK_LE(total_size, std::numeric_limits<uint16_t>::max());
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(total_size);
  char* ptr = PopulateHeader(io_buffer->data(), io_buffer->size(), packet_info);
  if (!ptr) {
    return nullptr;
  }
  if (packet_info.has_audio && data_size > 0) {
    DCHECK(data);
    std::copy(data, data + data_size, ptr);
  }
  return io_buffer;
}

bool ReadDataToAudioBus(const StreamInfo& stream_info,
                        const char* data,
                        size_t size,
                        ::media::AudioBus* audio_bus) {
  DCHECK(audio_bus);
  DCHECK_EQ(stream_info.num_channels, audio_bus->channels());
  return ConvertData(stream_info.num_channels, stream_info.sample_format,
                     data + kMessageHeaderBytes, size - kMessageHeaderBytes,
                     audio_bus);
}

}  // namespace capture_service
}  // namespace media
}  // namespace chromecast
