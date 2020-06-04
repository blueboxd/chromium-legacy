// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_TASK_RUNNER_CONTEXT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_TASK_RUNNER_CONTEXT_H_

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"

namespace reporting {

// This class defines refcounted context for multiple actions executed on
// a sequenced task runner with the ability to make asynchronous calls to
// other threads and resuming sequenced execution by calling |Schedule| or
// |ScheduleAfter|. Multiple actions can be scheduled at once; they will be
// executed on the same sequenced task runner. Ends execution when one of the
// actions calls |Response| (any previoudly scheduled action will still be
// executed after that, but it does not make much sense: it cannot call
// |Response| for the second time).
//
// Derived from RefCountedThreadSafe, because adding and releasing a reference
// may take place on different threads.
//
// Code snippet:
//
// Declaration:
// class SeriesOfActionsContext : public TaskRunnerContext<...> {
//    public:
//     SeriesOfActionsContext(
//         ...,
//         base::OnceCallback<void(...)> callback,
//         scoped_refptr<base::SequencedTaskRunner> task_runner)
//         : TaskRunnerContext<...>(std::move(callback),
//                                  std::move(task_runner)) {}
//
//    protected:
//     // Context can only be deleted by calling Response method.
//     ~SeriesOfActionsContext() override = default;
//
//    private:
//     void Action1(...) {
//       ...
//       if (...) {
//         Response(...);
//         return;
//       }
//       Schedule(&SeriesOfActionsContext::Action2, this, ...);
//       ...
//       ScheduleAfter(delay, &SeriesOfActionsContext::Action3, this, ...);
//     }
//
//     void OnStart() override { Action1(...); }
//   };
//
// Usage:
//   base::MakeRefCounted<SeriesOfActionsContext>(
//       ...,
//       returning_callback,
//       base::SequencedTaskRunnerHandle::Get())->Start();
//
template <typename ResponseType>
class TaskRunnerContext
    : public base::RefCountedThreadSafe<TaskRunnerContext<ResponseType>> {
 public:
  TaskRunnerContext(base::OnceCallback<void(ResponseType)> callback,
                    scoped_refptr<base::SequencedTaskRunner> task_runner)
      : callback_(std::move(callback)), task_runner_(std::move(task_runner)) {
    // Constructor can be called from any thread.
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  TaskRunnerContext(const TaskRunnerContext& other) = delete;
  TaskRunnerContext& operator=(const TaskRunnerContext& other) = delete;

  // Starts execution (can be called from any thread to schedule the first
  // action in the sequence).
  void Start() {
    // Hold to ourselves until Response() is called.
    base::RefCountedThreadSafe<TaskRunnerContext<ResponseType>>::AddRef();

    // Place actual start on the sequential task runner.
    Schedule(&TaskRunnerContext<ResponseType>::OnStartWrap, this);
  }

  // Schedules next execution (can be called from any thread).
  template <class Function, class... Args>
  void Schedule(Function&& proc, Args&&... args) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::forward<Function>(proc),
                                          std::forward<Args>(args)...));
  }

  // Schedules next execution with delay (can be called from any thread).
  template <class Function, class... Args>
  void ScheduleAfter(base::TimeDelta delay, Function&& proc, Args&&... args) {
    task_runner_->PostDelayedTask(FROM_HERE,
                                  base::BindOnce(std::forward<Function>(proc),
                                                 std::forward<Args>(args)...),
                                  delay);
  }

  // Responds to the caller once completed the work sequence
  // (can only be called by action scheduled to the sequenced task runner).
  void Response(ResponseType result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    OnCompletion();

    // Respond to the caller.
    DCHECK(!callback_.is_null()) << "Already responded";
    std::move(callback_).Run(result);

    // Release reference taken by Start().
    base::RefCountedThreadSafe<TaskRunnerContext<ResponseType>>::Release();
  }

  // Helper method checks that the caller runs on valid sequence.
  // Can be used by any scheduled action.
  // No need to call it by OnStart, OnCompletion and destructor.
  // For non-debug builds it is a no-op.
  void CheckOnValidSequence() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 protected:
  // Context can only be deleted by calling Response method.
  virtual ~TaskRunnerContext() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(callback_.is_null()) << "Released without responding to the caller";
  }

 private:
  friend class base::RefCountedThreadSafe<TaskRunnerContext<ResponseType>>;

  // Hook for execution start. Should be overridden to do non-trivial work.
  virtual void OnStart() { Response(ResponseType()); }

  // Finalization action before responding and deleting the context.
  // May be overridden, if necessary.
  virtual void OnCompletion() {}

  // Wrapper for OnStart to mandate sequence checker.
  void OnStartWrap() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    OnStart();
  }

  // User callback to deliver result.
  base::OnceCallback<void(ResponseType)> callback_;

  // Sequential task runner (guarantees that each action is executed
  // sequentially in order of submission).
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_TASK_RUNNER_CONTEXT_H_
