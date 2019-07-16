// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODER_H_
#define MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODER_H_

#include <stdint.h>
#include <va/va.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/threading/thread.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/decode_surface_handler.h"
#include "media/video/supported_video_decoder_config.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class AcceleratedVideoDecoder;
class DmabufVideoFramePool;
class VaapiWrapper;
class VideoFrame;
class VASurface;

class VaapiVideoDecoder : public media::VideoDecoder,
                          public DecodeSurfaceHandler<VASurface> {
 public:
  static std::unique_ptr<VideoDecoder> Create(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<DmabufVideoFramePool> frame_pool);

  static SupportedVideoDecoderConfigs GetSupportedConfigs();

  // media::VideoDecoder implementation.
  std::string GetDisplayName() const override;
  bool IsPlatformDecoder() const override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;

  // DecodeSurfaceHandler<VASurface> implementation.
  scoped_refptr<VASurface> CreateSurface() override;
  void SurfaceReady(const scoped_refptr<VASurface>& va_surface,
                    int32_t buffer_id,
                    const gfx::Rect& visible_rect,
                    const VideoColorSpace& color_space) override;

 private:
  // Decode task holding single decode request.
  struct DecodeTask {
    DecodeTask(scoped_refptr<DecoderBuffer> buffer,
               int32_t buffer_id,
               DecodeCB decode_done_cb);
    ~DecodeTask();
    DecodeTask(DecodeTask&&);
    DecodeTask& operator=(DecodeTask&&) = default;
    scoped_refptr<DecoderBuffer> buffer_;
    int32_t buffer_id_ = -1;
    DecodeCB decode_done_cb_;
    DISALLOW_COPY_AND_ASSIGN(DecodeTask);
  };

  enum class State {
    kUninitialized,     // not initialized yet or initialization failed.
    kWaitingForInput,   // waiting for input buffers.
    kWaitingForOutput,  // waiting for output buffers.
    kDecoding,          // decoding buffers.
    kResetting,         // resetting decoder.
    kError,             // decoder encountered an error.
  };

  VaapiVideoDecoder(scoped_refptr<base::SequencedTaskRunner> client_task_runner,
                    std::unique_ptr<DmabufVideoFramePool> frame_pool);
  ~VaapiVideoDecoder() override;

  // Destroy the VAAPIVideoDecoder, aborts pending decode requests and blocks
  // until destroyed.
  void Destroy() override;

  // Initialize the VAAPI video decoder on the decoder thread.
  void InitializeTask(const VideoDecoderConfig& config,
                      InitCB init_cb,
                      OutputCB output_cb);
  // Destroy the VAAPI video decoder on the decoder thread.
  void DestroyTask();

  // Queue a decode task on the decoder thread. If the decoder is currently
  // waiting for input buffers decoding will be started.
  void QueueDecodeTask(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb);
  // Schedule the next decode task in the queue to be executed.
  void ScheduleNextDecodeTask();
  // Try to decode a single input buffer on the decoder thread.
  void HandleDecodeTask();
  // Clear the decode task queue on the decoder thread. This is done when
  // resetting or destroying the decoder, or encountering an error.
  void ClearDecodeTaskQueue(DecodeStatus status);

  // Output a single |video_frame| on the decoder thread.
  void OutputFrameTask(scoped_refptr<VideoFrame> video_frame,
                       base::TimeDelta timestamp);
  // Called when a different output frame resolution is requested on the decoder
  // thread. This happens when either decoding just started or a resolution
  // change occurred in the video stream.
  void ChangeFrameResolutionTask();
  // Release the video frame associated with the specified |surface_id| on the
  // decoder thread. This is called when the last reference to the associated
  // VASurface has been released, which happens when the decoder outputted the
  // video frame, or stopped using it as a reference frame. Note that this
  // doesn't mean the frame can be reused immediately, as it might still be used
  // by the client.
  void ReleaseFrameTask(scoped_refptr<VASurface> va_surface,
                        VASurfaceID surface_id);
  // Called when a video frame was returned to the video frame pool on the
  // decoder thread. This will happen when both the client and decoder
  // (in ReleaseFrameTask()) released their reference to the video frame.
  void NotifyFrameAvailableTask();

  // Flush the decoder on the decoder thread, blocks until all pending decode
  // tasks have been executed and all frames have been output.
  void FlushTask();

  // Reset the decoder on the decoder thread. This will abort any pending decode
  // task. The |reset_cb| will be passed to ResetDoneTask() and called when
  // resetting has completed.
  void ResetTask(base::OnceClosure reset_cb);
  // Called on the decoder thread once resetting is done. Executes |reset_cb|.
  void ResetDoneTask(base::OnceClosure reset_cb);

  // Called on decoder thread to schedule |decode_cb| on the client task runner.
  void RunDecodeCB(DecodeCB decode_cb, DecodeStatus status);
  // Change the current |state_| to the specified |state| on the decoder thread.
  void SetState(State state);

  // The video decoder's state.
  State state_ = State::kUninitialized;

  // Callback used to notify the client when a frame is available for output.
  OutputCB output_cb_;

  // The video stream's profile.
  VideoCodecProfile profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;
  // True if the decoder needs bitstream conversion before decoding.
  bool needs_bitstream_conversion_ = false;

  // Output frame properties.
  base::Optional<VideoFrameLayout> frame_layout_;
  gfx::Rect visible_rect_;
  // Ratio of natural size to |visible_rect_| of the output frames.
  double pixel_aspect_ratio_ = 0.0;

  // Video frame pool used to allocate and recycle video frames.
  std::unique_ptr<DmabufVideoFramePool> frame_pool_;

  // The mapping between buffer id and the timestamp.
  std::map<int32_t, base::TimeDelta> buffer_id_to_timestamp_;

  // Queue containing all requested decode tasks.
  base::queue<DecodeTask> decode_task_queue_;
  // The decode task we're currently trying to execute.
  base::Optional<DecodeTask> current_decode_task_;
  // The next input buffer id.
  int32_t next_buffer_id_ = 0;

  // The list of frames currently used as output buffers or reference frames.
  std::map<VASurfaceID, scoped_refptr<VideoFrame>> output_frames_;

  // Platform and codec specific video decoder.
  std::unique_ptr<AcceleratedVideoDecoder> decoder_;
  scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  base::Thread decoder_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> decoder_thread_task_runner_;

  SEQUENCE_CHECKER(client_sequence_checker_);
  SEQUENCE_CHECKER(decoder_sequence_checker_);

  base::WeakPtr<VaapiVideoDecoder> weak_this_;
  base::WeakPtrFactory<VaapiVideoDecoder> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(VaapiVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODER_H_
