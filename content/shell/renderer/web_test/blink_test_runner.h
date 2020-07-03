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
#include "content/shell/common/web_test/web_test_bluetooth_fake_adapter_setter.mojom.h"
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

  // Set the bluetooth adapter while running a web test, uses Mojo to
  // communicate with the browser.
  void SetBluetoothFakeAdapter(const std::string& adapter_name,
                               base::OnceClosure callback);

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
  void CaptureDump(mojom::WebTestRenderFrame::CaptureDumpCallback callback);
  void DidCommitNavigationInMainFrame();
  void OnResetRendererAfterWebTest();
  void OnFinishTestInMainWindow();
  void OnLayoutDumpCompleted(std::string completed_layout_dump);

 private:
  // Helper reused by OnSetTestConfiguration and OnReplicateTestConfiguration.
  void ApplyTestConfiguration(mojom::WebTestRunTestConfigurationPtr params);

  // After finishing the test, retrieves the audio, text, and pixel dumps from
  // the TestRunner library and sends them to the browser process.
  void OnPixelsDumpCompleted(const SkBitmap& snapshot);
  void CaptureDumpComplete();
  void CaptureLocalAudioDump();
  void CaptureLocalLayoutDump();
  void CaptureLocalPixelsDump();

  mojom::WebTestBluetoothFakeAdapterSetter& GetBluetoothFakeAdapterSetter();
  mojo::Remote<mojom::WebTestBluetoothFakeAdapterSetter>
      bluetooth_fake_adapter_setter_;

  mojo::AssociatedRemote<mojom::WebTestControlHost>&
  GetWebTestControlHostRemote();
  mojo::AssociatedRemote<mojom::WebTestClient>& GetWebTestClientRemote();

  WebViewTestProxy* const web_view_test_proxy_;

  mojom::WebTestRunTestConfigurationPtr test_config_;

  bool is_main_window_ = false;
  bool waiting_for_reset_navigation_to_about_blank_ = false;

  mojom::WebTestRenderFrame::CaptureDumpCallback dump_callback_;
  mojom::WebTestDumpPtr dump_result_;
  bool waiting_for_layout_dump_results_ = false;
  bool waiting_for_pixels_dump_result_ = false;

  DISALLOW_COPY_AND_ASSIGN(BlinkTestRunner);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_BLINK_TEST_RUNNER_H_
