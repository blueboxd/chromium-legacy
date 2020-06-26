// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_DECODER_BROKER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_DECODER_BROKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "media/base/decode_status.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

// Implementation detail of VideoDecoderBroker. Helps safely perform decoder
// tasks on the media thread.
class MediaVideoTaskWrapper;

// Client interface for MediaVideoTaskWrapper. Implementation detail of
// VideoDecoderBroker, but we need to define it here to implement it below.
class CrossThreadVideoDecoderClient {
 public:
  virtual void OnDecodeOutput(scoped_refptr<media::VideoFrame> frame,
                              bool can_read_without_stalling) = 0;
};

// This class brokers the connection between WebCodecs and an underlying
// media::VideoDecoder. It abstracts away details of construction and selection
// of the media/ decoder. It also handles thread-hopping as required by
// underlying APIS.
//
// A new underlying decoder is selected anytime Initialize() is called.
// TODO(chcunningham): Elide re-selection if the config has not significantly
// changed.
//
// All API calls and callbacks must occur on the main thread.
class MODULES_EXPORT VideoDecoderBroker : public media::VideoDecoder,
                                          public CrossThreadVideoDecoderClient {
 public:
  static constexpr char kDefaultDisplayName[] = "EmptyWebCodecsVideoDecoder";

  struct DecoderDetails {
    std::string display_name;
    bool is_platform_decoder;
    bool needs_bitstream_conversion;
    int max_decode_requests;
  };

  // |gpu_factories| may be null when GPU accelerated decoding is not available.
  explicit VideoDecoderBroker(
      ExecutionContext& execution_context,
      media::GpuVideoAcceleratorFactories* gpu_factories);
  ~VideoDecoderBroker() override;

  // Disallow copy and assign.
  VideoDecoderBroker(const VideoDecoderBroker&) = delete;
  VideoDecoderBroker& operator=(const VideoDecoderBroker&) = delete;

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
  bool IsPlatformDecoder() const override;
  void Initialize(const media::VideoDecoderConfig& config,
                  bool low_delay,
                  media::CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const media::WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<media::DecoderBuffer> buffer,
              DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;

 private:
  void OnInitialize(InitCB init_cb,
                    media::Status status,
                    base::Optional<DecoderDetails> details);
  void OnDecodeDone(DecodeCB decode_cb, media::DecodeStatus status);
  void OnReset(base::OnceClosure reset_cb);

  // MediaVideoTaskWrapper::CrossThreadVideoDecoderClient
  void OnDecodeOutput(scoped_refptr<media::VideoFrame> frame,
                      bool can_read_without_stalling) override;

  // When media::GpuVideoAcceleratorFactories is provided, its API requires
  // that we use its TaskRunner (the media thread). When not provided, this task
  // runner will still be used to reduce contention on the main thread.
  // TODO(chcunningham): Try to eliminate the Post(). Most of the
  // underlying::VideoDecoders already offload their work, so this just adds
  // overhead.
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // Owner of state and methods to be used on media_task_runner_;
  std::unique_ptr<MediaVideoTaskWrapper> media_tasks_;

  // Display name for current underlying decoder. Will be kDefaultDisplayName
  // if no decoder is currently initialized.
  std::string display_name_ = kDefaultDisplayName;

  // Wrapper state for GetDisplayName(), IsPlatformDecoder() and others.
  base::Optional<DecoderDetails> decoder_details_;

  // Set to match the underlying decoder's answer at every OnDecodeOutput().
  bool can_read_without_stalling_ = true;

  // OutputCB saved from last call to Initialize().
  OutputCB output_cb_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<VideoDecoderBroker> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_DECODER_BROKER_H_
