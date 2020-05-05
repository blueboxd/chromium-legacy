// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/widget_base.h"

#include "base/metrics/histogram_macros.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/ukm_manager.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_render_widget_scheduling_state.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_view.h"
#include "third_party/blink/renderer/platform/widget/widget_base_client.h"

namespace blink {

namespace {

scoped_refptr<base::SingleThreadTaskRunner> GetCleanupTaskRunner() {
  if (auto* main_thread_scheduler =
          scheduler::WebThreadScheduler::MainThreadScheduler()) {
    return main_thread_scheduler->CleanupTaskRunner();
  } else {
    return base::ThreadTaskRunnerHandle::Get();
  }
}

}  // namespace

WidgetBase::WidgetBase(
    WidgetBaseClient* client,
    CrossVariantMojoAssociatedRemote<mojom::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::WidgetInterfaceBase> widget)
    : client_(client),
      widget_host_(std::move(widget_host)),
      receiver_(this, std::move(widget)) {
  if (auto* main_thread_scheduler =
          scheduler::WebThreadScheduler::MainThreadScheduler()) {
    render_widget_scheduling_state_ =
        main_thread_scheduler->NewRenderWidgetSchedulingState();
  }
}

WidgetBase::~WidgetBase() {
  // Ensure Shutdown was called.
  DCHECK(!layer_tree_view_);
}

void WidgetBase::InitializeCompositing(
    cc::TaskGraphRunner* task_graph_runner,
    const cc::LayerTreeSettings& settings,
    std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory) {
  auto* main_thread_scheduler =
      scheduler::WebThreadScheduler::MainThreadScheduler();
  auto* compositing_thread_scheduler =
      scheduler::WebThreadScheduler::CompositorThreadScheduler();
  layer_tree_view_ = std::make_unique<LayerTreeView>(
      this,
      main_thread_scheduler ? main_thread_scheduler->CompositorTaskRunner()
                            : base::ThreadTaskRunnerHandle::Get(),
      compositing_thread_scheduler
          ? compositing_thread_scheduler->DefaultTaskRunner()
          : nullptr,
      task_graph_runner, main_thread_scheduler);
  layer_tree_view_->Initialize(settings, std::move(ukm_recorder_factory));
}

void WidgetBase::Shutdown(
    scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner,
    base::OnceCallback<void()> cleanup_task) {
  if (!cleanup_runner)
    cleanup_runner = GetCleanupTaskRunner();

  // The LayerTreeHost may already be in the call stack, if this WidgetBase
  // is being destroyed during an animation callback for instance. We can not
  // delete it here and unwind the stack back up to it, or it will crash. So
  // we post the deletion to another task, but disconnect the LayerTreeHost
  // (via the LayerTreeView) from the destroying WidgetBase. The
  // LayerTreeView owns the LayerTreeHost, and is its client, so they are kept
  // alive together for a clean call stack.
  if (layer_tree_view_) {
    layer_tree_view_->Disconnect();
    cleanup_runner->DeleteSoon(FROM_HERE, std::move(layer_tree_view_));
  }
  // This needs to be a NonNestableTask as it needs to occur after DeleteSoon.
  if (cleanup_task)
    cleanup_runner->PostNonNestableTask(FROM_HERE, std::move(cleanup_task));
}

cc::LayerTreeHost* WidgetBase::LayerTreeHost() const {
  return layer_tree_view_->layer_tree_host();
}

cc::AnimationHost* WidgetBase::AnimationHost() const {
  return layer_tree_view_->animation_host();
}

scheduler::WebRenderWidgetSchedulingState*
WidgetBase::RendererWidgetSchedulingState() const {
  return render_widget_scheduling_state_.get();
}

void WidgetBase::ApplyViewportChanges(
    const cc::ApplyViewportChangesArgs& args) {
  client_->ApplyViewportChanges(args);
}

void WidgetBase::RecordManipulationTypeCounts(cc::ManipulationInfo info) {
  client_->RecordManipulationTypeCounts(info);
}

void WidgetBase::SendOverscrollEventFromImplSide(
    const gfx::Vector2dF& overscroll_delta,
    cc::ElementId scroll_latched_element_id) {
  client_->SendOverscrollEventFromImplSide(overscroll_delta,
                                           scroll_latched_element_id);
}

void WidgetBase::SendScrollEndEventFromImplSide(
    cc::ElementId scroll_latched_element_id) {
  client_->SendScrollEndEventFromImplSide(scroll_latched_element_id);
}

void WidgetBase::OnDeferMainFrameUpdatesChanged(bool defer) {
  client_->OnDeferMainFrameUpdatesChanged(defer);
}

void WidgetBase::OnDeferCommitsChanged(bool defer) {
  client_->OnDeferCommitsChanged(defer);
}

void WidgetBase::DidBeginMainFrame() {
  client_->DidBeginMainFrame();
}

void WidgetBase::RequestNewLayerTreeFrameSink(
    LayerTreeFrameSinkCallback callback) {
  client_->RequestNewLayerTreeFrameSink(std::move(callback));
}

void WidgetBase::DidCommitAndDrawCompositorFrame() {
  client_->DidCommitAndDrawCompositorFrame();
}

void WidgetBase::WillCommitCompositorFrame() {
  client_->BeginCommitCompositorFrame();
}

void WidgetBase::DidCommitCompositorFrame(base::TimeTicks commit_start_time) {
  client_->EndCommitCompositorFrame(commit_start_time);
}

void WidgetBase::DidCompletePageScaleAnimation() {
  client_->DidCompletePageScaleAnimation();
}

void WidgetBase::RecordStartOfFrameMetrics() {
  client_->RecordStartOfFrameMetrics();
}

void WidgetBase::RecordEndOfFrameMetrics(
    base::TimeTicks frame_begin_time,
    cc::ActiveFrameSequenceTrackers trackers) {
  client_->RecordEndOfFrameMetrics(frame_begin_time, trackers);
}

std::unique_ptr<cc::BeginMainFrameMetrics>
WidgetBase::GetBeginMainFrameMetrics() {
  return client_->GetBeginMainFrameMetrics();
}

void WidgetBase::BeginUpdateLayers() {
  client_->BeginUpdateLayers();
}

void WidgetBase::EndUpdateLayers() {
  client_->EndUpdateLayers();
}

void WidgetBase::WillBeginMainFrame() {
  client_->SetSuppressFrameRequestsWorkaroundFor704763Only(true);
  client_->WillBeginMainFrame();
}

void WidgetBase::SetCompositorVisible(bool visible) {
  if (visible)
    was_shown_time_ = base::TimeTicks::Now();
  else
    first_update_visual_state_after_hidden_ = true;
  layer_tree_view_->SetVisible(visible);
}

void WidgetBase::UpdateVisualState() {
  // When recording main frame metrics set the lifecycle reason to
  // kBeginMainFrame, because this is the calller of UpdateLifecycle
  // for the main frame. Otherwise, set the reason to kTests, which is
  // the only other reason this method is called.
  DocumentUpdateReason lifecycle_reason =
      ShouldRecordBeginMainFrameMetrics()
          ? DocumentUpdateReason::kBeginMainFrame
          : DocumentUpdateReason::kTest;
  client_->UpdateLifecycle(WebLifecycleUpdate::kAll, lifecycle_reason);
  client_->SetSuppressFrameRequestsWorkaroundFor704763Only(false);
  if (first_update_visual_state_after_hidden_) {
    client_->RecordTimeToFirstActivePaint(base::TimeTicks::Now() -
                                          was_shown_time_);
    first_update_visual_state_after_hidden_ = false;
  }
}

void WidgetBase::BeginMainFrame(base::TimeTicks frame_time) {
  client_->DispatchRafAlignedInput(frame_time);
  client_->BeginMainFrame(frame_time);
}

bool WidgetBase::ShouldRecordBeginMainFrameMetrics() {
  // We record metrics only when running in multi-threaded mode, not
  // single-thread mode for testing.
  return Thread::CompositorThread();
}

void WidgetBase::AddPresentationCallback(
    uint32_t frame_token,
    base::OnceCallback<void(base::TimeTicks)> callback) {
  layer_tree_view_->AddPresentationCallback(frame_token, std::move(callback));
}

}  // namespace blink
