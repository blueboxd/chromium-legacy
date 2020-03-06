// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_BITSTREAM_HELPERS_H_
#define MEDIA_GPU_TEST_BITSTREAM_HELPERS_H_

namespace base {
class UnsafeSharedMemoryRegion;
}

namespace media {

struct BitstreamBufferMetadata;

namespace test {

// This class defines an abstract interface for classes that are interested in
// processing bitstreams (e.g. BitstreamBufferValidator,...).
class BitstreamProcessor {
 public:
  virtual ~BitstreamProcessor() = default;

  // Process the specified bitstream buffer. This can e.g. validate the
  // buffer's contents or calculate the buffer's checksum.
  virtual void ProcessBitstreamBuffer(
      int32_t bitstream_buffer_id,
      const BitstreamBufferMetadata& metadata,
      const base::UnsafeSharedMemoryRegion* shm) = 0;

  // Wait until all currently scheduled bitstream buffers have been processed.
  // Returns whether processing was successful.
  virtual bool WaitUntilDone() = 0;
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_BITSTREAM_HELPERS_H_
