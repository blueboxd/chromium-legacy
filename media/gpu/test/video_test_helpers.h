// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_TEST_HELPERS_H_
#define MEDIA_GPU_TEST_VIDEO_TEST_HELPERS_H_

#include <string>
#include <vector>

#include "base/bits.h"
#include "base/containers/queue.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_codecs.h"

namespace media {
namespace test {

// Helper class allowing one thread to wait on a notification from another.
// If notifications come in faster than they are Wait()'d for, they are
// accumulated (so exactly as many Wait() calls will unblock as Notify() calls
// were made, regardless of order).
template <typename StateEnum>
class ClientStateNotification {
 public:
  ClientStateNotification();
  ~ClientStateNotification();

  // Used to notify a single waiter of a ClientState.
  void Notify(StateEnum state);
  // Used by waiters to wait for the next ClientState Notification.
  StateEnum Wait();

 private:
  base::Lock lock_;
  base::ConditionVariable cv_;
  base::queue<StateEnum> pending_states_for_notification_;
};

template <typename StateEnum>
ClientStateNotification<StateEnum>::ClientStateNotification() : cv_(&lock_) {}

template <typename StateEnum>
ClientStateNotification<StateEnum>::~ClientStateNotification() {}

template <typename StateEnum>
void ClientStateNotification<StateEnum>::Notify(StateEnum state) {
  base::AutoLock auto_lock(lock_);
  pending_states_for_notification_.push(state);
  cv_.Signal();
}

template <typename StateEnum>
StateEnum ClientStateNotification<StateEnum>::Wait() {
  base::AutoLock auto_lock(lock_);
  while (pending_states_for_notification_.empty())
    cv_.Wait();
  StateEnum ret = pending_states_for_notification_.front();
  pending_states_for_notification_.pop();
  return ret;
}

class EncodedDataHelper {
 public:
  EncodedDataHelper(const std::vector<uint8_t>& stream,
                    VideoCodecProfile profile);
  ~EncodedDataHelper();

  // Compute and return the next fragment to be sent to the decoder, starting
  // from the current position in the stream, and advance the current position
  // to after the returned fragment.
  scoped_refptr<DecoderBuffer> GetNextBuffer();
  static bool HasConfigInfo(const uint8_t* data,
                            size_t size,
                            VideoCodecProfile profile);

  void Rewind() { next_pos_to_decode_ = 0; }
  bool AtHeadOfStream() const { return next_pos_to_decode_ == 0; }
  bool ReachEndOfStream() const { return next_pos_to_decode_ == data_.size(); }

  size_t num_skipped_fragments() { return num_skipped_fragments_; }

 private:
  // For h.264.
  scoped_refptr<DecoderBuffer> GetNextFragment();
  // For VP8/9.
  scoped_refptr<DecoderBuffer> GetNextFrame();

  // Helpers for GetBytesForNextFragment above.
  size_t GetBytesForNextNALU(size_t pos);
  bool IsNALHeader(const std::string& data, size_t pos);
  bool LookForSPS(size_t* skipped_fragments_count);

  std::string data_;
  VideoCodecProfile profile_;
  size_t next_pos_to_decode_ = 0;
  size_t num_skipped_fragments_ = 0;
};

#if defined(ARCH_CPU_ARM_FAMILY)
// ARM performs CPU cache management with CPU cache line granularity. We thus
// need to ensure our buffers are CPU cache line-aligned (64 byte-aligned).
// Otherwise newer kernels will refuse to accept them, and on older kernels
// we'll be treating ourselves to random corruption.
// Moreover, some hardware codecs require 128-byte alignment for physical
// buffers.
constexpr size_t kPlatformBufferAlignment = 128;
#else
constexpr size_t kPlatformBufferAlignment = 8;
#endif

inline static size_t AlignToPlatformRequirements(size_t value) {
  return base::bits::Align(value, kPlatformBufferAlignment);
}

// An aligned STL allocator.
template <typename T, size_t ByteAlignment = kPlatformBufferAlignment>
class AlignedAllocator : public std::allocator<T> {
 public:
  typedef size_t size_type;
  typedef T* pointer;

  template <class T1>
  struct rebind {
    typedef AlignedAllocator<T1, ByteAlignment> other;
  };

  AlignedAllocator() {}
  explicit AlignedAllocator(const AlignedAllocator&) {}
  template <class T1>
  explicit AlignedAllocator(const AlignedAllocator<T1, ByteAlignment>&) {}
  ~AlignedAllocator() {}

  pointer allocate(size_type n, const void* = 0) {
    return static_cast<pointer>(base::AlignedAlloc(n, ByteAlignment));
  }

  void deallocate(pointer p, size_type n) {
    base::AlignedFree(static_cast<void*>(p));
  }

  size_type max_size() const {
    return std::numeric_limits<size_t>::max() / sizeof(T);
  }
};

}  // namespace test
}  // namespace media
#endif  // MEDIA_GPU_TEST_VIDEO_TEST_HELPERS_H_
