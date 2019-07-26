// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_LINUX_PLATFORM_VIDEO_FRAME_POOL_H_
#define MEDIA_GPU_LINUX_PLATFORM_VIDEO_FRAME_POOL_H_

#include <stddef.h>
#include <vector>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/linux/dmabuf_video_frame_pool.h"
#include "media/gpu/media_gpu_export.h"

namespace base {
class TickClock;
}

namespace media {

// Simple VideoFrame pool used to avoid unnecessarily allocating and destroying
// VideoFrame objects. The pool manages the memory for the VideoFrame
// returned by GetFrame(). When one of these VideoFrames is destroyed,
// the memory is returned to the pool for use by a subsequent GetFrame()
// call. The memory in the pool is retained for the life of the
// PlatformVideoFramePool object. Before calling GetFrame(), the client should
// call NegotiateFrameFormat(). If the parameters passed to
// NegotiateFrameFormat() are changed, then the memory used by frames with the
// old parameter values will be purged from the pool. Frames which are not used
// for a certain period will be purged.
class MEDIA_GPU_EXPORT PlatformVideoFramePool : public DmabufVideoFramePool {
 public:
  using DmabufId = const std::vector<base::ScopedFD>*;

  PlatformVideoFramePool();
  ~PlatformVideoFramePool() override;

  // VideoFramePoolBase Implementation.
  void set_parent_task_runner(
      scoped_refptr<base::SequencedTaskRunner> parent_task_runner) override;
  void SetMaxNumFrames(size_t max_num_frames) override;
  base::Optional<VideoFrameLayout> NegotiateFrameFormat(
      const VideoFrameLayout& layout,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size) override;
  scoped_refptr<VideoFrame> GetFrame() override;
  bool IsExhausted() override;
  VideoFrame* UnwrapFrame(const VideoFrame& wrapped_frame) override;
  void NotifyWhenFrameAvailable(base::OnceClosure cb) override;

 private:
  friend class PlatformVideoFramePoolTest;

  using CreateFrameCB = base::RepeatingCallback<scoped_refptr<VideoFrame>(
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp)>;

  // Used to store free frame and the last used time.
  struct FrameEntry;

  // Allows injection of create frame callback and base::SimpleTestClock.
  // This is used to test the behavior of the video frame pool.
  PlatformVideoFramePool(CreateFrameCB cb, const base::TickClock* tick_clock);

  // Returns the number of frames in the pool for testing purposes.
  size_t GetPoolSizeForTesting();

  // Thunk to post OnFrameReleased() to |task_runner|.
  // Because this thunk may be called in any thread, We don't want to
  // dereference WeakPtr. Therefore we wrap the WeakPtr by base::Optional to
  // avoid the task runner defererencing the WeakPtr.
  static void OnFrameReleasedThunk(
      base::Optional<base::WeakPtr<PlatformVideoFramePool>> pool,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      scoped_refptr<VideoFrame> origin_frame);
  // Called when a wrapped frame gets destroyed.
  // When returning a frame to the pool, the pool might have already been
  // destroyed. In this case, the WeakPtr of the pool will have been invalidated
  // at |parent_task_runner_|, and OnFrameReleased() will not get executed.
  void OnFrameReleased(scoped_refptr<VideoFrame> origin_frame);

  void InsertFreeFrame_Locked(scoped_refptr<VideoFrame> frame)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  size_t GetTotalNumFrames_Locked() const EXCLUSIVE_LOCKS_REQUIRED(lock_);
  bool IsSameLayout_Locked(const VideoFrameLayout& layout) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  bool IsExhausted_Locked() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Purges the stale frame. This method is executed periodically on
  // |parent_task_runner_|.
  void PurgeStaleFrames();

  // The function used to allocate new frames.
  const CreateFrameCB create_frame_cb_;

  // |tick_clock_| is always a DefaultTickClock outside of testing.
  const base::TickClock* tick_clock_;

  // Lock to protect all data members.
  // Every public method and OnFrameReleased() should acquire this lock.
  base::Lock lock_;

  // The arguments of current frame. We allocate new frames only if a pixel
  // format or coded size in |frame_layout_| is changed. When GetFrame() is
  // called, we update |visible_rect_| and |natural_size_| of wrapped frames.
  base::Optional<VideoFrameLayout> frame_layout_ GUARDED_BY(lock_);
  gfx::Rect visible_rect_ GUARDED_BY(lock_);
  gfx::Size natural_size_ GUARDED_BY(lock_);

  // The pool of free frames. The layout of all the frames in |free_frames_|
  // should be the same as |format_| and |coded_size_|.
  base::circular_deque<FrameEntry> free_frames_ GUARDED_BY(lock_);
  // Mapping from the unique_id of the wrapped frame to the original frame.
  std::map<DmabufId, VideoFrame*> frames_in_use_ GUARDED_BY(lock_);

  // The maximum number of frames created by the pool.
  size_t max_num_frames_ GUARDED_BY(lock_);

  // Callback which is called when the pool is not exhausted.
  base::OnceClosure frame_available_cb_ GUARDED_BY(lock_);

  // The weak pointer of this, bound at |parent_task_runner_|.
  // Used at the VideoFrame destruction callback.
  base::WeakPtr<PlatformVideoFramePool> weak_this_;
  base::WeakPtrFactory<PlatformVideoFramePool> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(PlatformVideoFramePool);
};

}  // namespace media
#endif  // MEDIA_GPU_LINUX_PLATFORM_VIDEO_FRAME_POOL_H_
