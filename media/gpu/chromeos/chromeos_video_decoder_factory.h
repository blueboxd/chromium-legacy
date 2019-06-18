// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_CHROMEOS_VIDEO_DECODER_FACTORY_H_
#define MEDIA_GPU_CHROMEOS_CHROMEOS_VIDEO_DECODER_FACTORY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "media/gpu/media_gpu_export.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace gpu {
class CommandBufferStub;
}

namespace media {

class VideoDecoder;

class MEDIA_GPU_EXPORT ChromeosVideoDecoderFactory {
 public:
  using GetCommandBufferStubCB = base::OnceCallback<gpu::CommandBufferStub*()>;

  // Create VideoDecoder instance that does convert the output VideoFrame
  // to mailbox-backed VideoFrame by CommandBufferHelper.
  // We convert the frame by MailboxVideoFrameConverter. See the description for
  // |get_stub_cb| at MailboxVideoFrameConverter class.
  static std::unique_ptr<VideoDecoder> Create(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      GetCommandBufferStubCB get_stub_cb);

  // Create VideoDecoder instance for tesing. The created VideoDecoder does not
  // convert the output VideoFrame to mailbox-backed VideoFrame.
  static std::unique_ptr<VideoDecoder> CreateForTesting(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner);
};

}  // namespace media
#endif  // MEDIA_GPU_CHROMEOS_CHROMEOS_VIDEO_DECODER_FACTORY_H_
