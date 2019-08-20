// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_LINUX_MAILBOX_VIDEO_FRAME_CONVERTER_H_
#define MEDIA_GPU_LINUX_MAILBOX_VIDEO_FRAME_CONVERTER_H_

#include <map>

#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/video_frame_converter.h"

namespace media {

// The linux VideoDecoder implementations request DMA-buf VideoFrame from the
// DmabufVideoFramePool, and store the decoded data into DMA-buf. However the
// client of the VideoDecoder may only accept mailbox VideoFrame.
// This class is used for converting DMA-buf VideoFrame to mailbox VideoFrame.
// After conversion, the mailbox VideoFrame will retain a reference of the
// VideoFrame passed to ConvertFrame().
class MEDIA_GPU_EXPORT MailboxVideoFrameConverter : public VideoFrameConverter {
 public:
  using UnwrapFrameCB =
      base::RepeatingCallback<VideoFrame*(const VideoFrame& wrapped_frame)>;
  using GetCommandBufferStubCB = base::OnceCallback<gpu::CommandBufferStub*()>;

  // Create a MailboxVideoFrameConverter instance. Return nullptr if any
  // argument is invalid.
  static std::unique_ptr<VideoFrameConverter> Create(
      UnwrapFrameCB unwrap_frame_cb,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      GetCommandBufferStubCB get_stub_cb);

  // Convert DMA-buf VideoFrame to mailbox VideoFrame.
  // For each frame, we bind DMA-buf to GL texture and create mailbox on the GPU
  // main thread.
  // The mailbox of each frame will be stored at |mailbox_table_|. When
  // converting a frame second time, we just lookup the table instead of
  // creating texture and mailbox.
  void ConvertFrame(scoped_refptr<VideoFrame> frame) override;
  void AbortPendingFrames() override;
  bool HasPendingFrames() const override;

 private:
  // In order to recycle VideoFrame, the DmabufVideoFramePool implementation may
  // wrap the frame. We want to create texture only once for the same buffer, so
  // we need to get the original frame at ConvertFrame(). |unwrap_frame_cb| is
  // the callback used to get the original frame.
  // |gpu_task_runner| is the task runner of the GPU main thread. We generate
  // mailbox on it.
  // |get_stub_cb| is the callback used to get the CommandBufferStub, which is
  // used to create CommandBufferHelper.
  MailboxVideoFrameConverter(
      UnwrapFrameCB unwrap_frame_cb,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      GetCommandBufferStubCB get_stub_cb);
  // Destructor runs on the GPU main thread.
  ~MailboxVideoFrameConverter() override;
  void Destroy() override;
  void DestroyOnGPUThread();

  bool CreateCommandBufferHelper();

  // Try to convert frames in |input_frame_queue_| and output the converted
  // frames to client.
  void TryOutputFrames();

  // Generate mailbox for the DMA-buf VideoFrame. This method runs on the GPU
  // main thread.
  // |origin_frame| is unwrapped from |frame| passed from ConvertFrame().
  // |frame| is passed only for keeping |origin_frame| alive.
  void GenerateMailbox(VideoFrame* origin_frame,
                       scoped_refptr<VideoFrame> frame);

  // Register the mapping between DMA-buf VideoFrame and the mailbox.
  // |frame| is passed only for keeping |origin_frame| alive.
  void RegisterMailbox(VideoFrame* origin_frame,
                       const gpu::Mailbox& mailbox,
                       scoped_refptr<VideoFrame> frame);

  // Thunk for calling UnregisterMailbox() on |task_runner|.
  // Because this thunk may be called in any thread, We cannot dereference
  // WeakPtr. Therefore we wrap the WeakPtr by base::Optional to avoid task
  // runner defererence the WeakPtr.
  static void UnregisterMailboxThunk(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::Optional<base::WeakPtr<MailboxVideoFrameConverter>> converter,
      const int origin_frame_id);
  // Remove the mapping between the frame whose unique id is |origin_frame_id|
  // and the mailbox.
  void UnregisterMailbox(const int origin_frame_id);

  // Invoked when any error occurs. |msg| is the error message.
  void OnError(const std::string& msg);

  // In DmabufVideoFramePool, we recycle the unused frames. To do that, each
  // time a frame is requested from the pool it is wrapped inside another frame.
  // A destruction callback is then added to this wrapped frame to automatically
  // return it to the pool upon destruction. Unfortunately this means that a new
  // frame is returned each time, and we need a way to uniquely identify the
  // underlying frame to avoid converting the same frame multiple times.
  // |unwrap_frame_cb_| is used to get the origin frame.
  UnwrapFrameCB unwrap_frame_cb_;

  const scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  GetCommandBufferStubCB get_stub_cb_;
  // The interface to communicate with command buffer. We use this to create and
  // destroy texture, wait for SyncToken, and generate mailbox.
  scoped_refptr<CommandBufferHelper> command_buffer_helper_;

  // Mapping from the unique id of the frame to its corresponding mailbox.
  // Accessed only on |parent_task_runner_|.
  std::map<int, gpu::Mailbox> mailbox_table_;

  // The queue of input frames and the unique_id of their origin frame.
  // Accessed only on |parent_task_runner_|.
  base::queue<std::pair<scoped_refptr<VideoFrame>, int>> input_frame_queue_;

  // The weak pointer of this, bound to |parent_task_runner_|.
  // Used at the VideoFrame destruction callback.
  base::WeakPtr<MailboxVideoFrameConverter> parent_weak_this_;
  // The weak pointer of this, bound to |gpu_task_runner_|.
  // Used to generate mailbox on the GPU main thread.
  base::WeakPtr<MailboxVideoFrameConverter> gpu_weak_this_;
  base::WeakPtrFactory<MailboxVideoFrameConverter> parent_weak_this_factory_{
      this};
  base::WeakPtrFactory<MailboxVideoFrameConverter> gpu_weak_this_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MailboxVideoFrameConverter);
};

}  // namespace media
#endif  // MEDIA_GPU_LINUX_MAILBOX_VIDEO_FRAME_CONVERTER_H_
