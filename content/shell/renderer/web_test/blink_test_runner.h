// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_BLINK_TEST_RUNNER_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_BLINK_TEST_RUNNER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "content/public/common/page_state.h"
#include "content/shell/common/web_test/web_test.mojom.h"
#include "content/shell/renderer/web_test/layout_dump.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "url/origin.h"
#include "v8/include/v8.h"

class SkBitmap;

namespace content {
class WebViewTestProxy;

// An instance of this class is attached to each RenderView in each renderer
// process during a web test. It handles IPCs (forwarded from
// WebTestRenderFrameObserver) from the browser to manage the web test state
// machine.
class BlinkTestRunner {
 public:
  explicit BlinkTestRunner(WebViewTestProxy* web_view_test_proxy);
  ~BlinkTestRunner();

  // Convert the provided relative path into an absolute path.
  blink::WebString GetAbsoluteWebStringFromUTF8Path(const std::string& path);

  // Invoked when the test finished.
  void TestFinished();

  // Returns the length of the back/forward history of the main WebView.
  int NavigationEntryCount();

  // Returns true if resource requests to external URLs should be permitted.
  bool AllowExternalPages();

  // Causes the beforeinstallprompt event to be sent to the renderer.
  // |event_platforms| are the platforms to be sent with the event. Once the
  // event listener completes, |callback| will be called with a boolean
  // argument. This argument will be true if the event is canceled, and false
  // otherwise.
  void DispatchBeforeInstallPromptEvent(
      const std::vector<std::string>& event_platforms,
      base::OnceCallback<void(bool)> callback);

  // Message handlers forwarded by WebTestRenderFrameObserver.
  void OnSetTestConfiguration(mojom::WebTestRunTestConfigurationPtr params);
  void OnReplicateTestConfiguration(
      mojom::WebTestRunTestConfigurationPtr params);
  void OnSetupRendererProcessForNonTestWindow();
  void DidCommitNavigationInMainFrame();
  void OnResetRendererAfterWebTest();
  void OnFinishTestInMainWindow();

  // True if the RenderView is hosting a frame tree fragment that is part of the
  // web test harness' main window.
  bool is_main_window() const { return is_main_window_; }

 private:
  // Helper reused by OnSetTestConfiguration and OnReplicateTestConfiguration.
  void ApplyTestConfiguration(mojom::WebTestRunTestConfigurationPtr params);

  // Grabs the audio results. This is only called when audio results are
  // known to be present.
  std::vector<uint8_t> CaptureLocalAudioDump();
  // Returns a string if able to capture the dump locally. If not, then the
  // browser must do the capture.
  base::Optional<std::string> CaptureLocalLayoutDump(TextResultType type);
  // Grabs the pixel results. This is only called when pixel results are being
  // captured in the renderer (aka CanDumpPixelsFromRenderer() is true), such as
  // to grab the current image being dragged by the mouse.
  SkBitmap CaptureLocalPixelsDump();
  // Returns the current selection rect if it should be drawn in the pixel
  // results, or an empty rect.
  gfx::Rect CaptureLocalMainFrameSelectionRect();

  mojo::AssociatedRemote<mojom::WebTestControlHost>&
  GetWebTestControlHostRemote();
  mojo::AssociatedRemote<mojom::WebTestClient>& GetWebTestClientRemote();

  WebViewTestProxy* const web_view_test_proxy_;

  mojom::WebTestRunTestConfigurationPtr test_config_;

  bool is_main_window_ = false;
  bool waiting_for_reset_navigation_to_about_blank_ = false;

  DISALLOW_COPY_AND_ASSIGN(BlinkTestRunner);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_BLINK_TEST_RUNNER_H_
