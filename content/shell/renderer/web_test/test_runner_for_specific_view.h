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
struct Manifest;
class WebLocalFrame;
class WebView;
class WebURL;
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
  void PostV8Callback(const v8::Local<v8::Function>& callback);
  void PostV8CallbackWithArgs(v8::UniquePersistent<v8::Function> callback,
                              int argc,
                              v8::Local<v8::Value> argv[]);
  void InvokeV8Callback(const v8::UniquePersistent<v8::Function>& callback);
  void InvokeV8CallbackWithArgs(
      const v8::UniquePersistent<v8::Function>& callback,
      const std::vector<v8::UniquePersistent<v8::Value>>& args);
  base::OnceClosure CreateClosureThatPostsV8Callback(
      const v8::Local<v8::Function>& callback);

 private:
  friend class TestRunnerBindings;

  void PostTask(base::OnceClosure callback);

  void UpdateAllLifecyclePhasesAndComposite();
  void UpdateAllLifecyclePhasesAndCompositeThen(
      v8::Local<v8::Function> callback);

  // The callback will be called after the next full frame update and raster,
  // with the captured snapshot as the parameters (width, height, snapshot).
  // The snapshot is in uint8_t RGBA format.
  void CapturePixelsAsyncThen(v8::Local<v8::Function> callback);

  void RunJSCallbackAfterCompositorLifecycle(
      v8::UniquePersistent<v8::Function> callback,
      const gfx::PresentationFeedback&);
  void RunJSCallbackWithBitmap(v8::UniquePersistent<v8::Function> callback,
                               const SkBitmap& snapshot);

  // Similar to CapturePixelsAsyncThen(). Copies to the clipboard the image
  // located at a particular point in the WebView (if there is such an image),
  // reads back its pixels, and provides the snapshot to the callback. If there
  // is no image at that point, calls the callback with (0, 0, empty_snapshot).
  void CopyImageAtAndCapturePixelsAsyncThen(
      int x,
      int y,
      const v8::Local<v8::Function> callback);

  void GetManifestThen(v8::Local<v8::Function> callback);
  void GetManifestCallback(v8::UniquePersistent<v8::Function> callback,
                           const blink::WebURL& manifest_url,
                           const blink::Manifest& manifest);

  // Calls |callback| with a DOMString[] representing the events recorded since
  // the last call to this function.
  void GetBluetoothManualChooserEvents(v8::Local<v8::Function> callback);
  void GetBluetoothManualChooserEventsCallback(
      v8::UniquePersistent<v8::Function> callback,
      const std::vector<std::string>& events);

  // Change the bluetooth test data while running a web test.
  void SetBluetoothFakeAdapter(const std::string& adapter_name,
                               v8::Local<v8::Function> callback);

  // If |enable| is true, makes the Bluetooth chooser record its input and wait
  // for instructions from the test program on how to proceed. Otherwise falls
  // back to the browser's default chooser.
  void SetBluetoothManualChooser(bool enable);

  // Calls the BluetoothChooser::EventHandler with the arguments here. Valid
  // event strings are:
  //  * "cancel" - simulates the user canceling the chooser.
  //  * "select" - simulates the user selecting a device whose device ID is in
  //               |argument|.
  void SendBluetoothManualChooserEvent(const std::string& event,
                                       const std::string& argument);

  // Immediately run all pending idle tasks, including all pending
  // requestIdleCallback calls.  Invoke the callback when all
  // idle tasks are complete.
  void RunIdleTasks(v8::Local<v8::Function> callback);

  // Checks if an internal command is currently available.
  bool IsCommandEnabled(const std::string& command);

  // Forces the selection colors for testing under Linux.
  void ForceRedSelectionColors();

  // Switch the visibility of the page.
  void SetPageVisibility(const std::string& new_visibility);

  // Changes the direction of the focused element.
  void SetTextDirection(const std::string& direction_name);

  // Permits the adding and removing of only one opaque overlay.
  void AddWebPageOverlay();
  void RemoveWebPageOverlay();

  void SetHighlightAds(bool);

  // Sets a flag causing the next call to WebGLRenderingContext::create to fail.
  void ForceNextWebGLContextCreationToFail();

  // Sets a flag causing the next call to DrawingBuffer::create to fail.
  void ForceNextDrawingBufferCreationToFail();

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

  void SetDomainRelaxationForbiddenForURLScheme(bool forbidden,
                                                const std::string& scheme);
  v8::Local<v8::Value> EvaluateScriptInIsolatedWorldAndReturnValue(
      int32_t world_id,
      const std::string& script);
  void EvaluateScriptInIsolatedWorld(int32_t world_id,
                                     const std::string& script);
  void SetIsolatedWorldInfo(int32_t world_id,
                            v8::Local<v8::Value> security_origin,
                            v8::Local<v8::Value> content_security_policy);
  bool FindString(const std::string& search_text,
                  const std::vector<std::string>& options_array);
  std::string SelectionAsMarkup();
  void SetViewSourceForFrame(const std::string& name, bool enabled);

  // Sets the network service-global Trust Tokens key commitments.
  // |raw_commitments| should be JSON-encoded according to the format expected
  // by NetworkService::SetTrustTokenKeyCommitments.
  void SetTrustTokenKeyCommitments(const std::string& raw_commitments,
                                   v8::Local<v8::Function> callback);

  // Clears persistent Trust Tokens state
  // (https://github.com/wicg/trust-token-api) via a test-only Mojo interface.
  void ClearTrustTokenState(v8::Local<v8::Function> callback);

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
