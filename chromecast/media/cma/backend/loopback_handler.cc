// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/loopback_handler.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "chromecast/public/media/external_audio_pipeline_shlib.h"

namespace chromecast {
namespace media {

namespace {

const int kDefaultBufferSize = 1024 * 2 * sizeof(float);
const int kNumBuffers = 16;
const int kMaxTasks = kNumBuffers + 1;

class LoopbackHandlerImpl : public LoopbackHandler,
                            public CastMediaShlib::LoopbackAudioObserver {
 public:
  LoopbackHandlerImpl(scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : external_audio_pipeline_supported_(
            ExternalAudioPipelineShlib::IsSupported()),
        task_signal_(&lock_) {
    CreateBuffersIfNeeded(kDefaultBufferSize);

    if (task_runner) {
      task_runner_ = std::move(task_runner);
    } else {
      thread_ = std::make_unique<base::Thread>("CMA loopback");
      base::Thread::Options options;
      options.priority = base::ThreadPriority::REALTIME_AUDIO;
      thread_->StartWithOptions(options);
      task_runner_ = thread_->task_runner();

      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&LoopbackHandlerImpl::LoopbackTaskLoop,
                                    base::Unretained(this)));
    }

    if (external_audio_pipeline_supported_) {
      ExternalAudioPipelineShlib::AddExternalLoopbackAudioObserver(this);
    }
  }

 private:
  struct Task {
    Task(int64_t expected_playback_time,
         SampleFormat format,
         int sample_rate,
         int channels,
         uint32_t tag,
         std::unique_ptr<uint8_t[]> data,
         int length)
        : expected_playback_time(expected_playback_time),
          format(format),
          sample_rate(sample_rate),
          channels(channels),
          tag(tag),
          data(std::move(data)),
          length(length) {}

    const int64_t expected_playback_time;
    const SampleFormat format;
    const int sample_rate;
    const int channels;
    const uint32_t tag;
    std::unique_ptr<uint8_t[]> data;
    const int length;
  };

  ~LoopbackHandlerImpl() override {
    {
      base::AutoLock lock(lock_);
      stop_thread_ = true;
    }
    task_signal_.Signal();
    if (thread_) {
      thread_->Stop();
    }
  }

  // LoopbackHandler implementation:
  void Destroy() override {
    if (external_audio_pipeline_supported_) {
      ExternalAudioPipelineShlib::RemoveExternalLoopbackAudioObserver(this);
    } else {
      delete this;
    }
  }

  void SetDataSize(int data_size_bytes) override {
    CreateBuffersIfNeeded(data_size_bytes);
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override {
    return task_runner_;
  }

  void AddObserver(CastMediaShlib::LoopbackAudioObserver* observer) override {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&LoopbackHandlerImpl::AddObserverOnThread,
                                  base::Unretained(this), observer));
    task_signal_.Signal();
  }

  void RemoveObserver(
      CastMediaShlib::LoopbackAudioObserver* observer) override {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&LoopbackHandlerImpl::RemoveObserverOnThread,
                                  base::Unretained(this), observer));
    task_signal_.Signal();
  }

  void SendData(int64_t timestamp,
                int sample_rate,
                int num_channels,
                float* data,
                int frames) override {
    if (external_audio_pipeline_supported_) {
      return;
    }

    SendLoopbackData(timestamp, kSampleFormatF32, sample_rate, num_channels,
                     reinterpret_cast<uint8_t*>(data),
                     frames * num_channels * sizeof(float));
  }

  void SendInterrupt() override {
    base::AutoLock lock(lock_);
    SendInterruptedLocked();
  }

  // CastMediaShlib::LoopbackAudioObserver implementation:
  void OnLoopbackAudio(int64_t timestamp,
                       SampleFormat format,
                       int sample_rate,
                       int num_channels,
                       uint8_t* data,
                       int length) override {
    SendLoopbackData(timestamp, format, sample_rate, num_channels, data,
                     length);
  }

  void OnLoopbackInterrupted() override { SendInterrupt(); }

  void OnRemoved() override {
    // We expect that external pipeline will not invoke any other callbacks
    // after this one.
    delete this;
    // No need to pipe this, LoopbackHandlerImpl will let the other observer
    // know when it's being removed.
  }

  void AddObserverOnThread(CastMediaShlib::LoopbackAudioObserver* observer) {
    DCHECK(task_runner_->BelongsToCurrentThread());
    LOG(INFO) << __func__;
    DCHECK(observer);

    observers_.insert(observer);
  }

  void RemoveObserverOnThread(CastMediaShlib::LoopbackAudioObserver* observer) {
    DCHECK(task_runner_->BelongsToCurrentThread());
    LOG(INFO) << __func__;
    DCHECK(observer);

    observers_.erase(observer);
    observer->OnRemoved();
  }

  void CreateBuffersIfNeeded(int buffer_size_bytes) {
    if (buffer_size_bytes <= buffer_size_) {
      return;
    }
    LOG(INFO) << "Create new buffers, size = " << buffer_size_bytes;

    base::AutoLock lock(lock_);
    ++buffer_tag_;
    buffers_.clear();
    for (int i = 0; i < kNumBuffers; ++i) {
      auto buffer = std::make_unique<uint8_t[]>(buffer_size_bytes);
      std::fill_n(buffer.get(), buffer_size_bytes, 0);
      buffers_.push_back(std::move(buffer));
    }
    buffer_size_ = buffer_size_bytes;

    tasks_.reserve(kMaxTasks);
    new_tasks_.reserve(kMaxTasks);
  }

  void SendLoopbackData(int64_t timestamp,
                        SampleFormat format,
                        int sample_rate,
                        int num_channels,
                        uint8_t* data,
                        int length) {
    CreateBuffersIfNeeded(length);

    {
      base::AutoLock lock(lock_);

      if (buffers_.empty() || tasks_.size() >= kNumBuffers) {
        LOG(ERROR) << "Can't send loopback data";
        SendInterruptedLocked();
        return;
      }

      std::unique_ptr<uint8_t[]> buffer = std::move(buffers_.back());
      buffers_.pop_back();

      std::copy(data, data + length, buffer.get());
      tasks_.emplace_back(timestamp, format, sample_rate, num_channels,
                          buffer_tag_, std::move(buffer), length);
    }

    if (thread_) {
      task_signal_.Signal();
    } else {
      HandleLoopbackTask(&tasks_.back());
      tasks_.pop_back();
    }
  }

  void SendInterruptedLocked() {
    lock_.AssertAcquired();

    if (tasks_.size() == kMaxTasks) {
      return;
    }
    tasks_.emplace_back(0, kSampleFormatF32, 0, 0, 0, nullptr, 0);

    if (thread_) {
      task_signal_.Signal();
    } else {
      HandleLoopbackTask(&tasks_.back());
      tasks_.pop_back();
    }
  }

  void LoopbackTaskLoop() {
    DCHECK(task_runner_->BelongsToCurrentThread());

    {
      base::AutoLock lock(lock_);
      if (stop_thread_)
        return;
      if (tasks_.empty()) {
        task_signal_.Wait();
      }
      new_tasks_.swap(tasks_);
    }

    for (auto& task : new_tasks_) {
      HandleLoopbackTask(&task);
    }
    new_tasks_.clear();

    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&LoopbackHandlerImpl::LoopbackTaskLoop,
                                  base::Unretained(this)));
  }

  void HandleLoopbackTask(Task* task) {
    if (!task->data) {
      for (auto* observer : observers_) {
        observer->OnLoopbackInterrupted();
      }
      return;
    }

    for (auto* observer : observers_) {
      observer->OnLoopbackAudio(task->expected_playback_time, task->format,
                                task->sample_rate, task->channels,
                                task->data.get(), task->length);
    }

    base::AutoLock lock(lock_);
    if (task->tag == buffer_tag_) {
      // Only return the buffer if the tag matches. Otherwise, the buffer size
      // may have changed (so we should just delete the buffer).
      buffers_.push_back(std::move(task->data));
    }
  }

  const bool external_audio_pipeline_supported_;

  std::unique_ptr<base::Thread> thread_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  int buffer_size_ = 0;

  base::Lock lock_;
  uint32_t buffer_tag_ GUARDED_BY(lock_) = 0;
  std::vector<std::unique_ptr<uint8_t[]>> buffers_ GUARDED_BY(lock_);
  bool stop_thread_ GUARDED_BY(lock_);
  base::ConditionVariable task_signal_;

  std::vector<Task> tasks_;
  std::vector<Task> new_tasks_;

  base::flat_set<CastMediaShlib::LoopbackAudioObserver*> observers_;

  DISALLOW_COPY_AND_ASSIGN(LoopbackHandlerImpl);
};

}  // namespace

// static
std::unique_ptr<LoopbackHandler, LoopbackHandler::Deleter>
LoopbackHandler::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return std::unique_ptr<LoopbackHandler, LoopbackHandler::Deleter>(
      new LoopbackHandlerImpl(std::move(task_runner)));
}

}  // namespace media
}  // namespace chromecast
