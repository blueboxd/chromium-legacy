// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/callback_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {
class AutoSignal {
 public:
  explicit AutoSignal(base::WaitableEvent* event) : event_(event) {
    DCHECK(event);
  }
  ~AutoSignal() { event_->Signal(); }

 private:
  base::WaitableEvent* event_;

  DISALLOW_COPY_AND_ASSIGN(AutoSignal);
};
}  // namespace

// static
std::unique_ptr<PlatformPaintWorkletLayerPainter>
PaintWorkletPaintDispatcher::CreateCompositorThreadPainter(
    scoped_refptr<PaintWorkletPaintDispatcher>& paint_dispatcher) {
  DCHECK(IsMainThread());
  scoped_refptr<PaintWorkletPaintDispatcher> dispatcher =
      base::MakeRefCounted<PaintWorkletPaintDispatcher>();
  paint_dispatcher = dispatcher;

  return std::make_unique<PlatformPaintWorkletLayerPainter>(dispatcher);
}

PaintWorkletPaintDispatcher::PaintWorkletPaintDispatcher() {
  // PaintWorkletPaintDispatcher is created on the main thread but used on the
  // compositor, so detach the sequence checker until a call is received.
  DCHECK(IsMainThread());
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void PaintWorkletPaintDispatcher::RegisterPaintWorkletPainter(
    PaintWorkletPainter* painter,
    scoped_refptr<base::SingleThreadTaskRunner> painter_runner) {
  TRACE_EVENT0("cc",
               "PaintWorkletPaintDispatcher::RegisterPaintWorkletPainter");

  int worklet_id = painter->GetWorkletId();
  MutexLocker lock(painter_map_mutex_);
  DCHECK(painter_map_.find(worklet_id) == painter_map_.end());
  painter_map_.insert(worklet_id, std::make_pair(painter, painter_runner));
}

void PaintWorkletPaintDispatcher::UnregisterPaintWorkletPainter(
    int worklet_id) {
  TRACE_EVENT0("cc",
               "PaintWorkletPaintDispatcher::"
               "UnregisterPaintWorkletPainter");
  MutexLocker lock(painter_map_mutex_);
  DCHECK(painter_map_.find(worklet_id) != painter_map_.end());
  painter_map_.erase(worklet_id);
}

// TODO(xidachen): we should bundle all PaintWorkletInputs and send them to the
// |worklet_queue| once, instead of sending one PaintWorkletInput at a time.
// This avoids thread hop and boost performance.
sk_sp<cc::PaintRecord> PaintWorkletPaintDispatcher::Paint(
    const cc::PaintWorkletInput* input) {
  TRACE_EVENT0("cc", "PaintWorkletPaintDispatcher::Paint");
  sk_sp<cc::PaintRecord> output = sk_make_sp<cc::PaintOpBuffer>();

  PaintWorkletPainterToTaskRunnerMap copied_painter_map = GetPainterMapCopy();
  if (copied_painter_map.IsEmpty())
    return output;

  base::WaitableEvent done_event;

  auto it = copied_painter_map.find(input->WorkletId());
  if (it == copied_painter_map.end())
    return output;

  PaintWorkletPainter* painter = it->value.first;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner = it->value.second;
  DCHECK(!task_runner->BelongsToCurrentThread());
  std::unique_ptr<AutoSignal> done = std::make_unique<AutoSignal>(&done_event);

  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(
          [](PaintWorkletPainter* painter, const cc::PaintWorkletInput* input,
             std::unique_ptr<AutoSignal> completion,
             sk_sp<cc::PaintRecord>* output) {
            *output = painter->Paint(input);
          },
          WrapCrossThreadPersistent(painter), WTF::CrossThreadUnretained(input),
          WTF::Passed(std::move(done)), WTF::CrossThreadUnretained(&output)));

  done_event.Wait();

  // If the paint fails, PaintWorkletPainter should return an empty record
  // rather than a nullptr.
  DCHECK(output);

  return output;
}

void PaintWorkletPaintDispatcher::DispatchWorklets(
    cc::PaintWorkletJobMap worklet_job_map,
    PlatformPaintWorkletLayerPainter::DoneCallback done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("cc", "PaintWorkletPaintDispatcher::DispatchWorklets");

  // Dispatching to the worklets is an asynchronous process, but there should
  // only be one dispatch going on at once. We store the completion callback and
  // the PaintWorklet job map in the class during the dispatch, then clear them
  // when we get results (see AsyncPaintDone).
  DCHECK(on_async_paint_complete_.is_null());
  on_async_paint_complete_ = std::move(done_callback);
  ongoing_jobs_ = std::move(worklet_job_map);

  scoped_refptr<base::SingleThreadTaskRunner> runner =
      Thread::Current()->GetTaskRunner();
  WTF::CrossThreadClosure on_done = CrossThreadBind(
      [](scoped_refptr<PaintWorkletPaintDispatcher> dispatcher,
         scoped_refptr<base::SingleThreadTaskRunner> runner) {
        PostCrossThreadTask(
            *runner, FROM_HERE,
            CrossThreadBindOnce(&PaintWorkletPaintDispatcher::AsyncPaintDone,
                                dispatcher));
      },
      WrapRefCounted(this), WTF::Passed(std::move(runner)));

  // Use a base::RepeatingClosure to make sure that AsyncPaintDone is only
  // called once, once all the worklets are done. If there are no inputs
  // specified, base::RepeatingClosure will trigger immediately and so the
  // callback will still happen.
  base::RepeatingClosure repeating_on_done = base::BarrierClosure(
      ongoing_jobs_.size(), ConvertToBaseCallback(std::move(on_done)));

  // Now dispatch the calls to the registered painters. For each input, we match
  // the id to a registered worklet and dispatch a cross-thread call to it,
  // using the above-created base::RepeatingClosure.
  PaintWorkletPainterToTaskRunnerMap copied_painter_map = GetPainterMapCopy();
  for (auto& job : ongoing_jobs_) {
    int worklet_id = job.first;
    scoped_refptr<cc::PaintWorkletJobVector> jobs = job.second;

    // Wrap the barrier closure in a ScopedClosureRunner to guarantee it runs
    // even if there is no matching worklet or the posted task does not run.
    auto on_done_runner =
        std::make_unique<base::ScopedClosureRunner>(repeating_on_done);

    auto it = copied_painter_map.find(worklet_id);
    if (it == copied_painter_map.end())
      continue;

    PaintWorkletPainter* painter = it->value.first;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner = it->value.second;
    DCHECK(!task_runner->BelongsToCurrentThread());

    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBindOnce(
            [](PaintWorkletPainter* painter,
               scoped_refptr<cc::PaintWorkletJobVector> jobs,
               std::unique_ptr<base::ScopedClosureRunner> on_done_runner) {
              for (cc::PaintWorkletJob& job : jobs->data) {
                job.SetOutput(painter->Paint(job.input().get()));
              }
              on_done_runner->RunAndReset();
            },
            WrapCrossThreadPersistent(painter), WTF::Passed(std::move(jobs)),
            WTF::Passed(std::move(on_done_runner))));
  }
}

void PaintWorkletPaintDispatcher::AsyncPaintDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("cc", "PaintWorkletPaintDispatcher::AsyncPaintDone");
  std::move(on_async_paint_complete_).Run(std::move(ongoing_jobs_));
}

PaintWorkletPaintDispatcher::PaintWorkletPainterToTaskRunnerMap
PaintWorkletPaintDispatcher::GetPainterMapCopy() {
  MutexLocker lock(painter_map_mutex_);
  return painter_map_;
}

}  // namespace blink
