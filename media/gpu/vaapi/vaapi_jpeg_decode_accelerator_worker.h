// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODE_ACCELERATOR_WORKER_H_
#define MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODE_ACCELERATOR_WORKER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/ipc/service/image_decode_accelerator_worker.h"

namespace base {
class SequencedTaskRunner;
}

namespace gfx {
class Size;
}

namespace media {

class VaapiJpegDecoder;

// This class uses the VAAPI to provide JPEG decode acceleration. The
// interaction with the VAAPI is done on |decoder_task_runner_|. Objects of this
// class can be created/destroyed on any thread, and the public interface of
// this class is thread-safe.
class VaapiJpegDecodeAcceleratorWorker
    : public gpu::ImageDecodeAcceleratorWorker {
 public:
  // Creates a VaapiJpegDecodeAcceleratorWorker and attempts to initialize the
  // internal state. Returns nullptr if initialization fails.
  static std::unique_ptr<VaapiJpegDecodeAcceleratorWorker> Create();

  ~VaapiJpegDecodeAcceleratorWorker() override;

  // gpu::ImageDecodeAcceleratorWorker implementation.
  std::vector<gpu::ImageDecodeAcceleratorSupportedProfile>
  GetSupportedProfiles() override;
  void Decode(std::vector<uint8_t> encoded_data,
              const gfx::Size& output_size,
              CompletedDecodeCB decode_cb) override;

 private:
  explicit VaapiJpegDecodeAcceleratorWorker(
      std::unique_ptr<VaapiJpegDecoder> decoder);

  // We delegate the decoding to |decoder_| which is constructed on the ctor and
  // then used and destroyed on |decoder_task_runner_| (unless initialization
  // failed, in which case it doesn't matter where it's destroyed since no tasks
  // using |decoder_| should have been posted to |decoder_task_runner_|).
  std::unique_ptr<VaapiJpegDecoder> decoder_;
  scoped_refptr<base::SequencedTaskRunner> decoder_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(VaapiJpegDecodeAcceleratorWorker);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODE_ACCELERATOR_WORKER_H_
