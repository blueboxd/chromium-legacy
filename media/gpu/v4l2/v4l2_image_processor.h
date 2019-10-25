// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_IMAGE_PROCESSOR_H_
#define MEDIA_GPU_V4L2_V4L2_IMAGE_PROCESSOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include <linux/videodev2.h>

#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Handles image processing accelerators that expose a V4L2 memory-to-memory
// interface. The threading model of this class is the same as for other V4L2
// hardware accelerators (see V4L2VideoDecodeAccelerator) for more details.
class MEDIA_GPU_EXPORT V4L2ImageProcessor : public ImageProcessor {
 public:
  // ImageProcessor implementation.
  ~V4L2ImageProcessor() override;
  bool Reset() override;

  // Returns true if image processing is supported on this platform.
  static bool IsSupported();

  // Returns a vector of supported input formats in fourcc.
  static std::vector<uint32_t> GetSupportedInputFormats();

  // Returns a vector of supported output formats in fourcc.
  static std::vector<uint32_t> GetSupportedOutputFormats();

  // Gets output allocated size and number of planes required by the device
  // for conversion from |input_pixelformat| with |input_size| to
  // |output_pixelformat| with expected |output_size|.
  // On success, returns true with adjusted |output_size| and |num_planes|.
  // On failure, returns false without touching |output_size| and |num_planes|.
  static bool TryOutputFormat(uint32_t input_pixelformat,
                              uint32_t output_pixelformat,
                              const gfx::Size& input_size,
                              gfx::Size* output_size,
                              size_t* num_planes);

  // Factory method to create V4L2ImageProcessor to convert from
  // input_config to output_config. The number of input buffers and output
  // buffers will be |num_buffers|. Provided |error_cb| will be posted to the
  // same thread Create() is called if an error occurs after initialization.
  // Returns nullptr if V4L2ImageProcessor fails to create.
  // Note: output_mode will be removed once all its clients use import mode.
  // TODO(crbug.com/917798): remove |device| parameter once
  //     V4L2VideoDecodeAccelerator no longer creates and uses
  //     |image_processor_device_| before V4L2ImageProcessor is created.
  static std::unique_ptr<V4L2ImageProcessor> Create(
      scoped_refptr<V4L2Device> device,
      const ImageProcessor::PortConfig& input_config,
      const ImageProcessor::PortConfig& output_config,
      const ImageProcessor::OutputMode output_mode,
      size_t num_buffers,
      ErrorCB error_cb);

 private:
  // Job record. Jobs are processed in a FIFO order. |input_frame| will be
  // processed and the result written into |output_frame|. Once processing is
  // complete, |ready_cb| or |legacy_ready_cb| will be called depending on which
  // Process() method has been used to create that JobRecord.
  struct JobRecord {
    JobRecord();
    ~JobRecord();
    scoped_refptr<VideoFrame> input_frame;
    FrameReadyCB ready_cb;
    LegacyFrameReadyCB legacy_ready_cb;
    scoped_refptr<VideoFrame> output_frame;
  };

  V4L2ImageProcessor(scoped_refptr<V4L2Device> device,
                     const ImageProcessor::PortConfig& input_config,
                     const ImageProcessor::PortConfig& output_config,
                     v4l2_memory input_memory_type,
                     v4l2_memory output_memory_type,
                     OutputMode output_mode,
                     size_t num_buffers,
                     ErrorCB error_cb);

  bool Initialize();
  void EnqueueInput(const JobRecord* job_record);
  void EnqueueOutput(const JobRecord* job_record);
  void Dequeue();
  bool EnqueueInputRecord(const JobRecord* job_record);
  bool EnqueueOutputRecord(const JobRecord* job_record);
  bool CreateInputBuffers();
  bool CreateOutputBuffers();

  void V4L2VFDestructionObserver(V4L2ReadableBufferRef buf);

  void NotifyError();

  // ImageProcessor implementation.
  bool ProcessInternal(scoped_refptr<VideoFrame> frame,
                       LegacyFrameReadyCB cb) override;
  bool ProcessInternal(scoped_refptr<VideoFrame> input_frame,
                       scoped_refptr<VideoFrame> output_frame,
                       FrameReadyCB cb) override;

  void ProcessTask(std::unique_ptr<JobRecord> job_record);
  void ProcessJobsTask();
  void ServiceDeviceTask();

  // Allocate/Destroy the input/output V4L2 buffers.
  void AllocateBuffersTask(bool* result, base::WaitableEvent* done);
  void DestroyBuffersTask();

  // Attempt to start/stop device_poll_thread_.
  void StartDevicePoll();
  void StopDevicePoll();

  // Ran on device_poll_thread_ to wait for device events.
  void DevicePollTask(bool poll_device);

  // Stop all processing and clean up. After this method returns no more
  // callbacks will be invoked.
  void Destroy();

  const v4l2_memory input_memory_type_;
  const v4l2_memory output_memory_type_;

  // V4L2 device in use.
  scoped_refptr<V4L2Device> device_;

  // Thread to communicate with the device on.
  base::Thread device_thread_;
  // Thread used to poll the V4L2 for events only.
  base::Thread device_poll_thread_;

  // CancelableTaskTracker for ProcessTask().
  // Because ProcessTask is posted from |client_task_runner_|'s thread to
  // another sequence, |device_thread_|, it is unsafe to cancel the posted tasks
  // from |client_task_runner_|'s thread using CancelableCallback and WeakPtr
  // binding. CancelableTaskTracker is designed to deal with this scenario.
  base::CancelableTaskTracker process_task_tracker_;

  // All the below members are to be accessed from device_thread_ only
  // (if it's running).
  base::queue<std::unique_ptr<JobRecord>> input_job_queue_;
  base::queue<std::unique_ptr<JobRecord>> running_jobs_;

  scoped_refptr<V4L2Queue> input_queue_;
  scoped_refptr<V4L2Queue> output_queue_;

  // The number of input or output buffers.
  const size_t num_buffers_;

  // Error callback to the client.
  ErrorCB error_cb_;

  // Checker for the sequence that creates this V4L2ImageProcessor.
  SEQUENCE_CHECKER(client_sequence_checker_);
  // Checker for the device thread owned by this V4L2ImageProcessor.
  THREAD_CHECKER(device_thread_checker_);

  // Keep at the end so it is destroyed first.
  base::WeakPtrFactory<V4L2ImageProcessor> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(V4L2ImageProcessor);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_IMAGE_PROCESSOR_H_
