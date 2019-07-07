// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_CONTENTS_BROWSERTEST_H_
#define CHROMECAST_BROWSER_CAST_WEB_CONTENTS_BROWSERTEST_H_

#include <algorithm>
#include <memory>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Property;

namespace content {
class WebContents;
}

namespace chromecast {

namespace {

const base::FilePath::CharType kTestDataPath[] =
    FILE_PATH_LITERAL("chromecast/browser/test/data");

base::FilePath GetTestDataPath() {
  return base::FilePath(kTestDataPath);
}

base::FilePath GetTestDataFilePath(const std::string& name) {
  base::FilePath file_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path));
  return file_path.Append(GetTestDataPath()).AppendASCII(name);
}

std::unique_ptr<net::test_server::HttpResponse> DefaultHandler(
    net::HttpStatusCode status_code,
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(status_code);
  return http_response;
}

// =============================================================================
// Mocks
// =============================================================================
class MockCastWebContentsDelegate : public CastWebContents::Delegate {
 public:
  MockCastWebContentsDelegate() {}
  ~MockCastWebContentsDelegate() override = default;

  MOCK_METHOD2(InnerContentsCreated,
               void(CastWebContents* inner_contents,
                    CastWebContents* outer_contents));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCastWebContentsDelegate);
};

class MockCastWebContentsObserver : public CastWebContents::Observer {
 public:
  MockCastWebContentsObserver() {}
  ~MockCastWebContentsObserver() override = default;

  MOCK_METHOD1(OnPageStateChanged, void(CastWebContents* cast_web_contents));
  MOCK_METHOD2(OnPageStopped,
               void(CastWebContents* cast_web_contents, int error_code));
  MOCK_METHOD4(
      RenderFrameCreated,
      void(int render_process_id,
           int render_frame_id,
           service_manager::InterfaceProvider* frame_interfaces,
           blink::AssociatedInterfaceProvider* frame_associated_interfaces));
  MOCK_METHOD1(ResourceLoadFailed, void(CastWebContents* cast_web_contents));
  MOCK_METHOD1(UpdateTitle, void(const base::string16& title));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCastWebContentsObserver);
};

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() = default;
  ~MockWebContentsDelegate() override = default;

  MOCK_METHOD1(CloseContents, void(content::WebContents* source));
};

class TitleChangeObserver : public CastWebContents::Observer {
 public:
  TitleChangeObserver() = default;
  ~TitleChangeObserver() override = default;

  // Spins a Runloop until the title of the page matches the |expected_title|
  // that have been set.
  void RunUntilTitleEquals(base::StringPiece expected_title) {
    expected_title_ = expected_title.as_string();
    // Spin the runloop until the expected conditions are met.
    if (current_title_ != expected_title_) {
      expected_title_ = expected_title.as_string();
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  // CastWebContents::Observer implementation:
  void UpdateTitle(const base::string16& title) override {
    // Resumes execution of RunUntilTitleEquals() if |title| matches
    // expectations.
    std::string title_utf8 = base::UTF16ToUTF8(title);
    current_title_ = title_utf8;
    if (!quit_closure_.is_null() && current_title_ == expected_title_) {
      DCHECK_EQ(current_title_, expected_title_);
      std::move(quit_closure_).Run();
    }
  }

 private:
  std::string current_title_;
  std::string expected_title_;

  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(TitleChangeObserver);
};

}  // namespace

// =============================================================================
// Test class
// =============================================================================
class CastWebContentsBrowserTest : public content::BrowserTestBase,
                                   public content::WebContentsObserver {
 protected:
  CastWebContentsBrowserTest() = default;
  ~CastWebContentsBrowserTest() override = default;

  void SetUp() final {
    SetUpCommandLine(base::CommandLine::ForCurrentProcess());
    BrowserTestBase::SetUp();
  }
  void SetUpCommandLine(base::CommandLine* command_line) final {
    command_line->AppendSwitch(switches::kNoWifi);
    command_line->AppendSwitchASCII(switches::kTestType, "browser");
  }
  void PreRunTestOnMainThread() override {
    // Pump startup related events.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    base::RunLoop().RunUntilIdle();

    metrics::CastMetricsHelper::GetInstance()->SetDummySessionIdForTesting();
    content::WebContents::CreateParams create_params(
        shell::CastBrowserProcess::GetInstance()->browser_context(), nullptr);
    web_contents_ = content::WebContents::Create(create_params);
    web_contents_->SetDelegate(&mock_wc_delegate_);

    CastWebContents::InitParams init_params = {
        &mock_cast_wc_delegate_, false /* enabled_for_dev */,
        false /* use_cma_renderer */, true /* is_root_window */};
    cast_web_contents_ =
        std::make_unique<CastWebContentsImpl>(web_contents_.get(), init_params);
    mock_cast_wc_observer_.Observe(cast_web_contents_.get());
    title_change_observer_.Observe(cast_web_contents_.get());

    render_frames_.clear();
    content::WebContentsObserver::Observe(web_contents_.get());
  }
  void PostRunTestOnMainThread() override {
    cast_web_contents_.reset();
    web_contents_.reset();
  }

  // content::WebContentsObserver implementation:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) final {
    render_frames_.insert(render_frame_host);
  }

  void StartTestServer() {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
  }

  MockWebContentsDelegate mock_wc_delegate_;
  MockCastWebContentsDelegate mock_cast_wc_delegate_;
  NiceMock<MockCastWebContentsObserver> mock_cast_wc_observer_;
  TitleChangeObserver title_change_observer_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<CastWebContentsImpl> cast_web_contents_;

  base::flat_set<content::RenderFrameHost*> render_frames_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastWebContentsBrowserTest);
};

MATCHER_P2(CheckPageState, cwc_ptr, expected_state, "") {
  if (arg != cwc_ptr)
    return false;
  return arg->page_state() == expected_state;
}

// =============================================================================
// Test cases
// =============================================================================
IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, Lifecycle) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: Load a blank page successfully, verify LOADED state.
  // ===========================================================================
  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));
  run_loop->Run();

  // ===========================================================================
  // Test: Load a blank page via WebContents API, verify LOADED state.
  // ===========================================================================
  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  run_loop = std::make_unique<base::RunLoop>();
  web_contents_->GetController().LoadURL(GURL(url::kAboutBlankURL),
                                         content::Referrer(),
                                         ui::PAGE_TRANSITION_TYPED, "");
  run_loop->Run();

  // ===========================================================================
  // Test: Inject an iframe, verify no events are received for the frame.
  // ===========================================================================
  EXPECT_CALL(mock_cast_wc_observer_, OnPageStateChanged(_)).Times(0);
  EXPECT_CALL(mock_cast_wc_observer_, OnPageStopped(_, _)).Times(0);
  std::string script =
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);"
      "iframe.src = 'about:blank';";
  ASSERT_TRUE(ExecJs(web_contents_.get(), script));

  // ===========================================================================
  // Test: Inject an iframe and navigate it to an error page. Verify no events.
  // ===========================================================================
  EXPECT_CALL(mock_cast_wc_observer_, OnPageStateChanged(_)).Times(0);
  EXPECT_CALL(mock_cast_wc_observer_, OnPageStopped(_, _)).Times(0);
  script = "iframe.src = 'https://www.fake-non-existent-cast-page.com';";
  ASSERT_TRUE(ExecJs(web_contents_.get(), script));

  // ===========================================================================
  // Test: Close the CastWebContents. WebContentsDelegate will be told to close
  // the page, and then after the timeout elapses CWC will enter the CLOSED
  // state and notify that the page has stopped.
  // ===========================================================================
  EXPECT_CALL(mock_wc_delegate_, CloseContents(web_contents_.get()))
      .Times(AtLeast(1));
  EXPECT_CALL(mock_cast_wc_observer_,
              OnPageStopped(CheckPageState(cast_web_contents_.get(),
                                           CastWebContents::PageState::CLOSED),
                            net::OK))
      .WillOnce(InvokeWithoutArgs(quit_closure));
  run_loop = std::make_unique<base::RunLoop>();
  cast_web_contents_->ClosePage();
  run_loop->Run();

  // ===========================================================================
  // Test: Destroy the underlying WebContents. Verify DESTROYED state.
  // ===========================================================================
  EXPECT_CALL(
      mock_cast_wc_observer_,
      OnPageStateChanged(CheckPageState(
          cast_web_contents_.get(), CastWebContents::PageState::DESTROYED)));
  web_contents_.reset();
  cast_web_contents_.reset();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, WebContentsDestroyed) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));
  run_loop->Run();

  // ===========================================================================
  // Test: Destroy the WebContents. Verify OnPageStopped(DESTROYED, net::OK).
  // ===========================================================================
  EXPECT_CALL(
      mock_cast_wc_observer_,
      OnPageStopped(CheckPageState(cast_web_contents_.get(),
                                   CastWebContents::PageState::DESTROYED),
                    net::OK));
  web_contents_.reset();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ErrorPageCrash) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: If the page's main render process crashes, enter ERROR state.
  // ===========================================================================
  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));
  run_loop->Run();

  EXPECT_CALL(mock_cast_wc_observer_,
              OnPageStopped(CheckPageState(cast_web_contents_.get(),
                                           CastWebContents::PageState::ERROR),
                            net::ERR_UNEXPECTED));
  CrashTab(web_contents_.get());
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ErrorLocalFileMissing) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: Loading a page with an HTTP error should enter ERROR state.
  // ===========================================================================
  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(mock_cast_wc_observer_,
                OnPageStopped(CheckPageState(cast_web_contents_.get(),
                                             CastWebContents::PageState::ERROR),
                              _))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  base::FilePath path = GetTestDataFilePath("this_file_does_not_exist.html");
  cast_web_contents_->LoadUrl(content::GetFileUrlWithQuery(path, ""));
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ErrorLoadFailSubFrames) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: Ignore load errors in sub-frames.
  // ===========================================================================
  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));
  run_loop->Run();

  // Create a sub-frame.
  EXPECT_CALL(mock_cast_wc_observer_, OnPageStateChanged(_)).Times(0);
  EXPECT_CALL(mock_cast_wc_observer_, OnPageStopped(_, _)).Times(0);
  std::string script =
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);"
      "iframe.src = 'about:blank';";
  ASSERT_TRUE(ExecJs(web_contents_.get(), script));

  ASSERT_EQ(2, (int)render_frames_.size());
  auto it =
      std::find_if(render_frames_.begin(), render_frames_.end(),
                   [this](content::RenderFrameHost* frame) {
                     return frame->GetParent() == web_contents_->GetMainFrame();
                   });
  ASSERT_NE(render_frames_.end(), it);
  content::RenderFrameHost* sub_frame = *it;
  ASSERT_NE(nullptr, sub_frame);
  cast_web_contents_->DidFailLoad(sub_frame, sub_frame->GetLastCommittedURL(),
                                  net::ERR_FAILED, base::string16());

  // ===========================================================================
  // Test: Ignore main frame load failures with net::ERR_ABORTED.
  // ===========================================================================
  EXPECT_CALL(mock_cast_wc_observer_, OnPageStateChanged(_)).Times(0);
  EXPECT_CALL(mock_cast_wc_observer_, OnPageStopped(_, _)).Times(0);
  cast_web_contents_->DidFailLoad(
      web_contents_->GetMainFrame(),
      web_contents_->GetMainFrame()->GetLastCommittedURL(), net::ERR_ABORTED,
      base::string16());

  // ===========================================================================
  // Test: If main frame fails to load, page should enter ERROR state.
  // ===========================================================================
  EXPECT_CALL(mock_cast_wc_observer_,
              OnPageStopped(CheckPageState(cast_web_contents_.get(),
                                           CastWebContents::PageState::ERROR),
                            net::ERR_FAILED));
  cast_web_contents_->DidFailLoad(
      web_contents_->GetMainFrame(),
      web_contents_->GetMainFrame()->GetLastCommittedURL(), net::ERR_FAILED,
      base::string16());
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ErrorHttp4XX) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: If a server responds with an HTTP 4XX error, page should enter ERROR
  // state.
  // ===========================================================================
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&DefaultHandler, net::HTTP_NOT_FOUND));
  StartTestServer();

  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(mock_cast_wc_observer_,
                OnPageStopped(CheckPageState(cast_web_contents_.get(),
                                             CastWebContents::PageState::ERROR),
                              net::ERR_FAILED))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  cast_web_contents_->LoadUrl(embedded_test_server()->GetURL("/dummy.html"));
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ErrorLoadFailed) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: When main frame load fails, enter ERROR state. This test simulates a
  // load error by intercepting the URL request and failing it with an arbitrary
  // error code.
  // ===========================================================================
  base::FilePath path = GetTestDataFilePath("dummy.html");
  GURL gurl = content::GetFileUrlWithQuery(path, "");
  content::URLLoaderInterceptor url_interceptor(base::BindRepeating(
      [](const GURL& url,
         content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url != url)
          return false;
        network::URLLoaderCompletionStatus status;
        status.error_code = net::ERR_ADDRESS_UNREACHABLE;
        params->client->OnComplete(status);
        return true;
      },
      gurl));

  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(mock_cast_wc_observer_,
                OnPageStopped(CheckPageState(cast_web_contents_.get(),
                                             CastWebContents::PageState::ERROR),
                              net::ERR_ADDRESS_UNREACHABLE))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  cast_web_contents_->LoadUrl(gurl);
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, LoadCanceledByApp) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: When the app calls window.stop(), the page should not enter the ERROR
  // state. Instead, we treat it as LOADED. This is a historical behavior for
  // some apps which intentionally stop the page and reload content.
  // ===========================================================================
  embedded_test_server()->ServeFilesFromSourceDirectory(GetTestDataPath());
  StartTestServer();

  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  cast_web_contents_->LoadUrl(
      embedded_test_server()->GetURL("/load_cancel.html"));
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, NotifyMissingResource) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: Loading a page with a missing resource should notify observers.
  // ===========================================================================
  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }
  EXPECT_CALL(mock_cast_wc_observer_,
              ResourceLoadFailed(cast_web_contents_.get()));

  base::FilePath path = GetTestDataFilePath("missing_resource.html");
  cast_web_contents_->LoadUrl(content::GetFileUrlWithQuery(path, ""));
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ExecuteJavaScriptOnLoad) {
  // ===========================================================================
  // Test: Injecting script to change title should work.
  // ===========================================================================
  constexpr char kExpectedTitle[] = "hello";
  constexpr char kOriginalTitle[] =
      "Welcome to Stan the Offline Dino's Homepage";

  // The script should be able to run before HTML <script> tag starts running.
  // The original title will be loaded first and then the injected script. Other
  // scripts must run after the injected script.
  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kExpectedTitle)));
  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kOriginalTitle)));
  constexpr char kBindingsId[] = "1234";

  GURL gurl = content::GetFileUrlWithQuery(
      GetTestDataFilePath("dynamic_title.html"), "");

  cast_web_contents_->AddBeforeLoadJavaScript(
      kBindingsId, {gurl.GetOrigin().spec()}, "stashed_title = 'hello';");

  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kExpectedTitle);
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest,
                       ExecuteJavaScriptUpdatedOnLoad) {
  // ===========================================================================
  // Test: Verify that this script replaces the previous script with same
  // binding id, as opposed to being injected alongside it. (The latter would
  // result in the title being "helloclobber").
  // ===========================================================================
  constexpr char kReplaceTitle[] = "clobber";
  constexpr char kOriginalTitle[] =
      "Welcome to Stan the Offline Dino's Homepage";

  // The script should be able to run before HTML <script> tag starts running.
  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kReplaceTitle)));
  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kOriginalTitle)));

  constexpr char kBindingsId[] = "1234";

  GURL gurl = content::GetFileUrlWithQuery(
      GetTestDataFilePath("dynamic_title.html"), "");

  cast_web_contents_->AddBeforeLoadJavaScript(
      kBindingsId, {gurl.GetOrigin().spec()}, "stashed_title = 'hello';");

  cast_web_contents_->AddBeforeLoadJavaScript(
      kBindingsId, {gurl.GetOrigin().spec()},
      "stashed_title = document.title + 'clobber';");

  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kReplaceTitle);
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest,
                       ExecuteJavaScriptOnLoadOrdered) {
  // ===========================================================================
  // Test: Verifies that bindings are injected in order by producing a
  // cumulative, non-commutative result.
  // ===========================================================================
  constexpr char kExpectedTitle[] = "hello there";
  constexpr char kOriginalTitle[] =
      "Welcome to Stan the Offline Dino's Homepage";
  constexpr char kBindingsId1[] = "1234";
  constexpr char kBindingsId2[] = "5678";

  // The script should be able to run before HTML <script> tag starts running.
  // The original title will be loaded first and then the injected script. Other
  // scripts must run after the injected script.
  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kExpectedTitle)));
  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kOriginalTitle)));

  GURL gurl = content::GetFileUrlWithQuery(
      GetTestDataFilePath("dynamic_title.html"), "");

  cast_web_contents_->AddBeforeLoadJavaScript(
      kBindingsId1, {gurl.GetOrigin().spec()}, "stashed_title = 'hello';");

  cast_web_contents_->AddBeforeLoadJavaScript(
      kBindingsId2, {gurl.GetOrigin().spec()}, "stashed_title += ' there';");

  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kExpectedTitle);
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest,
                       ExecuteJavaScriptOnLoadRemoved) {
  // ===========================================================================
  // Test: Verifies that bindings could be removed successfully before page
  // starts loading.
  // ===========================================================================
  constexpr char kExpectedTitle[] = "foo";
  constexpr char kOriginalTitle[] =
      "Welcome to Stan the Offline Dino's Homepage";
  constexpr char kBindingsId1[] = "1234";
  constexpr char kBindingsId2[] = "5678";

  // The script should be able to run before HTML <script> tag starts running.
  // The original title will be loaded first and then the injected script. Other
  // scripts must run after the injected script.
  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kExpectedTitle)));
  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kOriginalTitle)));

  GURL gurl = content::GetFileUrlWithQuery(
      GetTestDataFilePath("dynamic_title.html"), "");

  cast_web_contents_->AddBeforeLoadJavaScript(
      kBindingsId1, {gurl.GetOrigin().spec()}, "stashed_title = 'foo';");
  // Add a script which clobbers "foo".
  cast_web_contents_->AddBeforeLoadJavaScript(
      kBindingsId2, {gurl.GetOrigin().spec()}, "stashed_title = 'bar';");
  // Deletes the clobbering script.
  cast_web_contents_->RemoveBeforeLoadJavaScript(kBindingsId2);

  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kExpectedTitle);
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest,
                       ExecuteJavaScriptOnLoadWrongOrigin) {
  // ===========================================================================
  // Test: Injecting script should not happen if the to-be-loaded page's origin
  // is not whitelisted for the injection script.
  // ===========================================================================
  constexpr char kOriginalTitle[] =
      "Welcome to Stan the Offline Dino's Homepage";
  constexpr char kBindingsId[] = "1234";

  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kOriginalTitle)));

  GURL gurl = content::GetFileUrlWithQuery(
      GetTestDataFilePath("dynamic_title.html"), "");

  cast_web_contents_->AddBeforeLoadJavaScript(
      kBindingsId, {"http://example.com"}, "stashed_title = 'hello';");

  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kOriginalTitle);
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest,
                       ExecuteJavaScriptOnLoadWildcardOrigin) {
  // Start test server for hosting test HTML pages.
  embedded_test_server()->ServeFilesFromSourceDirectory(GetTestDataPath());
  StartTestServer();
  // ===========================================================================
  // Test: Injecting script should be able to load on arbitrary origins with
  // wildcard origin restriction set.
  // ===========================================================================
  constexpr char kInjectedTitle1[] = "hello";
  constexpr char kInjectedTitle2[] = "world";
  constexpr char kOriginalTitle[] =
      "Welcome to Stan the Offline Dino's Homepage";
  constexpr char kBindingsId1[] = "1234";
  constexpr char kBindingsId2[] = "5678";

  // The script should be able to run before HTML <script> tag starts running.
  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kOriginalTitle)))
      .Times(2);
  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kInjectedTitle1)));
  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kInjectedTitle2)));

  GURL gurl{embedded_test_server()->GetURL("/dynamic_title.html")};

  cast_web_contents_->AddBeforeLoadJavaScript(kBindingsId1, {"*"},
                                              "stashed_title = 'hello';");
  // Test script injection for the origin 127.0.0.1.
  // Load title "hello":
  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kInjectedTitle1);

  // Load AboutBlank page:
  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));

  cast_web_contents_->AddBeforeLoadJavaScript(kBindingsId2, {"*"},
                                              "stashed_title = 'world';");
  // Test script injection using a different origin ("localhost"), which should
  // still be picked up by the wildcard. And the title should be initialized as
  // origianl title first, then as 'world'.
  GURL alt_url =
      embedded_test_server()->GetURL("localhost", "/dynamic_title.html");
  cast_web_contents_->LoadUrl(alt_url);
  title_change_observer_.RunUntilTitleEquals(kInjectedTitle2);
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest,
                       ExecuteJavaScriptOnLoadEarlyAndLateRegistrations) {
  // ===========================================================================
  // Test: Tests that we can inject scripts before and after RenderFrame
  // creation.
  // ===========================================================================
  constexpr char kExpectedTitle1[] = "foo";
  constexpr char kExpectedTitle2[] = "foo bar";
  constexpr char kOriginalTitle[] =
      "Welcome to Stan the Offline Dino's Homepage";
  constexpr char kBindingsId1[] = "1234";
  constexpr char kBindingsId2[] = "5678";

  // The script should be able to run before HTML <script> tag starts running.
  // The original title will be loaded first and then the injected script. Other
  // scripts must run after the injected script.
  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kExpectedTitle2)));
  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kExpectedTitle1)));
  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kOriginalTitle)))
      .Times(2);

  GURL gurl = content::GetFileUrlWithQuery(
      GetTestDataFilePath("dynamic_title.html"), "");

  cast_web_contents_->AddBeforeLoadJavaScript(
      kBindingsId1, {gurl.GetOrigin().spec()}, "stashed_title = 'foo';");
  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kExpectedTitle1);

  // Inject bindings after RenderFrameCreation
  cast_web_contents_->AddBeforeLoadJavaScript(
      kBindingsId2, {gurl.GetOrigin().spec()}, "stashed_title += ' bar';");

  // Navigate away to clean the state.
  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));

  // Navigate back and see if both scripts are working.
  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kExpectedTitle2);
}

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_CONTENTS_BROWSERTEST_H_
