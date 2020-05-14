// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/test_runner_for_specific_view.h"

#include <stddef.h>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "content/common/widget_messages.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/renderer/render_frame.h"
#include "content/shell/common/web_test/web_test_string_util.h"
#include "content/shell/renderer/web_test/blink_test_runner.h"
#include "content/shell/renderer/web_test/layout_dump.h"
#include "content/shell/renderer/web_test/mock_content_settings_client.h"
#include "content/shell/renderer/web_test/mock_screen_orientation_client.h"
#include "content/shell/renderer/web_test/pixel_dump.h"
#include "content/shell/renderer/web_test/spell_check_client.h"
#include "content/shell/renderer/web_test/test_interfaces.h"
#include "content/shell/renderer/web_test/test_preferences.h"
#include "content/shell/renderer/web_test/test_runner.h"
#include "content/shell/renderer/web_test/web_view_test_proxy.h"
#include "content/shell/renderer/web_test/web_widget_test_proxy.h"
#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_isolated_world_ids.h"
#include "third_party/blink/public/platform/web_isolated_world_info.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_array_buffer.h"
#include "third_party/blink/public/web/web_array_buffer_converter.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_render_theme.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/switches.h"

namespace content {

TestRunnerForSpecificView::TestRunnerForSpecificView(
    WebViewTestProxy* web_view_test_proxy)
    : web_view_test_proxy_(web_view_test_proxy) {
  Reset();
}

TestRunnerForSpecificView::~TestRunnerForSpecificView() = default;

void TestRunnerForSpecificView::Reset() {
  pointer_locked_ = false;
  pointer_lock_planned_result_ = PointerLockWillSucceed;

  if (!web_view() || !web_view()->MainFrame())
    return;

  if (web_view()->MainFrame()->IsWebLocalFrame()) {
    web_view()->MainFrame()->ToWebLocalFrame()->EnableViewSourceMode(false);
    web_view()->SetTextZoomFactor(1);
    // As would the browser via IPC, set visibility on the RenderWidget then on
    // the Page.
    // TODO(danakj): This should set visibility on all RenderWidgets not just
    // the main frame.
    WidgetMsg_WasShown msg(main_frame_render_widget()->routing_id(),
                           /*show_request_timestamp=*/base::TimeTicks(),
                           /*was_evicted=*/false,
                           /*record_tab_switch_time_request=*/base::nullopt);
    main_frame_render_widget()->OnMessageReceived(msg);
  }
  web_view_test_proxy_->ApplyPageVisibilityState(
      content::PageVisibilityState::kVisible,
      /*initial_setting=*/true);
}

bool TestRunnerForSpecificView::RequestPointerLock() {
  switch (pointer_lock_planned_result_) {
    case PointerLockWillSucceed:
      PostTask(base::BindOnce(
          &TestRunnerForSpecificView::DidAcquirePointerLockInternal,
          weak_factory_.GetWeakPtr()));
      return true;
    case PointerLockWillRespondAsync:
      DCHECK(!pointer_locked_);
      return true;
    case PointerLockWillFailSync:
      DCHECK(!pointer_locked_);
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

void TestRunnerForSpecificView::RequestPointerUnlock() {
  PostTask(
      base::BindOnce(&TestRunnerForSpecificView::DidLosePointerLockInternal,
                     weak_factory_.GetWeakPtr()));
}

bool TestRunnerForSpecificView::isPointerLocked() {
  return pointer_locked_;
}

void TestRunnerForSpecificView::PostTask(base::OnceClosure callback) {
  // TODO(danakj): Use the frame that called the JS bindings to post the task.
  // not the main frame.
  blink::scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
      FROM_HERE, std::move(callback));
}

void TestRunnerForSpecificView::PostV8Callback(
    const v8::Local<v8::Function>& callback) {
  PostTask(base::BindOnce(&TestRunnerForSpecificView::InvokeV8Callback,
                          weak_factory_.GetWeakPtr(),
                          v8::UniquePersistent<v8::Function>(
                              blink::MainThreadIsolate(), callback)));
}

void TestRunnerForSpecificView::PostV8CallbackWithArgs(
    v8::UniquePersistent<v8::Function> callback,
    int argc,
    v8::Local<v8::Value> argv[]) {
  std::vector<v8::UniquePersistent<v8::Value>> args;
  for (int i = 0; i < argc; i++) {
    args.push_back(
        v8::UniquePersistent<v8::Value>(blink::MainThreadIsolate(), argv[i]));
  }

  PostTask(base::BindOnce(&TestRunnerForSpecificView::InvokeV8CallbackWithArgs,
                          weak_factory_.GetWeakPtr(), std::move(callback),
                          std::move(args)));
}

void TestRunnerForSpecificView::InvokeV8Callback(
    const v8::UniquePersistent<v8::Function>& callback) {
  std::vector<v8::UniquePersistent<v8::Value>> empty_args;
  InvokeV8CallbackWithArgs(callback, std::move(empty_args));
}

void TestRunnerForSpecificView::InvokeV8CallbackWithArgs(
    const v8::UniquePersistent<v8::Function>& callback,
    const std::vector<v8::UniquePersistent<v8::Value>>& args) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);

  blink::WebLocalFrame* frame = GetLocalMainFrame();
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;
  v8::Context::Scope context_scope(context);

  std::vector<v8::Local<v8::Value>> local_args;
  for (const auto& arg : args) {
    local_args.push_back(v8::Local<v8::Value>::New(isolate, arg));
  }

  frame->CallFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>::New(isolate, callback), context->Global(),
      local_args.size(), local_args.data());
}

base::OnceClosure TestRunnerForSpecificView::CreateClosureThatPostsV8Callback(
    const v8::Local<v8::Function>& callback) {
  return base::BindOnce(
      &TestRunnerForSpecificView::PostTask, weak_factory_.GetWeakPtr(),
      base::BindOnce(&TestRunnerForSpecificView::InvokeV8Callback,
                     weak_factory_.GetWeakPtr(),
                     v8::UniquePersistent<v8::Function>(
                         blink::MainThreadIsolate(), callback)));
}

void TestRunnerForSpecificView::CapturePixelsAsyncThen(
    v8::Local<v8::Function> callback) {
  v8::UniquePersistent<v8::Function> persistent_callback(
      blink::MainThreadIsolate(), callback);

  CHECK(web_view()->MainFrame()->IsWebLocalFrame())
      << "Web tests harness doesn't currently support running "
      << "testRuner.capturePixelsAsyncThen from an OOPIF";

  TestInterfaces* interfaces = web_view_test_proxy_->test_interfaces();

  if (interfaces->GetTestRunner()->CanDumpPixelsFromRenderer()) {
    // If we're grabbing pixels from printing, we do that in the renderer, and
    // some tests actually look at the results.
    interfaces->GetTestRunner()->DumpPixelsAsync(
        web_view_test_proxy_,
        base::BindOnce(&TestRunnerForSpecificView::RunJSCallbackWithBitmap,
                       weak_factory_.GetWeakPtr(),
                       std::move(persistent_callback)));
  } else {
    // If we're running the compositor lifecycle then the pixels aren't
    // available from the renderer, and they don't matter to tests.
    // TODO(crbug.com/952399): We could stop pretending they matter and split
    // this into a separate testRunner API that won't act like its returning
    // pixels.
    main_frame_render_widget()->RequestPresentation(base::BindOnce(
        &TestRunnerForSpecificView::RunJSCallbackAfterCompositorLifecycle,
        weak_factory_.GetWeakPtr(), std::move(persistent_callback)));
  }
}

void TestRunnerForSpecificView::RunJSCallbackAfterCompositorLifecycle(
    v8::UniquePersistent<v8::Function> callback,
    const gfx::PresentationFeedback&) {
  // TODO(crbug.com/952399): We're not testing pixels on this path, remove the
  // SkBitmap plumbing entirely and rename CapturePixels* to RunLifecycle*.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(0);
  RunJSCallbackWithBitmap(std::move(callback), bitmap);
}

void TestRunnerForSpecificView::RunJSCallbackWithBitmap(
    v8::UniquePersistent<v8::Function> callback,
    const SkBitmap& snapshot) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context =
      GetLocalMainFrame()->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Value> argv[3];

  // Size can be 0 for cases where copyImageAt was called on position
  // that doesn't have an image.
  int width = snapshot.info().width();
  argv[0] = v8::Number::New(isolate, width);

  int height = snapshot.info().height();
  argv[1] = v8::Number::New(isolate, height);

  // Skia's internal byte order is platform-dependent. Always convert to RGBA
  // in order to provide a consistent ordering to the web tests.
  const SkImageInfo bufferInfo =
      snapshot.info().makeColorType(kRGBA_8888_SkColorType);
  const size_t bufferRowBytes = bufferInfo.minRowBytes();
  blink::WebArrayBuffer buffer = blink::WebArrayBuffer::Create(
      bufferInfo.computeByteSize(bufferRowBytes), 1);
  if (!snapshot.readPixels(bufferInfo, buffer.Data(), bufferRowBytes, 0, 0)) {
    // We only expect readPixels to fail for null bitmaps.
    DCHECK(snapshot.isNull());
  }

  argv[2] = blink::WebArrayBufferConverter::ToV8Value(
      &buffer, context->Global(), isolate);

  PostV8CallbackWithArgs(std::move(callback), base::size(argv), argv);
}

void TestRunnerForSpecificView::GetBluetoothManualChooserEvents(
    v8::Local<v8::Function> callback) {
  return blink_test_runner()->GetBluetoothManualChooserEvents(base::BindOnce(
      &TestRunnerForSpecificView::GetBluetoothManualChooserEventsCallback,
      weak_factory_.GetWeakPtr(),
      v8::UniquePersistent<v8::Function>(blink::MainThreadIsolate(),
                                         callback)));
}

void TestRunnerForSpecificView::GetBluetoothManualChooserEventsCallback(
    v8::UniquePersistent<v8::Function> callback,
    const std::vector<std::string>& events) {
  // Build the V8 context.
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      GetLocalMainFrame()->MainWorldScriptContext();
  if (context.IsEmpty())
    return;
  v8::Context::Scope context_scope(context);

  // Convert the argument.
  v8::Local<v8::Value> arg;
  if (!gin::TryConvertToV8(isolate, events, &arg))
    return;

  // Call the callback.
  PostV8CallbackWithArgs(std::move(callback), 1, &arg);
}

void TestRunnerForSpecificView::SetPageVisibility(
    const std::string& new_visibility) {
  content::PageVisibilityState visibility;
  if (new_visibility == "visible") {
    visibility = content::PageVisibilityState::kVisible;
  } else if (new_visibility == "hidden") {
    visibility = content::PageVisibilityState::kHidden;
  } else {
    return;
  }

  // As would the browser via IPC, set visibility on the RenderWidget then on
  // the Page.
  // TODO(danakj): This should set visibility on all RenderWidgets not just the
  // main frame.
  if (visibility == content::PageVisibilityState::kVisible) {
    WidgetMsg_WasShown msg(main_frame_render_widget()->routing_id(),
                           /*show_request_timestamp=*/base::TimeTicks(),
                           /*was_evicted=*/false,
                           /*record_tab_switch_time_request=*/base::nullopt);
    main_frame_render_widget()->OnMessageReceived(msg);
  } else {
    WidgetMsg_WasHidden msg(main_frame_render_widget()->routing_id());
    main_frame_render_widget()->OnMessageReceived(msg);
  }
  web_view_test_proxy_->ApplyPageVisibilityState(visibility,
                                                 /*initial_setting=*/false);
}

void TestRunnerForSpecificView::DidAcquirePointerLock() {
  DidAcquirePointerLockInternal();
}

void TestRunnerForSpecificView::DidNotAcquirePointerLock() {
  DidNotAcquirePointerLockInternal();
}

void TestRunnerForSpecificView::DidLosePointerLock() {
  DidLosePointerLockInternal();
}

void TestRunnerForSpecificView::SetPointerLockWillFailSynchronously() {
  pointer_lock_planned_result_ = PointerLockWillFailSync;
}

void TestRunnerForSpecificView::SetPointerLockWillRespondAsynchronously() {
  pointer_lock_planned_result_ = PointerLockWillRespondAsync;
}

void TestRunnerForSpecificView::DidAcquirePointerLockInternal() {
  pointer_locked_ = true;
  web_view()->MainFrameWidget()->DidAcquirePointerLock();

  // Reset planned result to default.
  pointer_lock_planned_result_ = PointerLockWillSucceed;
}

void TestRunnerForSpecificView::DidNotAcquirePointerLockInternal() {
  DCHECK(!pointer_locked_);
  pointer_locked_ = false;
  web_view()->MainFrameWidget()->DidNotAcquirePointerLock();

  // Reset planned result to default.
  pointer_lock_planned_result_ = PointerLockWillSucceed;
}

void TestRunnerForSpecificView::DidLosePointerLockInternal() {
  bool was_locked = pointer_locked_;
  pointer_locked_ = false;
  if (was_locked)
    web_view()->MainFrameWidget()->DidLosePointerLock();
}

v8::Local<v8::Value>
TestRunnerForSpecificView::EvaluateScriptInIsolatedWorldAndReturnValue(
    int32_t world_id,
    const std::string& script) {
  blink::WebScriptSource source(blink::WebString::FromUTF8(script));
  // This relies on the iframe focusing itself when it loads. This is a bit
  // sketchy, but it seems to be what other tests do.
  v8::Local<v8::Value> value =
      web_view()->FocusedFrame()->ExecuteScriptInIsolatedWorldAndReturnValue(
          world_id, source);
  if (!value.IsEmpty())
    return value;
  return v8::Local<v8::Value>();
}

void TestRunnerForSpecificView::EvaluateScriptInIsolatedWorld(
    int32_t world_id,
    const std::string& script) {
  blink::WebScriptSource source(blink::WebString::FromUTF8(script));
  web_view()->FocusedFrame()->ExecuteScriptInIsolatedWorld(world_id, source);
}

void TestRunnerForSpecificView::SetIsolatedWorldInfo(
    int32_t world_id,
    v8::Local<v8::Value> security_origin,
    v8::Local<v8::Value> content_security_policy) {
  if (world_id <= content::ISOLATED_WORLD_ID_GLOBAL ||
      world_id >= blink::IsolatedWorldId::kEmbedderWorldIdLimit) {
    return;
  }

  if (!security_origin->IsString() && !security_origin->IsNull())
    return;

  if (!content_security_policy->IsString() &&
      !content_security_policy->IsNull()) {
    return;
  }

  // If |content_security_policy| is specified, |security_origin| must also be
  // specified.
  if (content_security_policy->IsString() && security_origin->IsNull())
    return;

  blink::WebIsolatedWorldInfo info;
  if (security_origin->IsString()) {
    info.security_origin = blink::WebSecurityOrigin::CreateFromString(
        web_test_string_util::V8StringToWebString(
            blink::MainThreadIsolate(), security_origin.As<v8::String>()));
  }

  if (content_security_policy->IsString()) {
    info.content_security_policy = web_test_string_util::V8StringToWebString(
        blink::MainThreadIsolate(), content_security_policy.As<v8::String>());
  }

  // Clear the document->isolated world CSP mapping.
  web_view()->FocusedFrame()->ClearIsolatedWorldCSPForTesting(world_id);

  web_view()->FocusedFrame()->SetIsolatedWorldInfo(world_id, info);
}

void TestRunner::InsertStyleSheet(const std::string& source_code) {
  blink::WebLocalFrame::FrameForCurrentContext()
      ->GetDocument()
      .InsertStyleSheet(blink::WebString::FromUTF8(source_code));
}

blink::WebLocalFrame* TestRunnerForSpecificView::GetLocalMainFrame() {
  if (!web_view()->MainFrame()->IsWebLocalFrame()) {
    // Hitting the check below uncovers a new scenario that requires OOPIF
    // support in the web tests harness.
    CHECK(false) << "This function cannot be called if the main frame is not a "
                    "local frame.";
  }
  return web_view()->MainFrame()->ToWebLocalFrame();
}

WebWidgetTestProxy* TestRunnerForSpecificView::main_frame_render_widget() {
  return static_cast<WebWidgetTestProxy*>(
      web_view_test_proxy_->GetMainRenderFrame()->GetLocalRootRenderWidget());
}

blink::WebView* TestRunnerForSpecificView::web_view() {
  return web_view_test_proxy_->GetWebView();
}

BlinkTestRunner* TestRunnerForSpecificView::blink_test_runner() {
  return web_view_test_proxy_->blink_test_runner();
}

}  // namespace content
