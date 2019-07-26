// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_SLICE_VIDEO_DECODER_H_
#define MEDIA_GPU_V4L2_V4L2_SLICE_VIDEO_DECODER_H_

#include <linux/videodev2.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/v4l2_decode_surface_handler.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/video/supported_video_decoder_config.h"

namespace media {

class AcceleratedVideoDecoder;
class DmabufVideoFramePool;
class V4L2DecodeSurface;

class MEDIA_GPU_EXPORT V4L2SliceVideoDecoder : public VideoDecoder,
                                               public V4L2DecodeSurfaceHandler {
 public:
  // Create V4L2SliceVideoDecoder instance. The success of the creation doesn't
  // ensure V4L2SliceVideoDecoder is available on the device. It will be
  // determined in Initialize().
  static std::unique_ptr<VideoDecoder> Create(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<DmabufVideoFramePool> frame_pool);

  static SupportedVideoDecoderConfigs GetSupportedConfigs();

  // VideoDecoder implementation.
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

  // V4L2DecodeSurfaceHandler implementation.
  scoped_refptr<V4L2DecodeSurface> CreateSurface() override;
  bool SubmitSlice(const scoped_refptr<V4L2DecodeSurface>& dec_surface,
                   const uint8_t* data,
                   size_t size) override;
  void DecodeSurface(
      const scoped_refptr<V4L2DecodeSurface>& dec_surface) override;
  void SurfaceReady(const scoped_refptr<V4L2DecodeSurface>& dec_surface,
                    int32_t bitstream_id,
                    const gfx::Rect& visible_rect,
                    const VideoColorSpace& /* color_space */) override;

 private:
  friend class V4L2SliceVideoDecoderTest;

  V4L2SliceVideoDecoder(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      scoped_refptr<V4L2Device> device,
      std::unique_ptr<DmabufVideoFramePool> frame_pool);
  ~V4L2SliceVideoDecoder() override;
  void Destroy() override;

  // Request for decoding buffer. Every Decode() call generates 1 DecodeRequest.
  struct DecodeRequest {
    // The decode buffer passed from Decode().
    scoped_refptr<DecoderBuffer> buffer;
    // The callback function passed from Decode().
    DecodeCB decode_cb;
    // The identifier for the decoder buffer.
    int32_t bitstream_id;

    DecodeRequest(scoped_refptr<DecoderBuffer> buf, DecodeCB cb, int32_t id)
        : buffer(std::move(buf)), decode_cb(std::move(cb)), bitstream_id(id) {}

    // Allow move, but not copy
    DecodeRequest(DecodeRequest&&) = default;
    DecodeRequest& operator=(DecodeRequest&&) = default;

    DISALLOW_COPY_AND_ASSIGN(DecodeRequest);
  };

  // Request for displaying the surface or calling the decode callback.
  struct OutputRequest;

  enum class State {
    // Initial state. Transitions to |kDecoding| if Initialize() is successful,
    // |kError| otherwise.
    kUninitialized,
    // Transitions to |kFlushing| when flushing or changing resolution,
    // |kError| if any unexpected error occurs.
    kDecoding,
    // Transitions to |kDecoding| when flushing or changing resolution is
    // finished. Do not process new input buffer in this state.
    kFlushing,
    // Error state. Cannot transition to other state anymore.
    kError,
  };

  // The reason the decoding is paused.
  enum class PauseReason {
    // Not stopped, decoding normally.
    kNone,
    // Cannot create a new V4L2 surface. Waiting for surfaces to be released.
    kRanOutOfSurfaces,
    // A VP9 superframe contains multiple subframes. Before decoding the next
    // subframe, we need to wait for previous subframes decoded and update the
    // context.
    kWaitSubFrameDecoded,
  };

  // Initialize on decoder thread.
  void InitializeTask(const VideoDecoderConfig& config,
                      InitCB init_cb,
                      const OutputCB& output_cb);
  // Setup format for input queue.
  bool SetupInputFormat(uint32_t input_format_fourcc);

  // Call VIDIOC_S_FMT with |format_fourcc| and |size|. Returns v4l2_format
  // returned by VIDIOC_S_FMT on success, otherwise returns base::nullopt.
  // This should be called only from SetupOutputFormat().
  base::Optional<struct v4l2_format> SetFormatOnOutputQueue(
      uint32_t format_fourcc,
      const gfx::Size& size);
  // Setup format for output queue. This function sets output format on output
  // queue that is supported by a v4l2 driver, can be allocatable by
  // VideoFramePool and can be composited by chrome. This also updates format
  // in VideoFramePool. The returned VideoFrameLayout is one of VideoFrame that
  // VideoFramePool will allocate. Returns base::nullopt on failure of if there
  // is no format that satisfies the above conditions.
  base::Optional<VideoFrameLayout> SetupOutputFormat(
      const gfx::Size& size,
      const gfx::Rect& visible_rect);
  // Update the format of frames in |frame_pool_| with |output_format_fourcc|,
  // |size| and |visible_rect|.
  base::Optional<VideoFrameLayout> UpdateVideoFramePoolFormat(
      uint32_t output_format_fourcc,
      const gfx::Size& size,
      const gfx::Rect& visible_rect);

  // Destroy on decoder thread.
  void DestroyTask();
  // Reset on decoder thread.
  void ResetTask(base::OnceClosure closure);
  // Clear all pending requests, and call all pending decode callback with
  // |status| argument.
  void ClearPendingRequests(DecodeStatus status);

  // Enqueue |request| to the pending decode request queue, and try to decode
  // from the queue.
  void EnqueueDecodeTask(DecodeRequest request);
  // Try to decode buffer from the pending decode request queue.
  // This method stops decoding when:
  // - Run out of surface
  // - Flushing or changing resolution
  // Invoke this method again when these situation ends.
  void PumpDecodeTask();
  // Try to output surface from |output_request_queue_|.
  // This method stops outputting surface when the first surface is not dequeued
  // from the V4L2 device. Invoke this method again when any surface is
  // dequeued from the V4L2 device.
  void PumpOutputSurfaces();
  // Setup the format of V4L2 output buffer, and allocate new buffer set.
  bool ChangeResolution();
  // Callback which is called when V4L2 surface is destroyed.
  void ReuseOutputBuffer(V4L2ReadableBufferRef buffer);

  // Start streaming V4L2 input and output queues. Attempt to start
  // |device_poll_thread_| before starting streaming.
  bool StartStreamV4L2Queue();
  // Stop streaming V4L2 input and output queues. Stop |device_poll_thread_|
  // before stopping streaming.
  bool StopStreamV4L2Queue();
  // Schedule poll if we have any buffers queued and the poll thread is not
  // stopped (on surface set change).
  void SchedulePollTaskIfNeeded();
  // Ran on device_poll_thread_ to wait for device events.
  void DevicePollTask();
  // Try to dequeue input and output buffers from device.
  void ServiceDeviceTask();

  // Get the next bitsream ID.
  int32_t GetNextBitstreamId();
  // Convert the frame and call the output callback.
  void RunOutputCB(scoped_refptr<VideoFrame> frame,
                   const gfx::Rect& visible_rect,
                   base::TimeDelta timestamp);
  // Call the decode callback and count the number of pending callbacks.
  void RunDecodeCB(DecodeCB cb, DecodeStatus status);
  // Change the state and check the state transition is valid.
  void SetState(State new_state);

  // V4L2 device in use.
  scoped_refptr<V4L2Device> device_;
  // VideoFrame manager used to allocate and recycle video frame.
  std::unique_ptr<DmabufVideoFramePool> frame_pool_;
  // Video decoder used to parse stream headers by software.
  std::unique_ptr<AcceleratedVideoDecoder> avd_;

  // Client task runner. All public methods of VideoDecoder interface are
  // executed at this task runner.
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  // Thread to communicate with the device on. Most of internal methods and data
  // members are manipulated on this thread.
  const scoped_refptr<base::SequencedTaskRunner> decoder_task_runner_;

  // Thread used to poll the device for events.
  base::Thread device_poll_thread_;

  // State of the instance.
  State state_ = State::kUninitialized;
  // Indicates why decoding is currently paused.
  PauseReason pause_reason_ = PauseReason::kNone;

  // Parameters for generating output VideoFrame.
  base::Optional<VideoFrameLayout> frame_layout_;
  // Ratio of natural_size to visible_rect of the output frame.
  double pixel_aspect_ratio_ = 0.0;

  // Callbacks passed from Initialize().
  OutputCB output_cb_;
  // Callbacks of EOS buffer passed from Decode().
  DecodeCB flush_cb_;

  // V4L2 input and output queue.
  scoped_refptr<V4L2Queue> input_queue_;
  scoped_refptr<V4L2Queue> output_queue_;

  // The mapping between bitstream id and the timestamp.
  std::map<int32_t, base::TimeDelta> bitstream_id_to_timestamp_;

  // Queue of pending decode request.
  base::queue<DecodeRequest> decode_request_queue_;
  // Surfaces enqueued to V4L2 device. Since we are stateless, they are
  // guaranteed to be proceeded in FIFO order.
  base::queue<scoped_refptr<V4L2DecodeSurface>> surfaces_at_device_;
  // The decode request which is currently processed.
  base::Optional<DecodeRequest> current_decode_request_;
  // Queue of pending output request.
  base::queue<OutputRequest> output_request_queue_;

  // The number of planes, which is the number of DMA-buf fds we enqueue into
  // the V4L2 device.
  size_t num_output_planes_ = 0;
  // True if the decoder needs bitstream conversion before decoding.
  bool needs_bitstream_conversion_ = false;
  // Next bitstream ID.
  int32_t next_bitstream_buffer_id_ = 0;

  SEQUENCE_CHECKER(client_sequence_checker_);
  SEQUENCE_CHECKER(decoder_sequence_checker_);

  // |weak_this_| must be dereferenced and invalidated on
  // |decoder_task_runner_|.
  base::WeakPtr<V4L2SliceVideoDecoder> weak_this_;
  base::WeakPtrFactory<V4L2SliceVideoDecoder> weak_this_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_SLICE_VIDEO_DECODER_H_
