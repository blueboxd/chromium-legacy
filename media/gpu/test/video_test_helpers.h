// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_TEST_HELPERS_H_
#define MEDIA_GPU_TEST_VIDEO_TEST_HELPERS_H_

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_codecs.h"

namespace media {
namespace test {

class EncodedDataHelper {
 public:
  EncodedDataHelper(const std::vector<uint8_t>& stream,
                    VideoCodecProfile profile);
  ~EncodedDataHelper();

  // Compute and return the next fragment to be sent to the decoder, starting
  // from the current position in the stream, and advance the current position
  // to after the returned fragment.
  scoped_refptr<DecoderBuffer> GetNextBuffer();
  static bool HasConfigInfo(const uint8_t* data,
                            size_t size,
                            VideoCodecProfile profile);

  void Rewind() { next_pos_to_decode_ = 0; }
  bool AtHeadOfStream() const { return next_pos_to_decode_ == 0; }
  bool ReachEndOfStream() const { return next_pos_to_decode_ == data_.size(); }

  size_t num_skipped_fragments() { return num_skipped_fragments_; }

 private:
  // For h.264.
  scoped_refptr<DecoderBuffer> GetNextFragment();
  // For VP8/9.
  scoped_refptr<DecoderBuffer> GetNextFrame();

  // Helpers for GetBytesForNextFragment above.
  size_t GetBytesForNextNALU(size_t pos);
  bool IsNALHeader(const std::string& data, size_t pos);
  bool LookForSPS(size_t* skipped_fragments_count);

  std::string data_;
  VideoCodecProfile profile_;
  size_t next_pos_to_decode_ = 0;
  size_t num_skipped_fragments_ = 0;
};

}  // namespace test
}  // namespace media
#endif  // MEDIA_GPU_TEST_VIDEO_TEST_HELPERS_H_
