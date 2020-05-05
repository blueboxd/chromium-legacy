// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_BLINK_TEST_RUNNER_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_BLINK_TEST_RUNNER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
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
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "url/origin.h"
#include "v8/include/v8.h"

class SkBitmap;

namespace base {
class DictionaryValue;
}  // namespace base

namespace content {
class WebViewTestProxy;
struct WebPreferences;

// An instance of this class is attached to each RenderView in each renderer
// process during a web test. It handles IPCs (forwarded from
// WebTestRenderFrameObserver) from the browser to manage the web test state
// machine.
class BlinkTestRunner {
 public:
  explicit BlinkTestRunner(WebViewTestProxy* web_view_test_proxy);
  ~BlinkTestRunner();

  // Add a message to stderr (not saved to expected output files, for debugging
  // only).
  void PrintMessageToStderr(const std::string& message);

  // Add a message to the text dump for the web test.
  void PrintMessage(const std::string& message);

  // Register a new isolated filesystem with the given files, and return the
  // new filesystem id.
  blink::WebString RegisterIsolatedFileSystem(
      const blink::WebVector<blink::WebString>& absolute_filenames);

  // Convert the provided relative path into an absolute path.
  blink::WebString GetAbsoluteWebStringFromUTF8Path(const std::string& path);

  void SetPopupBlockingEnabled(bool block_popups);

  // Controls WebSQL databases.
  void ClearAllDatabases();
  // Setting quota to kDefaultDatabaseQuota will reset it to the default value.
  void SetDatabaseQuota(int quota);

  // Controls Web Notifications.
  void SimulateWebNotificationClick(
      const std::string& title,
      const base::Optional<int>& action_index,
      const base::Optional<base::string16>& reply);
  void SimulateWebNotificationClose(const std::string& title, bool by_user);

  // Controls Content Index entries.
  void SimulateWebContentIndexDelete(const std::string& id);

  // Set the bluetooth adapter while running a web test, uses Mojo to
  // communicate with the browser.
  void SetBluetoothFakeAdapter(const std::string& adapter_name,
                               base::OnceClosure callback);

  // If |enable| is true makes the Bluetooth chooser record its input and wait
  // for instructions from the test program on how to proceed. Otherwise
  // fall backs to the browser's default chooser.
  void SetBluetoothManualChooser(bool enable);

  // Returns the events recorded since the last call to this function.
  void GetBluetoothManualChooserEvents(
      base::OnceCallback<void(const std::vector<std::string>& events)>
          callback);

  // Calls the BluetoothChooser::EventHandler with the arguments here. Valid
  // event strings are:
  //  * "cancel" - simulates the user canceling the chooser.
  //  * "select" - simulates the user selecting a device whose device ID is in
  //               |argument|.
  void SendBluetoothManualChooserEvent(const std::string& event,
                                       const std::string& argument);

  // Controls whether all cookies should be accepted or writing cookies in a
  // third-party context is blocked.
  void SetBlockThirdPartyCookies(bool block);

  // Sets the POSIX locale of the current process.
  void SetLocale(const std::string& locale);

  // Returns the absolute path to a directory this test can write data in. This
  // returns the path to a fresh empty directory for each test that calls this
  // method, but repeatedly calling this from the same test will return the same
  // directory.
  base::FilePath GetWritableDirectory();

  // Sets the path that should be returned when the test shows a file dialog.
  void SetFilePathForMockFileDialog(const base::FilePath& path);

  // Invoked when web test runtime flags change.
  void OnWebTestRuntimeFlagsChanged(
      const base::DictionaryValue& changed_values);

  // Invoked when the test finished.
  void TestFinished();

  // Invoked when the embedder should close all but the main WebView.
  void CloseRemainingWindows();

  void DeleteAllCookies();

  // Returns the length of the back/forward history of the main WebView.
  int NavigationEntryCount();

  // The following trigger navigations on the main WebView.
  void GoToOffset(int offset);
  void Reload();
  void LoadURLForFrame(const blink::WebURL& url, const std::string& frame_name);

  // Returns true if resource requests to external URLs should be permitted.
  bool AllowExternalPages();

  // Sends a message to the WebTestPermissionManager in order for it to
  // update its database.
  void SetPermission(const std::string& permission_name,
                     const std::string& permission_value,
                     const GURL& origin,
                     const GURL& embedding_origin);

  // Clear all the permissions set via SetPermission().
  void ResetPermissions();

  // Causes the beforeinstallprompt event to be sent to the renderer.
  // |event_platforms| are the platforms to be sent with the event. Once the
  // event listener completes, |callback| will be called with a boolean
  // argument. This argument will be true if the event is canceled, and false
  // otherwise.
  void DispatchBeforeInstallPromptEvent(
      const std::vector<std::string>& event_platforms,
      base::OnceCallback<void(bool)> callback);

  // Mark the orientation changed for fullscreen layout tests.
  void SetScreenOrientationChanged();

  // Sets the network service-global Trust Tokens key commitments.
  // |raw_commitments| should be JSON-encoded according to the format expected
  // by NetworkService::SetTrustTokenKeyCommitments.
  void SetTrustTokenKeyCommitments(const std::string& raw_commitments,
                                   base::OnceClosure callback);

  // Clears persistent Trust Token API state
  // (https://github.com/wicg/trust-token-api).
  void ClearTrustTokenState(base::OnceClosure callback);

  // Moves focus and active state to the secondary devtools window, which exists
  // only in devtools JS tests.
  void FocusDevtoolsSecondaryWindow();

  // Pass the overridden WebPreferences to the browser.
  void OverridePreferences(const WebPreferences& prefs);

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
  void OnReplyBluetoothManualChooserEvents(
      const std::vector<std::string>& events);

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

  void HandleWebTestControlHostDisconnected();
  mojo::AssociatedRemote<mojom::WebTestControlHost>&
  GetWebTestControlHostRemote();
  mojo::AssociatedRemote<mojom::WebTestControlHost>
      web_test_control_host_remote_;

  void HandleWebTestClientDisconnected();
  mojo::AssociatedRemote<mojom::WebTestClient>& GetWebTestClientRemote();
  mojo::AssociatedRemote<mojom::WebTestClient> web_test_client_remote_;

  WebViewTestProxy* const web_view_test_proxy_;

  mojom::WebTestRunTestConfigurationPtr test_config_;

  base::circular_deque<
      base::OnceCallback<void(const std::vector<std::string>&)>>
      get_bluetooth_events_callbacks_;

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
