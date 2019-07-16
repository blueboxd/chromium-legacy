// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_LINUX_VIDEO_DECODER_PIPELINE_H_
#define MEDIA_GPU_LINUX_VIDEO_DECODER_PIPELINE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "media/base/video_decoder.h"
#include "media/gpu/media_gpu_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {

class VideoFrameConverter;

class MEDIA_GPU_EXPORT VideoDecoderPipeline : public VideoDecoder {
 public:
  VideoDecoderPipeline(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<VideoDecoder> decoder,
      std::unique_ptr<VideoFrameConverter> frame_converter);
  ~VideoDecoderPipeline() override;

  // VideoDecoder implementation
  std::string GetDisplayName() const override;
  bool IsPlatformDecoder() const override;
  int GetMaxDecodeRequests() const override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;

  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Reset(base::OnceClosure closure) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;

 private:
  void Destroy() override;

  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_;

  const std::unique_ptr<VideoDecoder> decoder_;
  const std::unique_ptr<VideoFrameConverter> frame_converter_;

  static void OnFrameDecodedThunk(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::Optional<base::WeakPtr<VideoDecoderPipeline>> pipeline,
      scoped_refptr<VideoFrame> frame);
  void OnFrameDecoded(scoped_refptr<VideoFrame> frame);

  OutputCB client_output_cb_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<VideoDecoderPipeline> weak_this_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_LINUX_VIDEO_DECODER_PIPELINE_H_
