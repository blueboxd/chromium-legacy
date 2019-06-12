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
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/common/renderer.mojom.h"
#include "content/common/unique_name_helper.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/screen_info.h"
#include "content/renderer/compositor/layer_tree_view.h"
#include "content/renderer/input/render_widget_input_handler_delegate.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/web_worker_fetch_context_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/common/web_test/web_test_switches.h"
#include "content/shell/renderer/web_test/blink_test_runner.h"
#include "content/shell/renderer/web_test/web_test_render_thread_observer.h"
#include "content/shell/test_runner/test_common.h"
#include "content/shell/test_runner/web_frame_test_proxy.h"
#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_input_event.h"
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
#elif defined(OS_WIN)
#include "content/child/font_warmup_win.h"
#include "third_party/blink/public/web/win/web_font_rendering.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"
#include "ui/gfx/win/direct_write.h"
#endif

using blink::WebRect;
using blink::WebSize;

namespace content {

namespace {

RenderViewImpl* CreateWebViewTestProxy(CompositorDependencies* compositor_deps,
                                       const mojom::CreateViewParams& params) {
  test_runner::WebTestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();

  auto* render_view_proxy =
      new test_runner::WebViewTestProxy(compositor_deps, params);

  auto test_runner = std::make_unique<BlinkTestRunner>(render_view_proxy);
  // TODO(lukasza): Using the 1st BlinkTestRunner as the main delegate is wrong,
  // but it is difficult to change because this behavior has been baked for a
  // long time into test assumptions (i.e. which PrintMessage gets delivered to
  // the browser depends on this).
  static bool first_test_runner = true;
  if (first_test_runner) {
    first_test_runner = false;
    interfaces->SetDelegate(test_runner.get());
  }

  render_view_proxy->Initialize(interfaces, std::move(test_runner));
  return render_view_proxy;
}

scoped_refptr<RenderWidget> CreateRenderWidgetForFrame(
    int32_t routing_id,
    CompositorDependencies* compositor_deps,
    const ScreenInfo& screen_info,
    blink::WebDisplayMode display_mode,
    bool swapped_out,
    bool hidden,
    bool never_visible,
    mojom::WidgetRequest widget_request) {
  return base::MakeRefCounted<test_runner::WebWidgetTestProxy>(
      routing_id, compositor_deps, screen_info, display_mode, swapped_out,
      hidden, never_visible, std::move(widget_request));
}

RenderFrameImpl* CreateWebFrameTestProxy(RenderFrameImpl::CreateParams params) {
  test_runner::WebTestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();

  // RenderFrameImpl always has a RenderViewImpl for it.
  RenderViewImpl* render_view_impl = params.render_view;

  auto* render_frame_proxy =
      new test_runner::WebFrameTestProxy(std::move(params));
  render_frame_proxy->Initialize(interfaces, render_view_impl);
  return render_frame_proxy;
}

float GetWindowToViewportScale(RenderWidget* render_widget) {
  blink::WebFloatRect rect(0, 0, 1.0f, 0.0);
  render_widget->ConvertWindowToViewport(&rect);
  return rect.width;
}

#if defined(OS_WIN)
// DirectWrite only has access to %WINDIR%\Fonts by default. For developer
// side-loading, support kRegisterFontFiles to allow access to additional fonts.
void RegisterSideloadedTypefaces(SkFontMgr* fontmgr) {
  for (const auto& file : switches::GetSideloadFontFiles()) {
    blink::WebFontRendering::AddSideloadedFontForTesting(
        fontmgr->makeFromFile(file.c_str()));
  }
}
#endif  // OS_WIN

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

#if defined(OS_WIN)
  RegisterSideloadedTypefaces(SkFontMgr_New_DirectWrite().get());
#endif
}

void EnableBrowserWebTestMode() {
#if defined(OS_MACOSX)
  PopupMenuHelper::DontShowPopupMenuForTesting();
#endif
  RenderWidgetHostImpl::DisableResizeAckCheckForTesting();
}

void TerminateAllSharedWorkersForTesting(StoragePartition* storage_partition,
                                         base::OnceClosure callback) {
  static_cast<SharedWorkerServiceImpl*>(
      storage_partition->GetSharedWorkerService())
      ->TerminateAllWorkersForTesting(std::move(callback));
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
  auto* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  RenderWidget* render_widget = render_view_impl->GetWidget();
  gfx::Rect window_rect(render_widget->WindowRect().x,
                        render_widget->WindowRect().y, new_size.width,
                        new_size.height);
  render_widget->SetWindowRectSynchronouslyForTesting(window_rect);
}

void SetDeviceScaleFactor(RenderView* render_view, float factor) {
  RenderWidget* render_widget =
      static_cast<RenderViewImpl*>(render_view)->GetWidget();
  render_widget->SetDeviceScaleFactorForTesting(factor);
}

float GetWindowToViewportScale(RenderView* render_view) {
  return GetWindowToViewportScale(
      static_cast<RenderViewImpl*>(render_view)->GetWidget());
}

std::unique_ptr<blink::WebInputEvent> TransformScreenToWidgetCoordinates(
    test_runner::WebWidgetTestProxy* web_widget_test_proxy,
    const blink::WebInputEvent& event) {
  DCHECK(web_widget_test_proxy);

  RenderWidget* render_widget =
      static_cast<RenderWidget*>(web_widget_test_proxy);

  blink::WebRect view_rect = render_widget->ViewRect();
  float scale = GetWindowToViewportScale(render_widget);
  gfx::Vector2d delta(-view_rect.x, -view_rect.y);
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
  RenderWidget* render_widget =
      static_cast<RenderViewImpl*>(render_view)->GetWidget();
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
  RenderWidget* render_widget =
      static_cast<RenderViewImpl*>(render_view)->GetWidget();
  render_widget->UseSynchronousResizeModeForTesting(enable);
}

void EnableAutoResizeMode(RenderView* render_view,
                          const WebSize& min_size,
                          const WebSize& max_size) {
  RenderWidget* render_widget =
      static_cast<RenderViewImpl*>(render_view)->GetWidget();
  render_widget->EnableAutoResizeForTesting(min_size, max_size);
}

void DisableAutoResizeMode(RenderView* render_view, const WebSize& new_size) {
  RenderWidget* render_widget =
      static_cast<RenderViewImpl*>(render_view)->GetWidget();
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
