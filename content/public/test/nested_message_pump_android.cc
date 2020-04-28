// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/nested_message_pump_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_op.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "content/public/test/android/test_support_content_jni_headers/NestedSystemMessageHandler_jni.h"

namespace content {

struct NestedMessagePumpAndroid::RunState {
  RunState(base::MessagePump::Delegate* delegate, int run_depth)
      : delegate(delegate),
        run_depth(run_depth),
        should_quit(false),
        waitable_event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED) {
    waitable_event.declare_only_used_while_idle();
  }

  base::MessagePump::Delegate* delegate;

  // Used to count how many Run() invocations are on the stack.
  int run_depth;

  // Used to flag that the current Run() invocation should return ASAP.
  bool should_quit;

  // Used to sleep until there is more work to do.
  base::WaitableEvent waitable_event;
};

NestedMessagePumpAndroid::NestedMessagePumpAndroid()
    : state_(NULL) {
}

NestedMessagePumpAndroid::~NestedMessagePumpAndroid() {
}

void NestedMessagePumpAndroid::Run(Delegate* delegate) {
  RunState state(delegate, state_ ? state_->run_depth + 1 : 1);
  RunState* previous_state = state_;
  state_ = &state;

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  // Need to cap the wait time to allow task processing on the java
  // side. Otherwise, a long wait time on the native will starve java
  // tasks.
  base::TimeDelta max_delay = base::TimeDelta::FromMilliseconds(100);

  for (;;) {
    if (state_->should_quit)
      break;

    Delegate::NextWorkInfo next_work_info = delegate->DoWork();
    bool has_more_immediate_work = next_work_info.is_immediate();
    if (state_->should_quit)
      break;

    if (has_more_immediate_work)
      continue;

    has_more_immediate_work = state_->delegate->DoIdleWork();
    if (state_->should_quit)
      break;

    if (has_more_immediate_work)
      continue;

    // No native tasks to process right now. Process tasks from the Java
    // System message handler. This will return when the java message queue
    // is idle.
    bool ret = Java_NestedSystemMessageHandler_runNestedLoopTillIdle(env);
    CHECK(ret) << "Error running java message loop, tests will likely fail.";

    if (next_work_info.delayed_run_time.is_max()) {
      state_->waitable_event.TimedWait(max_delay);
    } else {
      base::TimeDelta delay = next_work_info.remaining_delay();
      if (delay > max_delay)
        delay = max_delay;
      DCHECK_GT(delay, base::TimeDelta());
      state_->waitable_event.TimedWait(delay);
    }
  }

  state_ = previous_state;
}

void NestedMessagePumpAndroid::Attach(base::MessagePump::Delegate* delegate) {}

void NestedMessagePumpAndroid::Quit() {
  if (state_) {
    state_->should_quit = true;
    state_->waitable_event.Signal();
    return;
  }
}

void NestedMessagePumpAndroid::ScheduleWork() {
  if (state_) {
    state_->waitable_event.Signal();
    return;
  }
}

void NestedMessagePumpAndroid::ScheduleDelayedWork(
    const base::TimeTicks& delayed_work_time) {
  // Since this is always called from the same thread as Run(), there is nothing
  // to do as the loop is already running. It will wait in Run() with the
  // correct timeout when it's out of immediate tasks.
  // TODO(gab): Consider removing ScheduleDelayedWork() when all pumps function
  // this way (bit.ly/merge-message-pump-do-work).
}

}  // namespace content
