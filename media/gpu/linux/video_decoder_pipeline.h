// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_LINUX_VIDEO_DECODER_PIPELINE_H_
#define MEDIA_GPU_LINUX_VIDEO_DECODER_PIPELINE_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/video_frame_converter.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {

class DmabufVideoFramePool;

class MEDIA_GPU_EXPORT VideoDecoderPipeline : public VideoDecoder {
 public:
  // Function signature for creating VideoDecoder.
  using CreateVDFunc = std::unique_ptr<VideoDecoder> (*)(
      scoped_refptr<base::SequencedTaskRunner>,
      scoped_refptr<base::SequencedTaskRunner>,
      base::RepeatingCallback<DmabufVideoFramePool*()>);
  using GetCreateVDFunctionsCB =
      base::RepeatingCallback<base::queue<CreateVDFunc>(CreateVDFunc)>;

  static std::unique_ptr<VideoDecoder> Create(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<DmabufVideoFramePool> frame_pool,
      std::unique_ptr<VideoFrameConverter> frame_converter,
      GetCreateVDFunctionsCB get_create_vd_functions_cb);

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
  // Get a list of the available functions for creating VideoDeocoder.
  static base::queue<CreateVDFunc> GetCreateVDFunctions(
      CreateVDFunc current_func);

  VideoDecoderPipeline(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<DmabufVideoFramePool> frame_pool,
      std::unique_ptr<VideoFrameConverter> frame_converter,
      GetCreateVDFunctionsCB get_create_vd_functions_cb);
  void Destroy() override;
  void DestroyTask();

  void CreateAndInitializeVD(base::queue<CreateVDFunc> create_vd_funcs,
                             VideoDecoderConfig config,
                             bool low_delay,
                             CdmContext* cdm_context,
                             WaitingCB waiting_cb);
  void OnInitializeDone(base::queue<CreateVDFunc> create_vd_funcs,
                        VideoDecoderConfig config,
                        bool low_delay,
                        CdmContext* cdm_context,
                        WaitingCB waiting_cb,
                        bool success);

  void OnDecodeDone(bool eos_buffer, DecodeCB decode_cb, DecodeStatus status);
  void OnResetDone();
  void OnFrameConverted(scoped_refptr<VideoFrame> frame);
  void OnError(const std::string& msg);

  static void OnFrameDecodedThunk(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::Optional<base::WeakPtr<VideoDecoderPipeline>> pipeline,
      scoped_refptr<VideoFrame> frame);
  void OnFrameDecoded(scoped_refptr<VideoFrame> frame);

  // Call |client_flush_cb_| with |status| if we need.
  void CallFlushCbIfNeeded(DecodeStatus status);

  // Get the video frame pool without passing the ownership.
  DmabufVideoFramePool* GetVideoFramePool() const;

  // The client task runner and its sequence checker. All public methods should
  // run on this task runner.
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  SEQUENCE_CHECKER(client_sequence_checker_);

  // The decoder task runner and its sequence checker. |decoder_| should post
  // time-consuming task and call |frame_pool_|'s methods on this task runner.
  const scoped_refptr<base::SequencedTaskRunner> decoder_task_runner_;
  SEQUENCE_CHECKER(decoder_sequence_checker_);

  // The frame pool passed from the client. Destroyed on |decoder_task_runner_|.
  std::unique_ptr<DmabufVideoFramePool> frame_pool_;
  // The frame converter passed from the client. Destroyed on
  // |client_task_runner_|.
  std::unique_ptr<VideoFrameConverter> frame_converter_;

  // The callback to get a list of function for creating VideoDecoder.
  GetCreateVDFunctionsCB get_create_vd_functions_cb_;

  // The current video decoder implementation. Valid after initialization is
  // successfully done.
  std::unique_ptr<VideoDecoder> decoder_;
  // The create function of |decoder_|. nullptr iff |decoder_| is nullptr.
  CreateVDFunc used_create_vd_func_ = nullptr;

  // Callback from the client. These callback are called on
  // |client_task_runner_|.
  InitCB init_cb_;
  OutputCB client_output_cb_;
  DecodeCB client_flush_cb_;
  base::OnceClosure client_reset_cb_;

  // Set to true when any unexpected error occurs.
  bool has_error_ = false;

  // The weak pointer of this, bound to |client_task_runner_|.
  base::WeakPtr<VideoDecoderPipeline> weak_this_;
  base::WeakPtrFactory<VideoDecoderPipeline> weak_this_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_LINUX_VIDEO_DECODER_PIPELINE_H_
