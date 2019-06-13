// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "v8/include/v8.h"

namespace blink {
namespace scheduler {

EventLoop::EventLoop(v8::Isolate* isolate)
    : isolate_(isolate),
      // TODO(keishi): Create MicrotaskQueue to enable per-EventLoop microtask
      // queue.
      microtask_queue_(nullptr) {
  DCHECK(isolate_);
}

EventLoop::~EventLoop() {
  microtask_queue_ = nullptr;
}

void EventLoop::EnqueueMicrotask(base::OnceClosure task) {
  pending_microtasks_.push_back(std::move(task));
  if (microtask_queue_) {
    microtask_queue_->EnqueueMicrotask(isolate_,
                                       &EventLoop::RunPendingMicrotask, this);
  } else {
    isolate_->EnqueueMicrotask(&EventLoop::RunPendingMicrotask, this);
  }
}

void EventLoop::PerformMicrotaskCheckpoint() {
  if (microtask_queue_)
    microtask_queue_->PerformCheckpoint(isolate_);
}

// static
void EventLoop::PerformIsolateGlobalMicrotasksCheckpoint(v8::Isolate* isolate) {
  v8::MicrotasksScope::PerformCheckpoint(isolate);
}

void EventLoop::Disable() {
  loop_enabled_ = false;
  // TODO(tzik): Disable associated Frames.
}

void EventLoop::Enable() {
  loop_enabled_ = true;
  // TODO(tzik): Enable associated Frames.
}

// static
void EventLoop::RunPendingMicrotask(void* data) {
  TRACE_EVENT0("renderer.scheduler", "RunPendingMicrotask");
  auto* self = static_cast<EventLoop*>(data);
  base::OnceClosure task = std::move(self->pending_microtasks_.front());
  self->pending_microtasks_.pop_front();
  std::move(task).Run();
}

}  // namespace scheduler
}  // namespace blink
