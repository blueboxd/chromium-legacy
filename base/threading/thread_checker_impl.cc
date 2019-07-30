// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_checker_impl.h"

#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"

namespace base {

ThreadCheckerImpl::ThreadCheckerImpl() {
  AutoLock auto_lock(lock_);
  EnsureAssigned();
}

ThreadCheckerImpl::~ThreadCheckerImpl() = default;

bool ThreadCheckerImpl::CalledOnValidThread() const {
  const bool has_thread_been_destroyed = ThreadLocalStorage::HasBeenDestroyed();

  AutoLock auto_lock(lock_);
  // TaskToken/SequenceToken access thread-local storage. During destruction
  // the state of thread-local storage is not guaranteed to be in a consistent
  // state. Further, task-runner only installs the tokens when running a task.
  if (!has_thread_been_destroyed) {
    EnsureAssigned();

    // Always return true when called from the task from which this
    // ThreadCheckerImpl was assigned to a thread.
    if (task_token_ == TaskToken::GetForCurrentThread())
      return true;

    // If this ThreadCheckerImpl is bound to a valid SequenceToken, it must be
    // equal to the current SequenceToken and there must be a registered
    // ThreadTaskRunnerHandle. Otherwise, the fact that the current task runs on
    // the thread to which this ThreadCheckerImpl is bound is fortuitous.
    if (sequence_token_.IsValid() &&
        (sequence_token_ != SequenceToken::GetForCurrentThread() ||
         !ThreadTaskRunnerHandle::IsSet())) {
      return false;
    }
  } else if (thread_id_.is_null()) {
    // We're in tls destruction but the |thread_id_| hasn't been assigned yet.
    // Assign it now. This doesn't call EnsureAssigned() as to do so while in
    // tls destruction may result in the wrong TaskToken/SequenceToken.
    thread_id_ = PlatformThread::CurrentRef();
  }

  return thread_id_ == PlatformThread::CurrentRef();
}

void ThreadCheckerImpl::DetachFromThread() {
  AutoLock auto_lock(lock_);
  thread_id_ = PlatformThreadRef();
  task_token_ = TaskToken();
  sequence_token_ = SequenceToken();
}

void ThreadCheckerImpl::EnsureAssigned() const {
  lock_.AssertAcquired();
  if (!thread_id_.is_null())
    return;

  thread_id_ = PlatformThread::CurrentRef();
  task_token_ = TaskToken::GetForCurrentThread();
  sequence_token_ = SequenceToken::GetForCurrentThread();
}

}  // namespace base
