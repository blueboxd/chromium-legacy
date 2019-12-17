// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/portal/portal_activated_observer.h"

#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/hit_test_region_observer.h"

namespace content {

PortalActivatedObserver::PortalActivatedObserver(Portal* portal)
    : interceptor_(PortalInterceptorForTesting::From(portal)->GetWeakPtr()) {
  interceptor_->AddObserver(this);
}

PortalActivatedObserver::~PortalActivatedObserver() {
  if (auto* interceptor = interceptor_.get())
    interceptor->RemoveObserver(this);
}

void PortalActivatedObserver::WaitForActivate() {
  if (has_activated_)
    return;

  base::RunLoop run_loop;
  base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
  run_loop.Run();

  DCHECK(has_activated_);
}

blink::mojom::PortalActivateResult
PortalActivatedObserver::WaitForActivateResult() {
  WaitForActivate();
  if (result_)
    return *result_;

  base::RunLoop run_loop;
  base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
  run_loop.Run();

  DCHECK(result_);
  return *result_;
}

void PortalActivatedObserver::WaitForActivateAndHitTestData() {
  RenderFrameHostImpl* portal_frame =
      interceptor_->GetPortalContents()->GetMainFrame();
  WaitForActivate();

  RenderWidgetHostViewBase* view =
      portal_frame->GetRenderWidgetHost()->GetView();
  viz::FrameSinkId root_frame_sink_id = view->GetRootFrameSinkId();
  HitTestRegionObserver observer(root_frame_sink_id);
  observer.WaitForHitTestData();

  while (true) {
    const auto& display_hit_test_query_map =
        GetHostFrameSinkManager()->display_hit_test_query();
    auto it = display_hit_test_query_map.find(root_frame_sink_id);
    // On Mac, we create a new root layer after activation, so the hit test data
    // may not have anything for the new layer yet.
    if (it != display_hit_test_query_map.end()) {
      viz::HitTestQuery* query = it->second.get();
      size_t index = 0;
      if (query->FindIndexOfFrameSink(view->GetFrameSinkId(), &index)) {
        // The hit test region for the portal frame should be at index 1 after
        // activation, so we wait for the hit test data to update until it's in
        // this state.
        if (index == 1)
          return;
      }
    }
    observer.WaitForHitTestDataChange();
  }
}

void PortalActivatedObserver::OnPortalActivate() {
  DCHECK(!has_activated_)
      << "PortalActivatedObserver can't handle overlapping activations.";
  has_activated_ = true;

  if (run_loop_)
    run_loop_->Quit();
}

void PortalActivatedObserver::OnPortalActivateResult(
    blink::mojom::PortalActivateResult result) {
  DCHECK(has_activated_) << "PortalActivatedObserver should observe the whole "
                            "activation; this may be a race.";
  DCHECK(!result_)
      << "PortalActivatedObserver can't handle overlapping activations.";
  result_ = result;

  if (run_loop_)
    run_loop_->Quit();

  if (auto* interceptor = interceptor_.get())
    interceptor->RemoveObserver(this);
}

}  // namespace content
