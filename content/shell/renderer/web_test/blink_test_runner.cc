// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/blink_test_runner.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/hash/md5.h"
#include "content/public/common/url_constants.h"
#include "content/shell/renderer/web_test/test_runner.h"
#include "content/shell/renderer/web_test/web_frame_test_proxy.h"
#include "content/shell/renderer/web_test/web_view_test_proxy.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

BlinkTestRunner::BlinkTestRunner(WebViewTestProxy* web_view_test_proxy)
    : web_view_test_proxy_(web_view_test_proxy),
      test_config_(mojom::WebTestRunTestConfiguration::New()) {}

BlinkTestRunner::~BlinkTestRunner() = default;

blink::WebString BlinkTestRunner::GetAbsoluteWebStringFromUTF8Path(
    const std::string& utf8_path) {
  base::FilePath path = base::FilePath::FromUTF8Unsafe(utf8_path);
  if (!path.IsAbsolute()) {
    GURL base_url =
        net::FilePathToFileURL(test_config_->current_working_directory.Append(
            FILE_PATH_LITERAL("foo")));
    net::FileURLToFilePath(base_url.Resolve(utf8_path), &path);
  }
  return blink::FilePathToWebString(path);
}

void BlinkTestRunner::TestFinished() {
  TestRunner* test_runner = web_view_test_proxy_->GetTestRunner();

  DCHECK(is_main_window_);
  DCHECK(web_view_test_proxy_->GetMainRenderFrame());

  // Avoid a situation where TestFinished is called twice, because
  // of a racey test where renderers both call notifyDone(), or a test that
  // calls notifyDone() more than once.
  if (!test_runner->TestIsRunning())
    return;
  test_runner->SetTestIsRunning(false);

  // Now we know that we're in the main frame, we should generate dump results.
  // Clean out the lifecycle if needed before capturing the web tree
  // dump and pixels from the compositor.
  auto* web_frame = web_view_test_proxy_->GetMainRenderFrame()->GetWebFrame();
  web_frame->FrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);

  // Initialize a new dump results object which we will populate in the calls
  // below.
  auto dump_result = mojom::WebTestRendererDumpResult::New();

  bool browser_should_dump_back_forward_list =
      test_runner->ShouldDumpBackForwardList();
  bool browser_should_dump_pixels = false;

  if (test_runner->ShouldDumpAsAudio()) {
    dump_result->audio = CaptureLocalAudioDump();
  } else {
    TextResultType text_result_type = test_runner->ShouldGenerateTextResults();
    bool pixel_result = test_runner->ShouldGeneratePixelResults();

    std::string spec = GURL(test_config_->test_url).spec();
    size_t path_start = spec.rfind("web_tests/");
    if (path_start != std::string::npos)
      spec = spec.substr(path_start);

    std::string mime_type =
        web_frame->GetDocumentLoader()->GetResponse().MimeType().Utf8();

    // In a text/plain document, and in a dumpAsText/ subdirectory, we generate
    // text results no matter what the test may previously have requested.
    if (mime_type == "text/plain" ||
        spec.find("/dumpAsText/") != std::string::npos) {
      text_result_type = TextResultType::kText;
      pixel_result = false;
    }

    // If possible we grab the layout dump locally because a round trip through
    // the browser would give javascript a chance to run and change the layout.
    // We only go to the browser if we can not do it locally, because we want to
    // dump more than just the local main frame. Those tests must be written to
    // not modify layout after signalling the test is finished.
    dump_result->layout = CaptureLocalLayoutDump(text_result_type);

    if (pixel_result) {
      if (test_runner->CanDumpPixelsFromRenderer()) {
        SkBitmap actual = CaptureLocalPixelsDump();

        base::MD5Digest digest;
        base::MD5Sum(actual.getPixels(), actual.computeByteSize(), &digest);
        dump_result->actual_pixel_hash = base::MD5DigestToBase16(digest);

        if (dump_result->actual_pixel_hash != test_config_->expected_pixel_hash)
          dump_result->pixels = std::move(actual);
      } else {
        browser_should_dump_pixels = true;
        dump_result->selection_rect = CaptureLocalMainFrameSelectionRect();
      }
    }
  }

  // Informs the browser that the test is done, passing along any test results
  // that have been generated locally. The browser may collect further results
  // from this and other renderer processes before moving on to the next test.
  GetWebTestControlHostRemote()->InitiateCaptureDump(
      std::move(dump_result), browser_should_dump_back_forward_list,
      browser_should_dump_pixels);
}

std::vector<uint8_t> BlinkTestRunner::CaptureLocalAudioDump() {
  TRACE_EVENT0("shell", "BlinkTestRunner::CaptureLocalAudioDump");
  TestRunner* test_runner = web_view_test_proxy_->GetTestRunner();
  return test_runner->GetAudioData();
}

base::Optional<std::string> BlinkTestRunner::CaptureLocalLayoutDump(
    TextResultType type) {
  TRACE_EVENT0("shell", "BlinkTestRunner::CaptureLocalLayoutDump");
  TestRunner* test_runner = web_view_test_proxy_->GetTestRunner();

  {
    // The CustomTextDump always takes precedence, and is also only available
    // for a local dump of the main frame.
    std::string layout;
    if (test_runner->HasCustomTextDump(&layout))
      return layout + "\n";
  }

  // If doing a recursive dump, it's done asynchronously from the browser.
  if (test_runner->IsRecursiveLayoutDumpRequested()) {
    return base::nullopt;
  }

  // Otherwise, in the common case, we do a synchronous text dump of the main
  // frame here.
  RenderFrame* main_frame = web_view_test_proxy_->GetMainRenderFrame();
  return DumpLayoutAsString(main_frame->GetWebFrame(), type);
}

SkBitmap BlinkTestRunner::CaptureLocalPixelsDump() {
  TRACE_EVENT0("shell", "BlinkTestRunner::CaptureLocalPixelsDump");
  TestRunner* test_runner = web_view_test_proxy_->GetTestRunner();
  SkBitmap snapshot = test_runner->DumpPixelsInRenderer(web_view_test_proxy_);
  DCHECK_GT(snapshot.info().width(), 0);
  DCHECK_GT(snapshot.info().height(), 0);
  return snapshot;
}

gfx::Rect BlinkTestRunner::CaptureLocalMainFrameSelectionRect() {
  TRACE_EVENT0("shell", "BlinkTestRunner::CaptureLocalSelectionRect");
  TestRunner* test_runner = web_view_test_proxy_->GetTestRunner();
  if (!test_runner->ShouldDumpSelectionRect())
    return gfx::Rect();
  auto* web_frame = web_view_test_proxy_->GetMainRenderFrame()->GetWebFrame();
  return web_frame->GetSelectionBoundsRectForTesting();
}

int BlinkTestRunner::NavigationEntryCount() {
  return web_view_test_proxy_->GetLocalSessionHistoryLengthForTesting();
}

bool BlinkTestRunner::AllowExternalPages() {
  return test_config_->allow_external_pages;
}

void BlinkTestRunner::DidCommitNavigationInMainFrame() {
  // This method is just meant to catch the about:blank navigation started in
  // ResetRendererAfterWebTest().
  if (!waiting_for_reset_navigation_to_about_blank_)
    return;

  TestRunner* test_runner = web_view_test_proxy_->GetTestRunner();
  RenderFrameImpl* main_frame = web_view_test_proxy_->GetMainRenderFrame();
  DCHECK(main_frame);

  // This would mean some other navigation was already happening when the test
  // ended, the about:blank should still be coming.
  GURL url = main_frame->GetWebFrame()->GetDocumentLoader()->GetUrl();
  if (!url.IsAboutBlank())
    return;

  waiting_for_reset_navigation_to_about_blank_ = false;

  static_cast<WebFrameTestProxy*>(main_frame)->Reset();
  test_runner->Reset();

  // Ack to the browser (this could converted to be a mojo reply).
  GetWebTestControlHostRemote()->ResetRendererAfterWebTestDone();
}

mojo::AssociatedRemote<mojom::WebTestControlHost>&
BlinkTestRunner::GetWebTestControlHostRemote() {
  TestRunner* test_runner = web_view_test_proxy_->GetTestRunner();
  return test_runner->GetWebTestControlHostRemote();
}

mojo::AssociatedRemote<mojom::WebTestClient>&
BlinkTestRunner::GetWebTestClientRemote() {
  TestRunner* test_runner = web_view_test_proxy_->GetTestRunner();
  return test_runner->GetWebTestClientRemote();
}

void BlinkTestRunner::OnSetupRendererProcessForNonTestWindow() {
  DCHECK(!is_main_window_);

  TestRunner* test_runner = web_view_test_proxy_->GetTestRunner();
  // Allows the window to receive replicated WebTestRuntimeFlags and to
  // control or end the test.
  test_runner->SetTestIsRunning(true);
}

void BlinkTestRunner::ApplyTestConfiguration(
    mojom::WebTestRunTestConfigurationPtr params) {
  TestRunner* test_runner = web_view_test_proxy_->GetTestRunner();

  test_config_ = params.Clone();

  is_main_window_ = true;
  test_runner->SetTestIsRunning(true);

  std::string spec = GURL(test_config_->test_url).spec();
  size_t path_start = spec.rfind("web_tests/");
  if (path_start != std::string::npos)
    spec = spec.substr(path_start);

  bool is_devtools_test =
      spec.find("/devtools/") != std::string::npos ||
      spec.find("/inspector-protocol/") != std::string::npos;
  if (is_devtools_test)
    test_runner->SetDumpConsoleMessages(false);

  // In protocol mode (see TestInfo::protocol_mode), we dump layout only when
  // requested by the test. In non-protocol mode, we dump layout by default
  // because the layout may be the only interesting thing to the user while
  // we don't dump non-human-readable binary data. In non-protocol mode, we
  // still generate pixel results (though don't dump them) to let the renderer
  // execute the same code regardless of the protocol mode, e.g. for ease of
  // debugging a web test issue.
  if (!params->protocol_mode)
    test_runner->SetShouldDumpAsLayout(true);

  // For http/tests/loading/, which is served via httpd and becomes /loading/.
  if (spec.find("/loading/") != std::string::npos)
    test_runner->SetShouldDumpFrameLoadCallbacks(true);

  if (spec.find("/external/wpt/") != std::string::npos ||
      spec.find("/external/csswg-test/") != std::string::npos ||
      spec.find("://web-platform.test") != std::string::npos ||
      spec.find("/harness-tests/wpt/") != std::string::npos)
    test_runner->SetIsWebPlatformTestsMode();

  web_view_test_proxy_->GetWebView()->GetSettings()->SetV8CacheOptions(
      is_devtools_test ? blink::WebSettings::V8CacheOptions::kNone
                       : blink::WebSettings::V8CacheOptions::kDefault);
}

void BlinkTestRunner::OnReplicateTestConfiguration(
    mojom::WebTestRunTestConfigurationPtr params) {
  ApplyTestConfiguration(std::move(params));
}

void BlinkTestRunner::OnSetTestConfiguration(
    mojom::WebTestRunTestConfigurationPtr params) {
  DCHECK(web_view_test_proxy_->GetMainRenderFrame());

  ApplyTestConfiguration(std::move(params));

  // If focus was in a child frame, it gets lost when we navigate to the next
  // test, but we want to start with focus in the main frame for every test.
  // Focus is controlled by the renderer, so we must do the reset here.
  web_view_test_proxy_->GetWebView()->SetFocusedFrame(
      web_view_test_proxy_->GetMainRenderFrame()->GetWebFrame());
}

void BlinkTestRunner::OnResetRendererAfterWebTest() {
  // BlinkTestMsg_Reset should always be sent to the *current* view.
  DCHECK(web_view_test_proxy_->GetMainRenderFrame());

  // Instead of resetting for the next test here, delay until after the
  // navigation to about:blank (this is in |DidCommitNavigationInMainFrame()|).
  // This ensures we reset settings that are set between now and the load of
  // about:blank.

  // Navigating to about:blank will make sure that no new loads are initiated
  // by the renderer.
  waiting_for_reset_navigation_to_about_blank_ = true;

  blink::WebURLRequest request{GURL(url::kAboutBlankURL)};
  request.SetMode(network::mojom::RequestMode::kNavigate);
  request.SetRedirectMode(network::mojom::RedirectMode::kManual);
  request.SetRequestContext(blink::mojom::RequestContextType::INTERNAL);
  request.SetRequestorOrigin(blink::WebSecurityOrigin::CreateUniqueOpaque());
  web_view_test_proxy_->GetMainRenderFrame()->GetWebFrame()->StartNavigation(
      request);
}

void BlinkTestRunner::OnFinishTestInMainWindow() {
  TestFinished();
}

}  // namespace content
