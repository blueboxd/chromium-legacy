/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/frame/dom_timer.h"

#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/scheduled_action.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

// Step 11 of the algorithm at
// https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html requires
// that a timeout less than 4ms is increased to 4ms when the nesting level is
// greater than 5.
constexpr int kMaxTimerNestingLevel = 5;
constexpr base::TimeDelta kMinimumInterval =
    base::TimeDelta::FromMilliseconds(4);

// Computes the timeout of a timer according to the algorithm at
// https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html.
base::TimeDelta ComputeTimeout(base::TimeDelta timeout, int nesting_level) {
  // Step 10:
  if (timeout < base::TimeDelta())
    timeout = base::TimeDelta();

  // Step 11 (the implementation is not spec-compliant crbug.com/1108877):
  if (nesting_level + 1 >= kMaxTimerNestingLevel && timeout < kMinimumInterval)
    timeout = kMinimumInterval;

  return timeout;
}

}  // namespace

int DOMTimer::Install(ExecutionContext* context,
                      ScheduledAction* action,
                      base::TimeDelta timeout,
                      bool single_shot) {
  int timeout_id = context->Timers()->InstallNewTimeout(context, action,
                                                        timeout, single_shot);
  return timeout_id;
}

void DOMTimer::RemoveByID(ExecutionContext* context, int timeout_id) {
  DOMTimer* timer = context->Timers()->RemoveTimeoutByID(timeout_id);
  TRACE_EVENT_INSTANT1("devtools.timeline", "TimerRemove",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_timer_remove_event::Data(context, timeout_id));
  // Eagerly unregister as ExecutionContext observer.
  if (timer)
    timer->SetExecutionContext(nullptr);
}
DOMTimer::DOMTimer(ExecutionContext* context,
                   ScheduledAction* action,
                   base::TimeDelta timeout,
                   bool single_shot,
                   int timeout_id)
    : ExecutionContextLifecycleObserver(context),
      TimerBase(context->GetTaskRunner(
          ComputeTimeout(timeout, context->Timers()->TimerNestingLevel())
                  .is_zero()
              ? TaskType::kJavascriptTimerImmediate
              : TaskType::kJavascriptTimerDelayed)),
      timeout_id_(timeout_id),
      nesting_level_(context->Timers()->TimerNestingLevel()),
      action_(action) {
  DCHECK_GT(timeout_id, 0);

  // Steps 10 and 11, and rounding up to 1 ms for historical reasons
  // crbug.com/402694.
  timeout = std::max(base::TimeDelta::FromMilliseconds(1),
                     ComputeTimeout(timeout, nesting_level_));

  // Step 12 and 13:
  ++nesting_level_;

  if (single_shot)
    StartOneShot(timeout, FROM_HERE);
  else
    StartRepeating(timeout, FROM_HERE);

  TRACE_EVENT_INSTANT1("devtools.timeline", "TimerInstall",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_timer_install_event::Data(
                           context, timeout_id, timeout, single_shot));
  probe::AsyncTaskScheduledBreakable(
      context, single_shot ? "setTimeout" : "setInterval", &async_task_id_);
}

DOMTimer::~DOMTimer() = default;

void DOMTimer::Dispose() {
  Stop();
}

void DOMTimer::Stop() {
  if (!action_)
    return;

  const bool is_interval = !RepeatInterval().is_zero();
  probe::AsyncTaskCanceledBreakable(
      GetExecutionContext(), is_interval ? "clearInterval" : "clearTimeout",
      &async_task_id_);

  // Need to release JS objects potentially protected by ScheduledAction
  // because they can form circular references back to the ExecutionContext
  // which will cause a memory leak.
  if (action_)
    action_->Dispose();
  action_ = nullptr;
  TimerBase::Stop();
}

void DOMTimer::ContextDestroyed() {
  Stop();
}

void DOMTimer::Fired() {
  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);
  context->Timers()->SetTimerNestingLevel(nesting_level_);
  DCHECK(!context->IsContextPaused());
  // Only the first execution of a multi-shot timer should get an affirmative
  // user gesture indicator.

  TRACE_EVENT1("devtools.timeline", "TimerFire", "data",
               inspector_timer_fire_event::Data(context, timeout_id_));
  const bool is_interval = !RepeatInterval().is_zero();
  probe::UserCallback probe(context, is_interval ? "setInterval" : "setTimeout",
                            g_null_atom, true);
  probe::AsyncTask async_task(context, &async_task_id_,
                              is_interval ? "fired" : nullptr);

  // Simple case for non-one-shot timers.
  if (IsActive()) {
    if (is_interval && RepeatInterval() < kMinimumInterval) {
      nesting_level_++;
      if (nesting_level_ >= kMaxTimerNestingLevel) {
        MoveToNewTaskRunner(
            context->GetTaskRunner(TaskType::kJavascriptTimerDelayed));
        AugmentRepeatInterval(kMinimumInterval - RepeatInterval());
      }
    }

    // No access to member variables after this point, it can delete the timer.
    action_->Execute(context);

    context->Timers()->SetTimerNestingLevel(0);

    return;
  }

  // Unregister the timer from ExecutionContext before executing the action
  // for one-shot timers.
  ScheduledAction* action = action_.Release();
  context->Timers()->RemoveTimeoutByID(timeout_id_);

  action->Execute(context);

  // Eagerly clear out |action|'s resources.
  action->Dispose();

  // ExecutionContext might be already gone when we executed action->execute().
  ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context)
    return;

  execution_context->Timers()->SetTimerNestingLevel(0);
  // Eagerly unregister as ExecutionContext observer.
  SetExecutionContext(nullptr);
}

void DOMTimer::Trace(Visitor* visitor) const {
  visitor->Trace(action_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
