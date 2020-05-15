// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_TEST_RUNNER_FOR_SPECIFIC_VIEW_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_TEST_RUNNER_FOR_SPECIFIC_VIEW_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "v8/include/v8.h"

class SkBitmap;

namespace blink {
class WebLocalFrame;
class WebView;
}  // namespace blink

namespace gfx {
struct PresentationFeedback;
}

namespace content {
class BlinkTestRunner;
class WebWidgetTestProxy;
class WebViewTestProxy;

// TestRunnerForSpecificView implements part of |testRunner| javascript bindings
// that work with a view where the javascript call originated from.  Examples:
// - testRunner.capturePixelsAsyncThen
// - testRunner.setPageVisibility
// Note that "global" bindings are handled by TestRunner class.
class TestRunnerForSpecificView {
 public:
  explicit TestRunnerForSpecificView(WebViewTestProxy* web_view_test_proxy);
  ~TestRunnerForSpecificView();

  void Reset();

  // Pointer lock methods used by WebViewTestClient.
  bool RequestPointerLock();
  void RequestPointerUnlock();
  bool isPointerLocked();

  // Helpers for working with base and V8 callbacks.
  void PostV8CallbackWithArgs(v8::UniquePersistent<v8::Function> callback,
                              int argc,
                              v8::Local<v8::Value> argv[]);
  void InvokeV8CallbackWithArgs(
      const v8::UniquePersistent<v8::Function>& callback,
      const std::vector<v8::UniquePersistent<v8::Value>>& args);

  void RunJSCallbackAfterCompositorLifecycle(
      v8::UniquePersistent<v8::Function> callback,
      const gfx::PresentationFeedback&);
  void RunJSCallbackWithBitmap(v8::UniquePersistent<v8::Function> callback,
                               const SkBitmap& snapshot);

 private:
  friend class TestRunnerBindings;

  void PostTask(base::OnceClosure callback);

  // The callback will be called after the next full frame update and raster,
  // with the captured snapshot as the parameters (width, height, snapshot).
  // The snapshot is in uint8_t RGBA format.
  void CapturePixelsAsyncThen(v8::Local<v8::Function> callback);

  // Similar to CapturePixelsAsyncThen(). Copies to the clipboard the image
  // located at a particular point in the WebView (if there is such an image),
  // reads back its pixels, and provides the snapshot to the callback. If there
  // is no image at that point, calls the callback with (0, 0, empty_snapshot).
  void CopyImageAtAndCapturePixelsAsyncThen(
      int x,
      int y,
      const v8::Local<v8::Function> callback);

  // Switch the visibility of the page.
  void SetPageVisibility(const std::string& new_visibility);

  // Changes the direction of the focused element.
  void SetTextDirection(const std::string& direction_name);

  // Pointer lock handling.
  void DidAcquirePointerLock();
  void DidNotAcquirePointerLock();
  void DidLosePointerLock();
  void SetPointerLockWillFailSynchronously();
  void SetPointerLockWillRespondAsynchronously();
  void DidAcquirePointerLockInternal();
  void DidNotAcquirePointerLockInternal();
  void DidLosePointerLockInternal();
  bool pointer_locked_;
  enum {
    PointerLockWillSucceed,
    PointerLockWillRespondAsync,
    PointerLockWillFailSync,
  } pointer_lock_planned_result_;

  // Many parts of the web test harness assume that the main frame is local.
  // Having all of them go through the helper below makes it easier to catch
  // scenarios that require breaking this assumption.
  blink::WebLocalFrame* GetLocalMainFrame();

  // Helpers for accessing pointers exposed by |web_view_test_proxy_|.
  WebWidgetTestProxy* main_frame_render_widget();
  blink::WebView* web_view();
  BlinkTestRunner* blink_test_runner();

  WebViewTestProxy* web_view_test_proxy_;

  base::WeakPtrFactory<TestRunnerForSpecificView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestRunnerForSpecificView);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_TEST_RUNNER_FOR_SPECIFIC_VIEW_H_
