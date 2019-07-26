// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/linux/platform_video_frame_pool.h"

#include <utility>

#include "base/logging.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/time/default_tick_clock.h"
#include "media/gpu/linux/platform_video_frame_utils.h"
#include "media/gpu/macros.h"

namespace media {

namespace {

// The lifespan for stale frames. If a frame is not used for 10 seconds, then
// drop the frame to reduce memory usage.
constexpr base::TimeDelta kStaleFrameLimit = base::TimeDelta::FromSeconds(10);

// The default maximum number of frames.
constexpr size_t kDefaultMaxNumFrames = 64;

// The default method to create frames.
scoped_refptr<VideoFrame> DefaultCreateFrame(VideoPixelFormat format,
                                             const gfx::Size& coded_size,
                                             const gfx::Rect& visible_rect,
                                             const gfx::Size& natural_size,
                                             base::TimeDelta timestamp) {
  return CreatePlatformVideoFrame(format, coded_size, visible_rect,
                                  natural_size, timestamp,
                                  gfx::BufferUsage::SCANOUT_VDA_WRITE);
}

// Get the identifier of |frame|. Calling this method with the frames backed
// by the same Dmabuf should return the same result.
PlatformVideoFramePool::DmabufId GetDmabufId(const VideoFrame& frame) {
  return &(frame.DmabufFds());
}

}  // namespace

struct PlatformVideoFramePool::FrameEntry {
  base::TimeTicks last_use_time;
  scoped_refptr<VideoFrame> frame;

  FrameEntry(base::TimeTicks time, scoped_refptr<VideoFrame> frame)
      : last_use_time(time), frame(std::move(frame)) {}
  FrameEntry(const FrameEntry& other) {
    last_use_time = other.last_use_time;
    frame = other.frame;
  }
  ~FrameEntry() = default;
};

PlatformVideoFramePool::PlatformVideoFramePool()
    : PlatformVideoFramePool(base::BindRepeating(&DefaultCreateFrame),
                             base::DefaultTickClock::GetInstance()) {}

PlatformVideoFramePool::PlatformVideoFramePool(
    CreateFrameCB cb,
    const base::TickClock* tick_clock)
    : create_frame_cb_(std::move(cb)),
      tick_clock_(tick_clock),
      max_num_frames_(kDefaultMaxNumFrames),
      weak_this_factory_(this) {
  DVLOGF(4);
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

PlatformVideoFramePool::~PlatformVideoFramePool() {
  if (parent_task_runner_)
    DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  frames_in_use_.clear();
  free_frames_.clear();
  weak_this_factory_.InvalidateWeakPtrs();
}

scoped_refptr<VideoFrame> PlatformVideoFramePool::GetFrame() {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  if (!frame_layout_) {
    VLOGF(1) << "Please call NegotiateFrameFormat() first.";
    return nullptr;
  }

  VideoPixelFormat format = frame_layout_->format();
  const gfx::Size& coded_size = frame_layout_->coded_size();
  if (free_frames_.empty()) {
    if (GetTotalNumFrames_Locked() >= max_num_frames_)
      return nullptr;

    // VideoFrame::WrapVideoFrame() will check whether the updated visible_rect
    // is sub rect of the original visible_rect. Therefore we set visible_rect
    // as large as coded_size to guarantee this condition.
    scoped_refptr<VideoFrame> new_frame =
        create_frame_cb_.Run(format, coded_size, gfx::Rect(coded_size),
                             coded_size, base::TimeDelta());
    if (!new_frame)
      return nullptr;

    InsertFreeFrame_Locked(std::move(new_frame));
  }

  DCHECK(!free_frames_.empty());
  scoped_refptr<VideoFrame> origin_frame = std::move(free_frames_.back().frame);
  free_frames_.pop_back();
  DCHECK_EQ(origin_frame->format(), format);
  DCHECK_EQ(origin_frame->coded_size(), coded_size);

  scoped_refptr<VideoFrame> wrapped_frame = VideoFrame::WrapVideoFrame(
      *origin_frame, format, visible_rect_, natural_size_);
  DCHECK(wrapped_frame);
  frames_in_use_.emplace(GetDmabufId(*wrapped_frame), origin_frame.get());
  wrapped_frame->AddDestructionObserver(
      base::BindOnce(&PlatformVideoFramePool::OnFrameReleasedThunk, weak_this_,
                     parent_task_runner_, std::move(origin_frame)));

  // Clear all metadata before returning to client, in case origin frame has any
  // unrelated metadata.
  wrapped_frame->metadata()->Clear();
  return wrapped_frame;
}

void PlatformVideoFramePool::set_parent_task_runner(
    scoped_refptr<base::SequencedTaskRunner> parent_task_runner) {
  DCHECK(!parent_task_runner_);

  parent_task_runner_ = std::move(parent_task_runner);
  parent_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PlatformVideoFramePool::PurgeStaleFrames, weak_this_),
      kStaleFrameLimit);
}

void PlatformVideoFramePool::PurgeStaleFrames() {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  const base::TimeTicks now = tick_clock_->NowTicks();
  while (!free_frames_.empty() &&
         now - free_frames_.front().last_use_time > kStaleFrameLimit) {
    free_frames_.pop_front();
  }

  parent_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PlatformVideoFramePool::PurgeStaleFrames, weak_this_),
      kStaleFrameLimit);
}

void PlatformVideoFramePool::SetMaxNumFrames(size_t max_num_frames) {
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  max_num_frames_ = max_num_frames;

  if (frame_available_cb_ && !IsExhausted_Locked())
    std::move(frame_available_cb_).Run();
}

base::Optional<VideoFrameLayout> PlatformVideoFramePool::NegotiateFrameFormat(
    const VideoFrameLayout& layout,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size) {
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  // If the frame layout changed we need to allocate new frames so we will clear
  // the pool here. If only the visible or natural size changed we don't need to
  // allocate new frames, but will just update the properties of wrapped frames
  // returned by GetFrame().
  // NOTE: It is assumed layout is determined by |format| and |coded_size|.
  if (!IsSameLayout_Locked(layout)) {
    DVLOGF(4) << "The video frame format is changed. Clearing the pool.";
    free_frames_.clear();
  }

  visible_rect_ = visible_rect;
  natural_size_ = natural_size;

  // Create a temporary frame in order to know VideoFrameLayout that VideoFrame
  // that will be allocated in GetFrame() has.
  auto frame =
      create_frame_cb_.Run(layout.format(), layout.coded_size(), visible_rect,
                           natural_size_, base::TimeDelta());
  if (!frame) {
    VLOGF(1) << "Failed to create video frame";
    return base::nullopt;
  }
  frame_layout_ = base::make_optional<VideoFrameLayout>(frame->layout());

  return frame_layout_;
}

bool PlatformVideoFramePool::IsExhausted() {
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  return IsExhausted_Locked();
}

bool PlatformVideoFramePool::IsExhausted_Locked() {
  DVLOGF(4);
  lock_.AssertAcquired();

  return free_frames_.empty() && GetTotalNumFrames_Locked() >= max_num_frames_;
}

VideoFrame* PlatformVideoFramePool::UnwrapFrame(
    const VideoFrame& wrapped_frame) {
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  auto it = frames_in_use_.find(GetDmabufId(wrapped_frame));
  return (it == frames_in_use_.end()) ? nullptr : it->second;
}

void PlatformVideoFramePool::NotifyWhenFrameAvailable(base::OnceClosure cb) {
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  if (!IsExhausted_Locked()) {
    parent_task_runner_->PostTask(FROM_HERE, std::move(cb));
    return;
  }

  frame_available_cb_ = std::move(cb);
}

// static
void PlatformVideoFramePool::OnFrameReleasedThunk(
    base::Optional<base::WeakPtr<PlatformVideoFramePool>> pool,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<VideoFrame> origin_frame) {
  DCHECK(pool);
  DVLOGF(4);

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&PlatformVideoFramePool::OnFrameReleased, *pool,
                                std::move(origin_frame)));
}

void PlatformVideoFramePool::OnFrameReleased(
    scoped_refptr<VideoFrame> origin_frame) {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  DmabufId frame_id = GetDmabufId(*origin_frame);
  auto it = frames_in_use_.find(frame_id);
  DCHECK(it != frames_in_use_.end());
  frames_in_use_.erase(it);

  if (IsSameLayout_Locked(origin_frame->layout())) {
    InsertFreeFrame_Locked(std::move(origin_frame));
  }

  if (frame_available_cb_ && !IsExhausted_Locked())
    std::move(frame_available_cb_).Run();
}

void PlatformVideoFramePool::InsertFreeFrame_Locked(
    scoped_refptr<VideoFrame> frame) {
  DCHECK(frame);
  DVLOGF(4);
  lock_.AssertAcquired();

  if (GetTotalNumFrames_Locked() < max_num_frames_)
    free_frames_.push_back({tick_clock_->NowTicks(), std::move(frame)});
}

size_t PlatformVideoFramePool::GetTotalNumFrames_Locked() const {
  DVLOGF(4);
  lock_.AssertAcquired();

  return free_frames_.size() + frames_in_use_.size();
}

bool PlatformVideoFramePool::IsSameLayout_Locked(
    const VideoFrameLayout& layout) const {
  DVLOGF(4);
  lock_.AssertAcquired();

  return frame_layout_ && frame_layout_->format() == layout.format() &&
         frame_layout_->coded_size() == layout.coded_size();
}

size_t PlatformVideoFramePool::GetPoolSizeForTesting() {
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  return free_frames_.size();
}

}  // namespace media
