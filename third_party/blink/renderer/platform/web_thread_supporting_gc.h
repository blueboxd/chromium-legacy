// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEB_THREAD_SUPPORTING_GC_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEB_THREAD_SUPPORTING_GC_H_

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/heap/gc_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

// WebThreadSupportingGC wraps a blink::Thread and adds support for attaching
// to and detaching from the Blink GC infrastructure.
//
// The initialize method must be called during initialization on the Thread
// and before the thread allocates any objects managed by the Blink GC. The
// shutdown method must be called on the Thread during shutdown when the
// thread no longer needs to access objects managed by the Blink GC.
//
// WebThreadSupportingGC internally creates and owns Thread.
class PLATFORM_EXPORT WebThreadSupportingGC final {
  USING_FAST_MALLOC(WebThreadSupportingGC);

 public:
  explicit WebThreadSupportingGC(const ThreadCreationParams&);
  ~WebThreadSupportingGC();

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() const;

  bool IsCurrentThread() const { return thread_->IsCurrentThread(); }

  void AddTaskObserver(Thread::TaskObserver* observer) {
    thread_->AddTaskObserver(observer);
  }

  void RemoveTaskObserver(Thread::TaskObserver* observer) {
    thread_->RemoveTaskObserver(observer);
  }

  // Must be called on the matching blink::Thread.
  void InitializeOnThread();
  void ShutdownOnThread();

  Thread& PlatformThread() const {
    DCHECK(thread_);
    return *thread_;
  }

 private:
  std::unique_ptr<GCTaskRunner> gc_task_runner_;

  std::unique_ptr<Thread> thread_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(WebThreadSupportingGC);
};

}  // namespace blink

#endif
