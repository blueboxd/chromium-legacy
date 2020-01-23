// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/display_scheduler_webview.h"
#include "android_webview/browser/gfx/root_frame_sink.h"

namespace android_webview {
DisplaySchedulerWebView::DisplaySchedulerWebView(RootFrameSink* root_frame_sink)
    : root_frame_sink_(root_frame_sink) {}
DisplaySchedulerWebView::~DisplaySchedulerWebView() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void DisplaySchedulerWebView::ForceImmediateSwapIfPossible() {
  // We can't swap immediately
  NOTREACHED();
}
void DisplaySchedulerWebView::SetNeedsOneBeginFrame(bool needs_draw) {
  // Used with De-Jelly and headless begin frames
  NOTREACHED();
}
void DisplaySchedulerWebView::DidSwapBuffers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  root_frame_sink_->SetNeedsDraw(false);
}
void DisplaySchedulerWebView::OutputSurfaceLost() {
  // WebView can't handle surface lost so this isn't called.
  NOTREACHED();
}
void DisplaySchedulerWebView::OnDisplayDamaged(viz::SurfaceId surface_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // We don't need to track damage of root frame sink as we submit frame to it
  // at DrawAndSwap and Root Renderer sink because Android View.Invalidation is
  // handled by SynchronousCompositorHost.
  if (surface_id.frame_sink_id() != root_frame_sink_->root_frame_sink_id() &&
      !root_frame_sink_->IsChildSurface(surface_id.frame_sink_id())) {
    root_frame_sink_->SetNeedsDraw(true);
  }
}
}  // namespace android_webview
