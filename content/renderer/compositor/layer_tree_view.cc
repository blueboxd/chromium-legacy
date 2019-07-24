// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/compositor/layer_tree_view.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/base/region.h"
#include "cc/benchmarks/micro_benchmark.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/layers/layer.h"
#include "cc/trees/latency_info_swap_promise_monitor.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_mutator.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/swap_promise.h"
#include "cc/trees/ukm_manager.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "content/renderer/compositor/layer_tree_view_delegate.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/blink.h"
#include "ui/gfx/presentation_feedback.h"

namespace base {
class Value;
}

namespace cc {
class Layer;
}

namespace content {

LayerTreeView::LayerTreeView(
    LayerTreeViewDelegate* delegate,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_thread,
    cc::TaskGraphRunner* task_graph_runner,
    blink::scheduler::WebThreadScheduler* scheduler)
    : delegate_(delegate),
      main_thread_(std::move(main_thread)),
      compositor_thread_(std::move(compositor_thread)),
      task_graph_runner_(task_graph_runner),
      web_main_thread_scheduler_(scheduler),
      animation_host_(cc::AnimationHost::CreateMainInstance()) {}

LayerTreeView::~LayerTreeView() = default;

void LayerTreeView::Initialize(
    const cc::LayerTreeSettings& settings,
    std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory) {
  const bool is_threaded = !!compositor_thread_;

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.settings = &settings;
  params.task_graph_runner = task_graph_runner_;
  params.main_task_runner = main_thread_;
  params.mutator_host = animation_host_.get();
  params.ukm_recorder_factory = std::move(ukm_recorder_factory);
  if (base::ThreadPoolInstance::Get()) {
    // The image worker thread needs to allow waiting since it makes discardable
    // shared memory allocations which need to make synchronous calls to the
    // IO thread.
    params.image_worker_task_runner = base::CreateSequencedTaskRunnerWithTraits(
        {base::WithBaseSyncPrimitives(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  }
  if (!is_threaded) {
    // Single-threaded web tests, and unit tests.
    layer_tree_host_ =
        cc::LayerTreeHost::CreateSingleThreaded(this, std::move(params));
  } else {
    layer_tree_host_ = cc::LayerTreeHost::CreateThreaded(compositor_thread_,
                                                         std::move(params));
  }
}

void LayerTreeView::SetVisible(bool visible) {
  layer_tree_host_->SetVisible(visible);

  if (visible && layer_tree_frame_sink_request_failed_while_invisible_)
    DidFailToInitializeLayerTreeFrameSink();
}

const base::WeakPtr<cc::InputHandler>& LayerTreeView::GetInputHandler() {
  return layer_tree_host_->GetInputHandler();
}

void LayerTreeView::SetNeedsDisplayOnAllLayers() {
  layer_tree_host_->SetNeedsDisplayOnAllLayers();
}

void LayerTreeView::SetRasterizeOnlyVisibleContent() {
  cc::LayerTreeDebugState current = layer_tree_host_->GetDebugState();
  current.rasterize_only_visible_content = true;
  layer_tree_host_->SetDebugState(current);
}

void LayerTreeView::SetNeedsRedrawRect(gfx::Rect damage_rect) {
  layer_tree_host_->SetNeedsRedrawRect(damage_rect);
}

bool LayerTreeView::IsSurfaceSynchronizationEnabled() const {
  return layer_tree_host_->GetSettings().enable_surface_synchronization;
}

std::unique_ptr<cc::SwapPromiseMonitor>
LayerTreeView::CreateLatencyInfoSwapPromiseMonitor(ui::LatencyInfo* latency) {
  return std::make_unique<cc::LatencyInfoSwapPromiseMonitor>(
      latency, layer_tree_host_->GetSwapPromiseManager(), nullptr);
}

int LayerTreeView::GetSourceFrameNumber() const {
  return layer_tree_host_->SourceFrameNumber();
}

const cc::Layer* LayerTreeView::GetRootLayer() const {
  return layer_tree_host_->root_layer();
}

int LayerTreeView::ScheduleMicroBenchmark(
    const std::string& name,
    std::unique_ptr<base::Value> value,
    base::OnceCallback<void(std::unique_ptr<base::Value>)> callback) {
  return layer_tree_host_->ScheduleMicroBenchmark(name, std::move(value),
                                                  std::move(callback));
}

bool LayerTreeView::SendMessageToMicroBenchmark(
    int id,
    std::unique_ptr<base::Value> value) {
  return layer_tree_host_->SendMessageToMicroBenchmark(id, std::move(value));
}

void LayerTreeView::SetViewportSizeAndScale(
    const gfx::Size& device_viewport_size,
    float device_scale_factor,
    const viz::LocalSurfaceIdAllocation& local_surface_id_allocation) {
  layer_tree_host_->SetViewportSizeAndScale(
      device_viewport_size, device_scale_factor, local_surface_id_allocation);
}

void LayerTreeView::RequestNewLocalSurfaceId() {
  layer_tree_host_->RequestNewLocalSurfaceId();
}

void LayerTreeView::RequestForceSendMetadata() {
  layer_tree_host_->RequestForceSendMetadata();
}

void LayerTreeView::SetViewportVisibleRect(const gfx::Rect& visible_rect) {
  layer_tree_host_->SetViewportVisibleRect(visible_rect);
}

viz::FrameSinkId LayerTreeView::GetFrameSinkId() {
  return frame_sink_id_;
}

void LayerTreeView::SetNonBlinkManagedRootLayer(
    scoped_refptr<cc::Layer> layer) {
  layer_tree_host_->SetNonBlinkManagedRootLayer(std::move(layer));
}

void LayerTreeView::SetNeedsBeginFrame() {
  layer_tree_host_->SetNeedsAnimate();
}

void LayerTreeView::SetLayerTreeFrameSink(
    std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink) {
  if (!layer_tree_frame_sink) {
    DidFailToInitializeLayerTreeFrameSink();
    return;
  }
  layer_tree_host_->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink));
}

int LayerTreeView::LayerTreeId() const {
  return layer_tree_host_->GetId();
}

void LayerTreeView::UpdateBrowserControlsState(
    cc::BrowserControlsState constraints,
    cc::BrowserControlsState current,
    bool animate) {
  layer_tree_host_->UpdateBrowserControlsState(constraints, current, animate);
}

void LayerTreeView::SetBrowserControlsHeight(float top_height,
                                             float bottom_height,
                                             bool shrink) {
  layer_tree_host_->SetBrowserControlsHeight(top_height, bottom_height, shrink);
}

void LayerTreeView::SetBrowserControlsShownRatio(float ratio) {
  layer_tree_host_->SetBrowserControlsShownRatio(ratio);
}

void LayerTreeView::WillBeginMainFrame() {
  delegate_->WillBeginCompositorFrame();
}

void LayerTreeView::DidBeginMainFrame() {
  delegate_->DidBeginMainFrame();
}

void LayerTreeView::WillUpdateLayers() {
  delegate_->BeginUpdateLayers();
}

void LayerTreeView::DidUpdateLayers() {
  delegate_->EndUpdateLayers();
  // Dump property trees and layers if run with:
  //   --vmodule=layer_tree_view=3
  VLOG(3) << "After updating layers:\n"
          << "property trees:\n"
          << layer_tree_host_->property_trees()->ToString() << "\n"
          << "cc::Layers:\n"
          << layer_tree_host_->LayersAsString();
}

void LayerTreeView::BeginMainFrame(const viz::BeginFrameArgs& args) {
  web_main_thread_scheduler_->WillBeginFrame(args);
  delegate_->BeginMainFrame(args.frame_time);
}

void LayerTreeView::BeginMainFrameNotExpectedSoon() {
  web_main_thread_scheduler_->BeginFrameNotExpectedSoon();
}

void LayerTreeView::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {
  web_main_thread_scheduler_->BeginMainFrameNotExpectedUntil(time);
}

void LayerTreeView::UpdateLayerTreeHost() {
  delegate_->UpdateVisualState();
}

void LayerTreeView::ApplyViewportChanges(
    const cc::ApplyViewportChangesArgs& args) {
  delegate_->ApplyViewportChanges(args);
}

void LayerTreeView::RecordManipulationTypeCounts(cc::ManipulationInfo info) {
  delegate_->RecordManipulationTypeCounts(info);
}

void LayerTreeView::SendOverscrollEventFromImplSide(
    const gfx::Vector2dF& overscroll_delta,
    cc::ElementId scroll_latched_element_id) {
  delegate_->SendOverscrollEventFromImplSide(overscroll_delta,
                                             scroll_latched_element_id);
}

void LayerTreeView::SendScrollEndEventFromImplSide(
    cc::ElementId scroll_latched_element_id) {
  delegate_->SendScrollEndEventFromImplSide(scroll_latched_element_id);
}

void LayerTreeView::RequestNewLayerTreeFrameSink() {
  // When the compositor is not visible it would not request a
  // LayerTreeFrameSink so this is a race where it requested one then became
  // not-visible. In that case, we can wait for it to become visible again
  // before replying.
  //
  // This deals with an insidious race when the RenderWidget is swapped-out
  // after this task was posted from the compositor. When swapped-out the
  // compositor is stopped by making it not visible, and the RenderWidget (our
  // delegate) is a zombie which can not be used (https://crbug.com/894899).
  // Eventually the RenderWidget/LayerTreeView will not exist at all in this
  // case (https://crbug.com/419087). So this handles the case for now since the
  // compositor is not visible, and we can avoid using the RenderWidget until
  // it marks the compositor visible again, indicating that it is valid to use
  // the RenderWidget again.
  //
  // If there is no compositor thread, then this is a test-only path where
  // composite is controlled directly by blink, and visibility is not
  // considered. We don't expect blink to ever try composite on a swapped-out
  // RenderWidget, which would be a bug, but the race condition can't happen
  // in the single-thread case since this isn't a posted task.
  if (!layer_tree_host_->IsVisible() && !!compositor_thread_) {
    layer_tree_frame_sink_request_failed_while_invisible_ = true;
    return;
  }

  delegate_->RequestNewLayerTreeFrameSink(base::BindOnce(
      &LayerTreeView::SetLayerTreeFrameSink, weak_factory_.GetWeakPtr()));
}

void LayerTreeView::DidInitializeLayerTreeFrameSink() {}

void LayerTreeView::DidFailToInitializeLayerTreeFrameSink() {
  if (!layer_tree_host_->IsVisible()) {
    layer_tree_frame_sink_request_failed_while_invisible_ = true;
    return;
  }
  layer_tree_frame_sink_request_failed_while_invisible_ = false;
  layer_tree_host_->GetTaskRunnerProvider()->MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&LayerTreeView::RequestNewLayerTreeFrameSink,
                                weak_factory_.GetWeakPtr()));
}

void LayerTreeView::WillCommit() {
  delegate_->WillCommitCompositorFrame();
}

void LayerTreeView::DidCommit() {
  delegate_->DidCommitCompositorFrame();
  web_main_thread_scheduler_->DidCommitFrameToCompositor();
}

void LayerTreeView::DidCommitAndDrawFrame() {
  delegate_->DidCommitAndDrawCompositorFrame();
}

void LayerTreeView::DidCompletePageScaleAnimation() {
  delegate_->DidCompletePageScaleAnimation();
}

void LayerTreeView::DidPresentCompositorFrame(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(layer_tree_host_->GetTaskRunnerProvider()
             ->MainThreadTaskRunner()
             ->RunsTasksInCurrentSequence());
  while (!presentation_callbacks_.empty()) {
    const auto& front = presentation_callbacks_.begin();
    if (viz::FrameTokenGT(front->first, frame_token))
      break;
    for (auto& callback : front->second)
      std::move(callback).Run(feedback.timestamp);
    presentation_callbacks_.erase(front);
  }
}

void LayerTreeView::RecordStartOfFrameMetrics() {
  delegate_->RecordStartOfFrameMetrics();
}

void LayerTreeView::RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) {
  delegate_->RecordEndOfFrameMetrics(frame_begin_time);
}

void LayerTreeView::DidSubmitCompositorFrame() {}

void LayerTreeView::DidLoseLayerTreeFrameSink() {}

void LayerTreeView::SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) {
  frame_sink_id_ = frame_sink_id;
}

void LayerTreeView::SetRasterColorSpace(const gfx::ColorSpace& color_space) {
  layer_tree_host_->SetRasterColorSpace(color_space);
}

void LayerTreeView::SetExternalPageScaleFactor(
    float page_scale_factor,
    bool is_external_pinch_gesture_active) {
  layer_tree_host_->SetExternalPageScaleFactor(
      page_scale_factor, is_external_pinch_gesture_active);
}

void LayerTreeView::ClearCachesOnNextCommit() {
  layer_tree_host_->ClearCachesOnNextCommit();
}

void LayerTreeView::RequestBeginMainFrameNotExpected(bool new_state) {
  layer_tree_host_->RequestBeginMainFrameNotExpected(new_state);
}

const cc::LayerTreeSettings& LayerTreeView::GetLayerTreeSettings() const {
  return layer_tree_host_->GetSettings();
}

void LayerTreeView::SetRenderFrameObserver(
    std::unique_ptr<cc::RenderFrameMetadataObserver> observer) {
  layer_tree_host_->SetRenderFrameObserver(std::move(observer));
}

void LayerTreeView::AddPresentationCallback(
    uint32_t frame_token,
    base::OnceCallback<void(base::TimeTicks)> callback) {
  if (!presentation_callbacks_.empty()) {
    auto& previous = presentation_callbacks_.back();
    uint32_t previous_frame_token = previous.first;
    if (previous_frame_token == frame_token) {
      previous.second.push_back(std::move(callback));
      DCHECK_LE(previous.second.size(), 250u);
      return;
    }
    DCHECK(viz::FrameTokenGT(frame_token, previous_frame_token));
  }
  std::vector<base::OnceCallback<void(base::TimeTicks)>> callbacks;
  callbacks.push_back(std::move(callback));
  presentation_callbacks_.push_back({frame_token, std::move(callbacks)});
  DCHECK_LE(presentation_callbacks_.size(), 25u);
}

void LayerTreeView::SetSourceURL(ukm::SourceId source_id, const GURL& url) {
  layer_tree_host_->SetSourceURL(source_id, url);
}

void LayerTreeView::ReleaseLayerTreeFrameSink() {
  layer_tree_host_->ReleaseLayerTreeFrameSink();
}

}  // namespace content
