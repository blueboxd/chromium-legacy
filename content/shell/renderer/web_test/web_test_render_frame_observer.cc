// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/web_test_render_frame_observer.h"

#include <string>

#include "base/bind.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/shell/renderer/web_test/blink_test_runner.h"
#include "content/shell/renderer/web_test/web_test_render_thread_observer.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_runner.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

WebTestRenderFrameObserver::WebTestRenderFrameObserver(
    RenderFrame* render_frame,
    BlinkTestRunner* blink_test_runner)
    : RenderFrameObserver(render_frame), blink_test_runner_(blink_test_runner) {
  TestRunner* test_runner = WebTestRenderThreadObserver::GetInstance()
                                ->test_interfaces()
                                ->GetTestRunner();
  render_frame->GetWebFrame()->SetContentSettingsClient(
      test_runner->GetWebContentSettings());
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&WebTestRenderFrameObserver::BindReceiver,
                          base::Unretained(this)));
}

WebTestRenderFrameObserver::~WebTestRenderFrameObserver() = default;

void WebTestRenderFrameObserver::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::BlinkTestControl> receiver) {
  receiver_.Bind(std::move(receiver),
                 blink::scheduler::GetSingleThreadTaskRunnerForTesting());
}

void WebTestRenderFrameObserver::DidCommitProvisionalLoad(
    bool is_same_document_navigation,
    ui::PageTransition transition) {
  if (!render_frame()->IsMainFrame())
    return;
  if (!is_same_document_navigation) {
    render_frame()->GetRenderView()->GetWebView()->SetFocusedFrame(
        render_frame()->GetWebFrame());
  }
  blink_test_runner_->DidCommitNavigationInMainFrame();
}

void WebTestRenderFrameObserver::OnDestruct() {
  delete this;
}

void WebTestRenderFrameObserver::CaptureDump(CaptureDumpCallback callback) {
  blink_test_runner_->CaptureDump(std::move(callback));
}

void WebTestRenderFrameObserver::CompositeWithRaster(
    CompositeWithRasterCallback callback) {
  // When the TestFinished() occurred, if the browser is capturing pixels, it
  // asks each composited RenderFrame to submit a new frame via here.
  render_frame()->UpdateAllLifecyclePhasesAndCompositeForTesting();
  std::move(callback).Run();
}

void WebTestRenderFrameObserver::DumpFrameLayout(
    DumpFrameLayoutCallback callback) {
  TestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();
  TestRunner* test_runner = interfaces->GetTestRunner();
  std::string dump = test_runner->DumpLayout(render_frame()->GetWebFrame());
  std::move(callback).Run(std::move(dump));
}

void WebTestRenderFrameObserver::ReplicateTestConfiguration(
    mojom::ShellTestConfigurationPtr config) {
  blink_test_runner_->OnReplicateTestConfiguration(std::move(config));
}

void WebTestRenderFrameObserver::SetTestConfiguration(
    mojom::ShellTestConfigurationPtr config) {
  blink_test_runner_->OnSetTestConfiguration(std::move(config));
}

void WebTestRenderFrameObserver::SetupRendererProcessForNonTestWindow() {
  blink_test_runner_->OnSetupRendererProcessForNonTestWindow();
}

void WebTestRenderFrameObserver::Reset() {
  blink_test_runner_->OnReset();
}

void WebTestRenderFrameObserver::TestFinishedInSecondaryRenderer() {
  blink_test_runner_->OnTestFinishedInSecondaryRenderer();
}

void WebTestRenderFrameObserver::LayoutDumpCompleted(
    const std::string& completed_layout_dump) {
  blink_test_runner_->OnLayoutDumpCompleted(completed_layout_dump);
}

void WebTestRenderFrameObserver::ReplyBluetoothManualChooserEvents(
    const std::vector<std::string>& events) {
  blink_test_runner_->OnReplyBluetoothManualChooserEvents(events);
}

}  // namespace content
