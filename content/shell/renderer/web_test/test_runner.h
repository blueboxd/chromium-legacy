// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_TEST_RUNNER_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_TEST_RUNNER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "content/shell/renderer/web_test/mock_screen_orientation_client.h"
#include "content/shell/renderer/web_test/web_test_runtime_flags.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "v8/include/v8.h"

class GURL;
class SkBitmap;

namespace base {
class DictionaryValue;
}

namespace blink {
class WebContentSettingsClient;
class WebFrame;
class WebLocalFrame;
class WebString;
class WebTextCheckClient;
class WebView;
}  // namespace blink

namespace content {
class RenderView;
struct TestPreferences;
}

namespace gin {
class ArrayBufferView;
class Arguments;
}  // namespace gin

namespace content {
class BlinkTestRunner;
class MockContentSettingsClient;
class MockScreenOrientationClient;
class RenderFrame;
class SpellCheckClient;
class TestInterfaces;
class TestRunnerForSpecificView;

// TestRunner class currently has dual purpose:
// 1. It implements TestRunner javascript bindings for "global" / "ambient".
//    Examples:
//    - TestRunner.DumpAsText (test flag affecting test behavior)
//    - TestRunner.SetAllowRunningOfInsecureContent (test flag affecting product
//      behavior)
//    - TestRunner.SetTextSubpixelPositioning (directly interacts with product).
//    Note that "per-view" (non-"global") bindings are handled by
//    instances of TestRunnerForSpecificView class.
// 2. It manages global test state.  Example:
//    - Tracking topLoadingFrame that can finish the test when it loads.
//    - WorkQueue holding load requests from the TestInterfaces
//    - WebTestRuntimeFlags
class TestRunner {
 public:
  explicit TestRunner(TestInterfaces*);
  virtual ~TestRunner();

  void Install(RenderFrame* frame,
               SpellCheckClient* spell_check,
               TestRunnerForSpecificView* view_test_runner);

  void SetDelegate(BlinkTestRunner*);
  void SetMainView(blink::WebView*);

  void Reset();

  void SetTestIsRunning(bool);
  bool TestIsRunning() const { return test_is_running_; }

  // Finishes the test if it is ready. This should be called before running
  // tasks that will change state, so that the test can capture the current
  // state. Specifically, should run before the BeginMainFrame step which does
  // layout and animation etc.
  // This does *not* run as part of loading finishing because that happens in
  // the middle of blink call stacks that have inconsistent state.
  void FinishTestIfReady();

  // Returns a mock WebContentSettings that is used for web tests. An
  // embedder should use this for all WebViews it creates.
  blink::WebContentSettingsClient* GetWebContentSettings() const;

  // Returns a mock WebTextCheckClient that is used for web tests. An
  // embedder should use this for all WebLocalFrames it creates.
  blink::WebTextCheckClient* GetWebTextCheckClient() const;

  // After BlinkTestRunner::TestFinished was invoked, the following methods
  // can be used to determine what kind of dump the main WebViewTestProxy can
  // provide.

  // Returns true if the test output should be an audio file, rather than text
  // or pixel results.
  bool ShouldDumpAsAudio() const;
  // Gets the audio test output for when audio test results are requested by
  // the current test.
  void GetAudioData(std::vector<unsigned char>* buffer_view) const;

  // Reports if tests requested a recursive layout dump of all frames
  // (i.e. by calling testRunner.dumpChildFramesAsText() from javascript).
  bool IsRecursiveLayoutDumpRequested();

  // Dumps layout of |frame| using the mode requested by the current test
  // (i.e. text mode if testRunner.dumpAsText() was called from javascript).
  std::string DumpLayout(blink::WebLocalFrame* frame);

  // Returns true if the selection window should be painted onto captured
  // pixels.
  bool ShouldDumpSelectionRect() const;

  // Returns false if the browser should capture the pixel output, true if it
  // can be done locally in the renderer via DumpPixelsAsync().
  bool CanDumpPixelsFromRenderer() const;

  // Snapshots the content of |render_view| using the mode requested by the
  // current test and calls |callback| with the result.  Caller needs to ensure
  // that |render_view| stays alive until |callback| is called.
  void DumpPixelsAsync(content::RenderView* render_view,
                       base::OnceCallback<void(const SkBitmap&)> callback);

  // Replicates changes to web test runtime flags (i.e. changes that happened in
  // another renderer). See also BlinkTestRunner::OnWebTestRuntimeFlagsChanged.
  void ReplicateWebTestRuntimeFlagsChanges(
      const base::DictionaryValue& changed_values);

  // If custom text dump is present (i.e. if testRunner.setCustomTextOutput has
  // been called from javascript), then returns |true| and populates the
  // |custom_text_dump| argument.  Otherwise returns |false|.
  bool HasCustomTextDump(std::string* custom_text_dump) const;

  // Returns true if the history should be included in text results generated at
  // the end of the test.
  bool ShouldDumpBackForwardList() const;

  // Returns true if pixel results should be generated at the end of the test.
  bool ShouldGeneratePixelResults();

  // Activate the window holding the given main frame, and set focus on the
  // frame's widget.
  void FocusWindow(RenderFrame* main_frame, bool focus);

  // Methods used by WebViewTestClient and WebFrameTestClient.
  std::string GetAcceptLanguages() const;
  bool ShouldStayOnPageAfterHandlingBeforeUnload() const;
  MockScreenOrientationClient* GetMockScreenOrientationClient();
  bool ShouldDumpAsCustomText() const;
  std::string CustomDumpText() const;
  void ShowDevTools(const std::string& settings,
                    const std::string& frontend_url);
  void SetV8CacheDisabled(bool);
  void SetShouldDumpAsText(bool);
  void SetShouldDumpAsMarkup(bool);
  void SetShouldDumpAsLayout(bool);
  void SetCustomTextOutput(const std::string& text);
  void SetShouldGeneratePixelResults(bool);
  void SetShouldDumpFrameLoadCallbacks(bool);
  void SetShouldEnableViewSource(bool);
  bool ShouldDumpEditingCallbacks() const;
  bool ShouldDumpFrameLoadCallbacks() const;
  bool ShouldDumpPingLoaderCallbacks() const;
  bool ShouldDumpUserGestureInFrameLoadCallbacks() const;
  bool ShouldDumpTitleChanges() const;
  bool ShouldDumpIconChanges() const;
  bool ShouldDumpCreateView() const;
  bool CanOpenWindows() const;
  bool ShouldWaitUntilExternalURLLoad() const;
  const std::set<std::string>* HttpHeadersToClear() const;
  bool ClearReferrer() const;
  bool IsWebPlatformTestsMode() const;
  void SetIsWebPlatformTestsMode();
  bool animation_requires_raster() const { return animation_requires_raster_; }
  void SetAnimationRequiresRaster(bool do_raster);

  // Add |frame| to the set of loading frames.
  //
  // Note: Only one renderer process is really tracking the loading frames. This
  //       is the first to observe one. Both local and remote frames are tracked
  //       by this process.
  void AddLoadingFrame(blink::WebFrame* frame);

  // Remove |frame| from the set of loading frames.
  //
  // When there are no more loading frames, this potentially finishes the test,
  // unless TestRunner.WaitUntilDone() was called and/or there are pending load
  // requests in WorkQueue.
  void RemoveLoadingFrame(blink::WebFrame* frame);

  blink::WebFrame* MainFrame() const;
  void PolicyDelegateDone();
  bool PolicyDelegateEnabled() const;
  bool PolicyDelegateIsPermissive() const;
  bool PolicyDelegateShouldNotifyDone() const;
  void SetToolTipText(const blink::WebString&);
  void SetDragImage(const SkBitmap& drag_image);
  bool ShouldDumpNavigationPolicy() const;

  bool ShouldDumpConsoleMessages() const;
  // Controls whether console messages produced by the page are dumped
  // to test output.
  void SetDumpConsoleMessages(bool value);

  bool ShouldDumpJavaScriptDialogs() const;

  blink::WebEffectiveConnectionType effective_connection_type() const {
    return effective_connection_type_;
  }

  // A single item in the work queue.
  class WorkItem {
   public:
    virtual ~WorkItem() {}

    // Returns true if this started a load.
    virtual bool Run(BlinkTestRunner*, blink::WebView*) = 0;
  };

 private:
  friend class TestRunnerBindings;
  friend class WorkQueue;

  // Helper class for managing events queued by methods like QueueLoad or
  // QueueScript.
  class WorkQueue {
   public:
    explicit WorkQueue(TestRunner* controller);
    virtual ~WorkQueue();
    void ProcessWorkSoon();

    // Reset the state of the class between tests.
    void Reset();

    void AddWork(WorkItem*);

    void set_frozen(bool frozen) { frozen_ = frozen; }
    bool is_empty() const { return queue_.empty(); }

    void set_finished_loading() { finished_loading_ = true; }

   private:
    void ProcessWork();

    base::circular_deque<WorkItem*> queue_;
    bool frozen_ = false;
    bool finished_loading_ = false;
    TestRunner* controller_;

    base::WeakPtrFactory<WorkQueue> weak_factory_{this};
  };

  ///////////////////////////////////////////////////////////////////////////
  // Methods dealing with the test logic

  // By default, tests end when page load is complete. These methods are used
  // to delay the completion of the test until NotifyDone is called.
  void NotifyDone();
  void WaitUntilDone();

  // Methods for adding actions to the work queue. Used in conjunction with
  // WaitUntilDone/NotifyDone above.
  void QueueBackNavigation(int how_far_back);
  void QueueForwardNavigation(int how_far_forward);
  void QueueReload();
  void QueueLoadingScript(const std::string& script);
  void QueueNonLoadingScript(const std::string& script);
  void QueueLoad(const std::string& url, const std::string& target);

  // Called from the TestRunnerBindings to inform that the test has modified
  // the TestPreferences. This will update the WebkitPreferences in the renderer
  // and the browser.
  void OnTestPreferencesChanged(const TestPreferences& test_prefs,
                                RenderFrame* frame);

  // Causes navigation actions just printout the intended navigation instead
  // of taking you to the page. This is used for cases like mailto, where you
  // don't actually want to open the mail program.
  void SetCustomPolicyDelegate(gin::Arguments* args);

  // Delays completion of the test until the policy delegate runs.
  void WaitForPolicyDelegate();

  // Functions for dealing with windows. By default we block all new windows.
  int WindowCount();
  void SetCloseRemainingWindowsWhenComplete(bool close_remaining_windows);

  // Allows web tests to manage origins' allow list.
  void AddOriginAccessAllowListEntry(const std::string& source_origin,
                                     const std::string& destination_protocol,
                                     const std::string& destination_host,
                                     bool allow_destination_subdomains);

  // Add |source_code| as an injected stylesheet to the active document of the
  // window of the current V8 context.
  void InsertStyleSheet(const std::string& source_code);

  // Enables or disables subpixel positioning (i.e. fractional X positions for
  // glyphs) in text rendering on Linux. Since this method changes global
  // settings, tests that call it must use their own custom font family for
  // all text that they render. If not, an already-cached style will be used,
  // resulting in the changed setting being ignored.
  void SetTextSubpixelPositioning(bool value);

  // After this function is called, all window-sizing machinery is
  // short-circuited inside the renderer. This mode is necessary for
  // some tests that were written before browsers had multi-process architecture
  // and rely on window resizes to happen synchronously.
  // The function has "unfortunate" it its name because we must strive to remove
  // all tests that rely on this... well, unfortunate behavior. See
  // http://crbug.com/309760 for the plan.
  void UseUnfortunateSynchronousResizeMode();

  void SetMockScreenOrientation(const std::string& orientation);
  void DisableMockScreenOrientation();

  void SetPopupBlockingEnabled(bool block_popups);

  // Modify accept_languages in blink::mojom::RendererPreferences.
  void SetAcceptLanguages(const std::string& accept_languages);

  ///////////////////////////////////////////////////////////////////////////
  // Methods that modify the state of TestRunner

  // This function sets a flag that tells the test runner to print a line of
  // descriptive text for each editing command. It takes no arguments, and
  // ignores any that may be present.
  void DumpEditingCallbacks();

  // This function sets a flag that tells the test runner to dump pages as
  // plain text. The pixel results will not be generated for this test.
  // It has higher priority than DumpAsMarkup() and DumpAsLayout().
  void DumpAsText();

  // This function sets a flag that tells the test runner to dump pages as
  // the DOM contents, rather than as a text representation of the renderer's
  // state. The pixel results will not be generated for this test. It has
  // higher priority than DumpAsLayout(), but lower than DumpAsText().
  void DumpAsMarkup();

  // This function sets a flag that tells the test runner to dump pages as
  // plain text. It will also generate a pixel dump for the test.
  void DumpAsTextWithPixelResults();

  // This function sets a flag that tells the test runner to dump pages as
  // text representation of the layout. The pixel results will not be generated
  // for this test. It has lower priority than DumpAsText() and DumpAsMarkup().
  void DumpAsLayout();

  // This function sets a flag that tells the test runner to dump pages as
  // text representation of the layout. It will also generate a pixel dump for
  // the test.
  void DumpAsLayoutWithPixelResults();

  // This function sets a flag that tells the test runner to recursively dump
  // all frames as text, markup or layout depending on which of DumpAsText,
  // DumpAsMarkup and DumpAsLayout is effective.
  void DumpChildFrames();

  // This function sets a flag that tells the test runner to print out the
  // information about icon changes notifications from WebKit.
  void DumpIconChanges();

  // Deals with Web Audio WAV file data.
  void SetAudioData(const gin::ArrayBufferView& view);

  // This function sets a flag that tells the test runner to print a line of
  // descriptive text for each frame load callback. It takes no arguments, and
  // ignores any that may be present.
  void DumpFrameLoadCallbacks();

  // This function sets a flag that tells the test runner to print a line of
  // descriptive text for each PingLoader dispatch. It takes no arguments, and
  // ignores any that may be present.
  void DumpPingLoaderCallbacks();

  // This function sets a flag that tells the test runner to print a line of
  // user gesture status text for some frame load callbacks. It takes no
  // arguments, and ignores any that may be present.
  void DumpUserGestureInFrameLoadCallbacks();

  void DumpTitleChanges();

  // This function sets a flag that tells the test runner to dump all calls to
  // WebViewClient::createView().
  // It takes no arguments, and ignores any that may be present.
  void DumpCreateView();

  void SetCanOpenWindows();

  // This function sets a flag that tells the test runner to dump the MIME type
  // for each resource that was loaded. It takes no arguments, and ignores any
  // that may be present.
  void DumpResourceResponseMIMETypes();

  // WebContentSettingsClient related.
  void SetImagesAllowed(bool allowed);
  void SetScriptsAllowed(bool allowed);
  void SetStorageAllowed(bool allowed);
  void SetPluginsAllowed(bool allowed);
  void SetAllowRunningOfInsecureContent(bool allowed);
  void DumpPermissionClientCallbacks();

  // Sets up a mock DocumentSubresourceFilter to disallow subsequent subresource
  // loads within the current document with the given path |suffixes|. The
  // filter is created and injected even if |suffixes| is empty. If |suffixes|
  // contains the empty string, all subresource loads will be disallowed. If
  // |block_subresources| is false, matching resources will not be blocked but
  // instead marked as matching a disallowed resource.
  void SetDisallowedSubresourcePathSuffixes(
      const std::vector<std::string>& suffixes,
      bool block_subresources);

  // This function sets a flag that tells the test runner to print out a text
  // representation of the back/forward list. It ignores all arguments.
  void DumpBackForwardList();

  void DumpSelectionRect();

  // Causes layout to happen as if targetted to printed pages.
  void SetPrinting();
  void SetPrintingForFrame(const std::string& frame_name);

  // Clears the state from SetPrinting().
  void ClearPrinting();

  void SetShouldStayOnPageAfterHandlingBeforeUnload(bool value);

  // Causes WillSendRequest to clear certain headers.
  // Note: This cannot be used to clear the request's `Referer` header, as this
  // header is computed later given its referrer string member. To clear it, use
  // SetWillSendRequestClearReferrer() below.
  void SetWillSendRequestClearHeader(const std::string& header);

  // Causes WillSendRequest to clear the request's referrer string and set its
  // referrer policy to the default.
  void SetWillSendRequestClearReferrer();

  // Sets a flag that causes the test to be marked as completed when the
  // WebLocalFrameClient receives a LoadURLExternally() call.
  void WaitUntilExternalURLLoad();

  // This function sets a flag to dump the drag image when the next drag&drop is
  // initiated. It is equivalent to DumpAsTextWithPixelResults but the pixel
  // results will be the drag image instead of a snapshot of the page.
  void DumpDragImage();

  // Sets a flag that tells the WebViewTestProxy to dump the default navigation
  // policy passed to the DecidePolicyForNavigation callback.
  void DumpNavigationPolicy();

  // Controls whether JavaScript dialogs such as alert() are dumped to test
  // output.
  void SetDumpJavaScriptDialogs(bool value);

  // Overrides the NetworkQualityEstimator's estimated network type. If |type|
  // is TypeUnknown the NQE's value is used. Be sure to call this with
  // TypeUnknown at the end of your test if you use this.
  void SetEffectiveConnectionType(
      blink::WebEffectiveConnectionType connection_type);

  ///////////////////////////////////////////////////////////////////////////
  // Methods forwarding to the BlinkTestRunner.

  // Shows DevTools window.
  void ShowWebInspector(const std::string& str,
                        const std::string& frontend_url);
  void CloseWebInspector();

  // Inspect chooser state
  bool IsChooserShown();

  // Allows web tests to exec scripts at WebInspector side.
  void EvaluateInWebInspector(int call_id, const std::string& script);

  // Clears all databases.
  void ClearAllDatabases();
  // Sets the default quota for all origins
  void SetDatabaseQuota(int quota);

  // Sets the cookie policy to:
  // - allow all cookies when |block| is false
  // - block only third-party cookies when |block| is true
  void SetBlockThirdPartyCookies(bool block);

  // Sets the permission's |name| to |value| for a given {origin, embedder}
  // tuple.
  void SetPermission(const std::string& name,
                     const std::string& value,
                     const GURL& origin,
                     const GURL& embedding_origin);

  // Calls setlocale(LC_ALL, ...) for a specified locale.
  // Resets between tests.
  void SetPOSIXLocale(const std::string& locale);

  // Simulates a click on a Web Notification.
  void SimulateWebNotificationClick(
      const std::string& title,
      const base::Optional<int>& action_index,
      const base::Optional<base::string16>& reply);

  // Simulates closing a Web Notification.
  void SimulateWebNotificationClose(const std::string& title, bool by_user);

  // Simulates a user deleting a content index entry.
  void SimulateWebContentIndexDelete(const std::string& id);

  // Returns the absolute path to a directory this test can write data in. This
  // returns the path to a fresh empty directory every time this method is
  // called. Additionally when this method is called any previously created
  // directories will be deleted.
  base::FilePath GetWritableDirectory();

  // Sets the path that should be returned when the test shows a file dialog.
  void SetFilePathForMockFileDialog(const base::FilePath& path);

  // Takes care of notifying the delegate after a change to web test runtime
  // flags.
  void OnWebTestRuntimeFlagsChanged();

  ///////////////////////////////////////////////////////////////////////////
  // Internal helpers

  bool IsFramePartOfMainTestWindow(blink::WebFrame*) const;

  void CheckResponseMimeType();

  bool test_is_running_ = false;

  // When reset is called, go through and close all but the main test shell
  // window. By default, set to true but toggled to false using
  // SetCloseRemainingWindowsWhenComplete().
  bool close_remaining_windows_ = false;

  WorkQueue work_queue_;

  // Bound variable to return the name of this platform (chromium).
  std::string platform_name_;

  // Bound variable to store the last tooltip text
  std::string tooltip_text_;

  // Bound variable counting the number of top URLs visited.
  int web_history_item_count_ = 0;

  // Flags controlling what content gets dumped as a layout text result.
  WebTestRuntimeFlags web_test_runtime_flags_;

  // If true, the test runner will output a base64 encoded WAVE file.
  bool dump_as_audio_;

  // If true, the test runner will produce a dump of the back forward list as
  // well.
  bool dump_back_forward_list_;

  // If true, pixel dump will be produced as a series of 1px-tall, view-wide
  // individual paints over the height of the view.
  bool test_repaint_;

  // If true and test_repaint_ is true as well, pixel dump will be produced as
  // a series of 1px-wide, view-tall paints across the width of the view.
  bool sweep_horizontally_;

  std::set<std::string> http_headers_to_clear_;
  bool clear_referrer_ = false;

  // WAV audio data is stored here.
  std::vector<unsigned char> audio_data_;

  TestInterfaces* test_interfaces_;
  BlinkTestRunner* blink_test_runner_ = nullptr;
  blink::WebView* main_view_ = nullptr;

  // This is non empty when a load is in progress.
  std::vector<blink::WebFrame*> loading_frames_;
  // When a loading task is started, this bool is set until all loading_frames_
  // are completed and removed. This bool becomes true earlier than
  // loading_frames_ becomes non-empty. Starts as true for the initial load
  // which does not come from the WorkQueue.
  bool running_load_ = true;
  // When NotifyDone() occurs, if loading is still working, it is delayed, and
  // this bool tracks that NotifyDone() was called. This differentiates from a
  // test that was not waiting for NotifyDone() at all.
  bool did_notify_done_ = false;

  // WebContentSettingsClient mock object.
  std::unique_ptr<MockContentSettingsClient> mock_content_settings_client_;

  bool use_mock_theme_ = false;

  MockScreenOrientationClient mock_screen_orientation_client_;

  // Number of currently active color choosers.
  int chooser_count_ = 0;

  // Captured drag image.
  SkBitmap drag_image_;

  // True if rasterization should be performed during tests that examine
  // fling-style animations. This includes middle-click auto-scroll behaviors.
  // This does not include most "ordinary" animations, such as CSS animations.
  bool animation_requires_raster_ = false;

  // An effective connection type settable by web tests.
  blink::WebEffectiveConnectionType effective_connection_type_ =
      blink::WebEffectiveConnectionType::kTypeUnknown;

  // Forces v8 compilation cache to be disabled (used for inspector tests).
  bool disable_v8_cache_ = false;

  base::WeakPtrFactory<TestRunner> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestRunner);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_TEST_RUNNER_H_
