// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/web_test_support.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "build/build_config.h"
#include "content/browser/bluetooth/bluetooth_device_chooser_controller.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/renderer.mojom.h"
#include "content/common/unique_name_helper.h"
#include "content/public/browser/storage_partition.h"
#include "content/renderer/input/render_widget_input_handler_delegate.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/web_worker_fetch_context_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "content/shell/renderer/web_test/blink_test_runner.h"
#include "content/shell/renderer/web_test/web_test_render_thread_observer.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/web_frame_test_proxy.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/web/web_manifest_manager.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/test/icc_profiles.h"

#if defined(OS_MACOSX)
#include "content/browser/frame_host/popup_menu_helper_mac.h"
#include "content/browser/sandbox_parameters_mac.h"
#include "net/test/test_data_directory.h"
#endif

using blink::WebRect;
using blink::WebSize;

namespace content {

namespace {

RenderViewImpl* CreateWebViewTestProxy(CompositorDependencies* compositor_deps,
                                       const mojom::CreateViewParams& params) {
  test_runner::TestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();

  auto* render_view_proxy =
      new test_runner::WebViewTestProxy(compositor_deps, params);

  auto blink_test_runner = std::make_unique<BlinkTestRunner>(render_view_proxy);
  // TODO(lukasza): Using the first BlinkTestRunner as the main delegate is
  // wrong, but it is difficult to change because this behavior has been baked
  // for a long time into test assumptions (i.e. which PrintMessage gets
  // delivered to the browser depends on this).
  if (!interfaces->GetDelegate()) {
    interfaces->SetDelegate(blink_test_runner.get());
  }

  render_view_proxy->Initialize(interfaces, std::move(blink_test_runner));
  return render_view_proxy;
}

std::unique_ptr<RenderWidget> CreateRenderWidgetForFrame(
    int32_t routing_id,
    CompositorDependencies* compositor_deps,
    blink::mojom::DisplayMode display_mode,
    bool never_composited,
    mojo::PendingReceiver<mojom::Widget> widget_receiver) {
  return std::make_unique<test_runner::WebWidgetTestProxy>(
      routing_id, compositor_deps, display_mode,
      /*hidden=*/true, never_composited, std::move(widget_receiver));
}

RenderFrameImpl* CreateWebFrameTestProxy(RenderFrameImpl::CreateParams params) {
  // RenderFrameImpl always has a RenderViewImpl for it.
  RenderViewImpl* render_view_impl = params.render_view;

  auto* render_frame_proxy =
      new test_runner::WebFrameTestProxy(std::move(params));
  render_frame_proxy->Initialize(render_view_impl);
  return render_frame_proxy;
}

}  // namespace

test_runner::WebWidgetTestProxy* GetWebWidgetTestProxy(
    blink::WebLocalFrame* frame) {
  DCHECK(frame);
  RenderFrame* local_root = RenderFrame::FromWebFrame(frame->LocalRoot());
  RenderFrameImpl* local_root_impl = static_cast<RenderFrameImpl*>(local_root);
  DCHECK(local_root);

  return static_cast<test_runner::WebWidgetTestProxy*>(
      local_root_impl->GetLocalRootRenderWidget());
}

void EnableWebTestProxyCreation() {
  RenderViewImpl::InstallCreateHook(CreateWebViewTestProxy);
  RenderWidget::InstallCreateForFrameHook(CreateRenderWidgetForFrame);
  RenderFrameImpl::InstallCreateHook(CreateWebFrameTestProxy);
}

void FetchManifest(blink::WebView* view, FetchManifestCallback callback) {
  blink::WebManifestManager::RequestManifestForTesting(
      RenderFrameImpl::FromWebFrame(view->MainFrame())->GetWebFrame(),
      std::move(callback));
}

void SetWorkerRewriteURLFunction(RewriteURLFunction rewrite_url_function) {
  WebWorkerFetchContextImpl::InstallRewriteURLFunction(rewrite_url_function);
}

void EnableRendererWebTestMode() {
  RenderThreadImpl::current()->enable_web_test_mode();

  UniqueNameHelper::PreserveStableUniqueNameForTesting();
}

void EnableBrowserWebTestMode() {
#if defined(OS_MACOSX)
  PopupMenuHelper::DontShowPopupMenuForTesting();

  // Expand the network service sandbox to allow reading the test TLS
  // certificates.
  SetNetworkTestCertsDirectoryForTesting(net::GetTestCertsDirectory());
#endif
  RenderWidgetHostImpl::DisableResizeAckCheckForTesting();
}

int GetLocalSessionHistoryLength(RenderView* render_view) {
  return static_cast<RenderViewImpl*>(render_view)
      ->GetLocalSessionHistoryLengthForTesting();
}

void SetFocusAndActivate(RenderView* render_view, bool enable) {
  static_cast<RenderViewImpl*>(render_view)
      ->SetFocusAndActivateForTesting(enable);
}

void ForceResizeRenderView(RenderView* render_view, const WebSize& new_size) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  RenderFrameImpl* main_frame = render_view_impl->GetMainRenderFrame();
  if (!main_frame)
    return;
  RenderWidget* render_widget = main_frame->GetLocalRootRenderWidget();
  gfx::Rect window_rect(render_widget->WindowRect().x,
                        render_widget->WindowRect().y, new_size.width,
                        new_size.height);
  render_widget->SetWindowRectSynchronouslyForTesting(window_rect);
}

void SetDeviceScaleFactor(RenderView* render_view, float factor) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  RenderFrameImpl* main_frame = render_view_impl->GetMainRenderFrame();
  if (!main_frame)
    return;
  RenderWidget* render_widget = main_frame->GetLocalRootRenderWidget();
  render_widget->SetDeviceScaleFactorForTesting(factor);
}

std::unique_ptr<blink::WebInputEvent> TransformScreenToWidgetCoordinates(
    test_runner::WebWidgetTestProxy* web_widget_test_proxy,
    const blink::WebInputEvent& event) {
  DCHECK(web_widget_test_proxy);

  RenderWidget* render_widget =
      static_cast<RenderWidget*>(web_widget_test_proxy);

  // Compute the scale from window (dsf-independent) to blink (dsf-dependent
  // under UseZoomForDSF).
  blink::WebFloatRect rect(0, 0, 1.0f, 0.0);
  render_widget->ConvertWindowToViewport(&rect);
  float scale = rect.width;

  blink::WebRect view_rect = render_widget->ViewRect();
  gfx::Vector2d delta(-view_rect.x, -view_rect.y);

  // The coordinates are given in terms of the root widget, so adjust for the
  // position of the main frame.
  // TODO(sgilhuly): This doesn't work for events sent to OOPIFs because the
  // main frame is remote, and doesn't have a corresponding RenderWidget.
  // Currently none of those tests are run out of headless mode.
  blink::WebFrame* frame =
      web_widget_test_proxy->GetWebViewTestProxy()->GetWebView()->MainFrame();
  if (frame->IsWebLocalFrame()) {
    test_runner::WebWidgetTestProxy* root_widget =
        GetWebWidgetTestProxy(frame->ToWebLocalFrame());
    blink::WebRect root_rect = root_widget->ViewRect();
    gfx::Vector2d root_delta(root_rect.x, root_rect.y);
    delta.Add(root_delta);
  }

  return ui::TranslateAndScaleWebInputEvent(event, delta, scale);
}

gfx::ColorSpace GetTestingColorSpace(const std::string& name) {
  if (name == "genericRGB") {
    return gfx::ICCProfileForTestingGenericRGB().GetColorSpace();
  } else if (name == "sRGB") {
    return gfx::ColorSpace::CreateSRGB();
  } else if (name == "test" || name == "colorSpin") {
    return gfx::ICCProfileForTestingColorSpin().GetColorSpace();
  } else if (name == "adobeRGB") {
    return gfx::ICCProfileForTestingAdobeRGB().GetColorSpace();
  } else if (name == "reset") {
    return display::Display::GetForcedDisplayColorProfile();
  }
  return gfx::ColorSpace();
}

void SetDeviceColorSpace(RenderView* render_view,
                         const gfx::ColorSpace& color_space) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  RenderFrameImpl* main_frame = render_view_impl->GetMainRenderFrame();
  if (!main_frame)
    return;
  RenderWidget* render_widget = main_frame->GetLocalRootRenderWidget();
  render_widget->SetDeviceColorSpaceForTesting(color_space);
}

void SetTestBluetoothScanDuration(BluetoothTestScanDurationSetting setting) {
  switch (setting) {
    case BluetoothTestScanDurationSetting::kImmediateTimeout:
      BluetoothDeviceChooserController::SetTestScanDurationForTesting(
          BluetoothDeviceChooserController::TestScanDurationSetting::
              IMMEDIATE_TIMEOUT);
      break;
    case BluetoothTestScanDurationSetting::kNeverTimeout:
      BluetoothDeviceChooserController::SetTestScanDurationForTesting(
          BluetoothDeviceChooserController::TestScanDurationSetting::
              NEVER_TIMEOUT);
      break;
  }
}

void UseSynchronousResizeMode(RenderView* render_view, bool enable) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  RenderFrameImpl* main_frame = render_view_impl->GetMainRenderFrame();
  if (!main_frame)
    return;
  RenderWidget* render_widget = main_frame->GetLocalRootRenderWidget();
  render_widget->UseSynchronousResizeModeForTesting(enable);
}

void EnableAutoResizeMode(RenderView* render_view,
                          const WebSize& min_size,
                          const WebSize& max_size) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  RenderFrameImpl* main_frame = render_view_impl->GetMainRenderFrame();
  if (!main_frame)
    return;
  RenderWidget* render_widget = main_frame->GetLocalRootRenderWidget();
  render_widget->EnableAutoResizeForTesting(min_size, max_size);
}

void DisableAutoResizeMode(RenderView* render_view, const WebSize& new_size) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  RenderFrameImpl* main_frame = render_view_impl->GetMainRenderFrame();
  if (!main_frame)
    return;
  RenderWidget* render_widget = main_frame->GetLocalRootRenderWidget();
  render_widget->DisableAutoResizeForTesting(new_size);
}

void SchedulerRunIdleTasks(base::OnceClosure callback) {
  blink::scheduler::WebThreadScheduler* scheduler =
      content::RenderThreadImpl::current()->GetWebMainThreadScheduler();
  blink::scheduler::RunIdleTasksForTesting(scheduler, std::move(callback));
}

void ForceTextInputStateUpdateForRenderFrame(RenderFrame* frame) {
  RenderWidget* render_widget =
      static_cast<RenderFrameImpl*>(frame)->GetLocalRootRenderWidget();
  render_widget->ShowVirtualKeyboard();
}

}  // namespace content
