// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_test_helpers.h"

#include <limits>

#include "base/stl_util.h"
#include "base/sys_byteorder.h"
#include "media/base/video_decoder_config.h"
#include "media/video/h264_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

// The structure for IVF frame header. IVF is a video file format.
// The helpful description is in https://wiki.multimedia.cx/index.php/IVF.
struct EncodedDataHelper::IVFHeader {
  uint32_t frame_size;
  uint64_t timestamp;
};

// The structure for IVF frame data and header.
// The data to be read is |header.frame_size| bytes from |data|.
struct EncodedDataHelper::IVFFrame {
  const char* data;
  IVFHeader header;
};

EncodedDataHelper::EncodedDataHelper(const std::vector<uint8_t>& stream,
                                     VideoCodecProfile profile)
    : data_(std::string(reinterpret_cast<const char*>(stream.data()),
                        stream.size())),
      profile_(profile) {}

EncodedDataHelper::~EncodedDataHelper() {
  base::STLClearObject(&data_);
}

bool EncodedDataHelper::IsNALHeader(const std::string& data, size_t pos) {
  return data[pos] == 0 && data[pos + 1] == 0 && data[pos + 2] == 0 &&
         data[pos + 3] == 1;
}

scoped_refptr<DecoderBuffer> EncodedDataHelper::GetNextBuffer() {
  switch (VideoCodecProfileToVideoCodec(profile_)) {
    case kCodecH264:
      return GetNextFragment();
    case kCodecVP8:
    case kCodecVP9:
      return GetNextFrame();
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<DecoderBuffer> EncodedDataHelper::GetNextFragment() {
  if (next_pos_to_decode_ == 0) {
    size_t skipped_fragments_count = 0;
    if (!LookForSPS(&skipped_fragments_count)) {
      next_pos_to_decode_ = 0;
      return nullptr;
    }
    num_skipped_fragments_ += skipped_fragments_count;
  }

  size_t start_pos = next_pos_to_decode_;
  size_t next_nalu_pos = GetBytesForNextNALU(start_pos);

  // Update next_pos_to_decode_.
  next_pos_to_decode_ = next_nalu_pos;
  return DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(&data_[start_pos]),
      next_nalu_pos - start_pos);
}

size_t EncodedDataHelper::GetBytesForNextNALU(size_t start_pos) {
  size_t pos = start_pos;
  if (pos + 4 > data_.size())
    return pos;
  if (!IsNALHeader(data_, pos)) {
    ADD_FAILURE();
    return std::numeric_limits<std::size_t>::max();
  }
  pos += 4;
  while (pos + 4 <= data_.size() && !IsNALHeader(data_, pos)) {
    ++pos;
  }
  if (pos + 3 >= data_.size())
    pos = data_.size();
  return pos;
}

bool EncodedDataHelper::LookForSPS(size_t* skipped_fragments_count) {
  *skipped_fragments_count = 0;
  while (next_pos_to_decode_ + 4 < data_.size()) {
    if ((data_[next_pos_to_decode_ + 4] & 0x1f) == 0x7) {
      return true;
    }
    *skipped_fragments_count += 1;
    next_pos_to_decode_ = GetBytesForNextNALU(next_pos_to_decode_);
  }
  return false;
}

scoped_refptr<DecoderBuffer> EncodedDataHelper::GetNextFrame() {
  // Helpful description: http://wiki.multimedia.cx/index.php?title=IVF
  constexpr size_t kIVFHeaderSize = 32;
  // Only IVF video files are supported. The first 4bytes of an IVF video file's
  // header should be "DKIF".
  if (next_pos_to_decode_ == 0) {
    if ((data_.size() < kIVFHeaderSize) || strncmp(&data_[0], "DKIF", 4) != 0) {
      LOG(ERROR) << "Unexpected data encountered while parsing IVF header";
      return nullptr;
    }
    next_pos_to_decode_ = kIVFHeaderSize;  // Skip IVF header.
  }

  // Group IVF data whose timestamps are the same. Spatial layers in a
  // spatial-SVC stream may separately be stored in IVF data, where the
  // timestamps of the IVF frame headers are the same. However, it is necessary
  // for VD(A) to feed the spatial layers by a single DecoderBuffer. So this
  // grouping is required.
  std::vector<IVFFrame> ivf_frames;
  while (!ReachEndOfStream()) {
    auto frame_header = GetNextIVFFrameHeader();
    if (!frame_header)
      return nullptr;

    // Timestamp is different from the current one. The next IVF data must be
    // grouped in the next group.
    if (!ivf_frames.empty() &&
        frame_header->timestamp != ivf_frames[0].header.timestamp) {
      break;
    }

    auto frame_data = ReadNextIVFFrame();
    if (!frame_data)
      return nullptr;

    ivf_frames.push_back(*frame_data);
  }

  if (ivf_frames.empty()) {
    LOG(ERROR) << "No IVF frame is available";
    return nullptr;
  }

  // Standard stream case.
  if (ivf_frames.size() == 1) {
    return DecoderBuffer::CopyFrom(
        reinterpret_cast<const uint8_t*>(ivf_frames[0].data),
        ivf_frames[0].header.frame_size);
  }

  if (ivf_frames.size() > 3) {
    LOG(ERROR) << "Number of IVF frames with same timestamps exceeds maximum of"
               << "3: ivf_frames.size()=" << ivf_frames.size();
    return nullptr;
  }

  std::string data;
  std::vector<uint32_t> frame_sizes;
  frame_sizes.reserve(ivf_frames.size());
  for (const IVFFrame& ivf : ivf_frames) {
    data.append(ivf.data, ivf.header.frame_size);
    frame_sizes.push_back(ivf.header.frame_size);
  }

  // Copy frame_sizes information to DecoderBuffer's side data. Since side_data
  // is uint8_t*, we need to copy as uint8_t from uint32_t. The copied data is
  // recognized as uint32_t in VD(A).
  const uint8_t* side_data =
      reinterpret_cast<const uint8_t*>(frame_sizes.data());
  size_t side_data_size =
      frame_sizes.size() * sizeof(uint32_t) / sizeof(uint8_t);

  return DecoderBuffer::CopyFrom(reinterpret_cast<const uint8_t*>(data.data()),
                                 data.size(), side_data, side_data_size);
}

base::Optional<EncodedDataHelper::IVFHeader>
EncodedDataHelper::GetNextIVFFrameHeader() const {
  constexpr size_t kIVFFrameHeaderSize = 12;

  const size_t pos = next_pos_to_decode_;

  // Read VP8/9 frame size from IVF header.
  if (pos + kIVFFrameHeaderSize > data_.size()) {
    LOG(ERROR) << "Unexpected data encountered while parsing IVF frame header";
    return base::nullopt;
  }

  uint32_t frame_size = 0;
  uint64_t timestamp = 0;
  memcpy(&frame_size, &data_[pos], 4);
  memcpy(&timestamp, &data_[pos + 4], 8);
  return IVFHeader{frame_size, timestamp};
}

base::Optional<EncodedDataHelper::IVFFrame>
EncodedDataHelper::ReadNextIVFFrame() {
  constexpr size_t kIVFFrameHeaderSize = 12;
  auto frame_header = GetNextIVFFrameHeader();
  if (!frame_header)
    return base::nullopt;

  // Skip IVF frame header.
  const size_t pos = next_pos_to_decode_ + kIVFFrameHeaderSize;

  // Make sure we are not reading out of bounds.
  if (pos + frame_header->frame_size > data_.size()) {
    LOG(ERROR) << "Unexpected data encountered while parsing IVF frame header";
    next_pos_to_decode_ = data_.size();
    return base::nullopt;
  }

  // Update next_pos_to_decode_.
  next_pos_to_decode_ = pos + frame_header->frame_size;

  return IVFFrame{&data_[pos], *frame_header};
}

// static
bool EncodedDataHelper::HasConfigInfo(const uint8_t* data,
                                      size_t size,
                                      VideoCodecProfile profile) {
  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX) {
    H264Parser parser;
    parser.SetStream(data, size);
    H264NALU nalu;
    H264Parser::Result result = parser.AdvanceToNextNALU(&nalu);
    if (result != H264Parser::kOk) {
      // Let the VDA figure out there's something wrong with the stream.
      return false;
    }

    return nalu.nal_unit_type == H264NALU::kSPS;
  } else if (profile >= VP8PROFILE_MIN && profile <= VP9PROFILE_MAX) {
    return (size > 0 && !(data[0] & 0x01));
  }
  // Shouldn't happen at this point.
  LOG(FATAL) << "Invalid profile: " << GetProfileName(profile);
  return false;
}

}  // namespace test
}  // namespace media
