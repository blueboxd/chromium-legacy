// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace content {

// WebContentsObserver that waits until an impression is available on a
// navigation handle for a finished navigation.
class ImpressionObserver : public WebContentsObserver {
 public:
  explicit ImpressionObserver(WebContents* contents)
      : WebContentsObserver(contents) {}

  // WebContentsObserver
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (!navigation_handle->GetImpression()) {
      if (waiting_for_null_impression_)
        impression_loop_.Quit();
      return;
    }

    last_impression_ = *(navigation_handle->GetImpression());

    if (!waiting_for_null_impression_)
      impression_loop_.Quit();
  }

  const Impression& last_impression() { return *last_impression_; }

  const Impression& WaitForImpression() {
    impression_loop_.Run();
    return last_impression();
  }

  bool WaitForNavigationWithNoImpression() {
    waiting_for_null_impression_ = true;
    impression_loop_.Run();
    waiting_for_null_impression_ = false;
    return true;
  }

 private:
  base::Optional<Impression> last_impression_;
  bool waiting_for_null_impression_ = false;
  base::RunLoop impression_loop_;
};

class ImpressionDeclarationBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "content/test/data/conversions");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(https_server_.get());
    https_server_->ServeFilesFromSourceDirectory(
        "content/test/data/conversions");
    SetupCrossSiteRedirector(https_server_.get());
    ASSERT_TRUE(https_server_->Start());
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ImpressionTagClicked_ImpressionReceived) {
  ImpressionObserver impression_observer(web_contents());
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTagWithReportingAndExpiry("link" /* id */,
                        "page_with_conversion_redirect.html" /* url */,
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */,
                        "https://report.com" /* report_origin */,
                        1000 /* expiry */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));

  // Wait for the impression to be seen by the observer.
  Impression last_impression = impression_observer.WaitForImpression();

  // Verify the attributes of the impression are set as expected.
  EXPECT_EQ(1UL, last_impression.impression_data);
  EXPECT_EQ(url::Origin::Create(GURL("https://a.com")),
            last_impression.conversion_destination);
  EXPECT_EQ(url::Origin::Create(GURL("https://report.com")),
            last_impression.reporting_origin);
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1000), *last_impression.expiry);
}

// https://crbug.com/1077216
IN_PROC_BROWSER_TEST_F(
    ImpressionDeclarationBrowserTest,
    DISABLED_ImpressionTagNavigatesRemoteFrame_ImpressionReceived) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  ShellAddedObserver new_shell_observer;

  // Create an impression tag with a target frame that does not exist, which
  // will open a new window to navigate.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTagWithTarget("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */,
                        "target" /* target */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));

  ImpressionObserver impression_observer(
      new_shell_observer.GetShell()->web_contents());

  // Wait for the impression to be seen by the observer.
  Impression last_impression = impression_observer.WaitForImpression();
  EXPECT_EQ(1UL, impression_observer.last_impression().impression_data);
}

IN_PROC_BROWSER_TEST_F(
    ImpressionDeclarationBrowserTest,
    ImpressionTagNavigatesExistingRemoteFrame_ImpressionReceived) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  WebContents* initial_web_contents = web_contents();

  ShellAddedObserver new_shell_observer;
  GURL remote_url = https_server()->GetURL("c.test", "/title.html");
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("window.open($1, 'target');", remote_url)));

  // Get the new web contents associated with the remote frame.
  WebContents* remote_web_contents =
      new_shell_observer.GetShell()->web_contents();

  // Click on the impression and target the existing remote frame.
  EXPECT_TRUE(ExecJs(initial_web_contents, R"(
    createImpressionTagWithTarget("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */,
                        "target" /* target */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));

  ImpressionObserver impression_observer(remote_web_contents);

  // Wait for the impression to be seen by the observer.
  Impression last_impression = impression_observer.WaitForImpression();
  EXPECT_EQ(1UL, impression_observer.last_impression().impression_data);
}

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ImpressionTagWithOutOfBoundData_DefaultedTo0) {
  ImpressionObserver impression_observer(web_contents());
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  // The provided data overflows an unsigned 64 bit int, and should be handled
  // properly.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "FFFFFFFFFFFFFFFFFFFFFF" /* impression data */,
                        "https://a.com" /* conversion_destination */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));

  // Wait for the impression to be seen by the observer.
  Impression last_impression = impression_observer.WaitForImpression();
  EXPECT_EQ(0UL, impression_observer.last_impression().impression_data);
}

IN_PROC_BROWSER_TEST_F(
    ImpressionDeclarationBrowserTest,
    ImpressionTagNavigatesFromMiddleClick_ImpressionReceived) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  ShellAddedObserver new_shell_observer;

  // Create an impression tag that is opened via middle click. This navigates in
  // a new WebContents.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateMiddleClick(\'link\');"));

  ImpressionObserver impression_observer(
      new_shell_observer.GetShell()->web_contents());

  Impression last_impression = impression_observer.WaitForImpression();

  // Verify the attributes of the impression are set as expected.
  EXPECT_EQ(1UL, last_impression.impression_data);
}

IN_PROC_BROWSER_TEST_F(
    ImpressionDeclarationBrowserTest,
    ImpressionTagNavigatesFromEnterPress_ImpressionReceived) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */);)"));

  // Focus the element, wait for it to receive focus, and simulate an enter
  // press.
  base::string16 expected_title = base::ASCIIToUTF16("focused");
  content::TitleWatcher title_watcher(web_contents(), expected_title);
  EXPECT_TRUE(ExecJs(shell(), R"(
    let link = document.getElementById('link');
    link.addEventListener('focus', function() { document.title = 'focused'; });
    link.focus();)"));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  content::SimulateKeyPress(web_contents(), ui::DomKey::ENTER,
                            ui::DomCode::ENTER, ui::VKEY_RETURN, false, false,
                            false, false);

  ImpressionObserver impression_observer(web_contents());
  Impression last_impression = impression_observer.WaitForImpression();

  // Verify the attributes of the impression are set as expected.
  EXPECT_EQ(1UL, last_impression.impression_data);
}

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ImpressionOnInsecureSite_NotDeclared) {
  // Navigate to a page with the non-https server.
  EXPECT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(
                          "b.test", "/page_with_impression_creator.html")));

  ImpressionObserver impression_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));

  // We should see a null impression on the navigation
  EXPECT_TRUE(impression_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ImpressionWithInsecureDestination_NotDeclared) {
  // Navigate to a page with the non-https server.
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  ImpressionObserver impression_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "http://a.com" /* conversion_destination */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));

  // We should see a null impression on the navigation
  EXPECT_TRUE(impression_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ImpressionWithInsecureReportingOrigin_NotDeclared) {
  // Navigate to a page with the non-https server.
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  ImpressionObserver impression_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTagWithReportingAndExpiry("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */,
                        "http://reporting.com" /* report_origin */,
                        1000 /* expiry */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));

  // We should see a null impression on the navigation
  EXPECT_TRUE(impression_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ImpressionNavigationReloads_NoImpression) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  ImpressionObserver impression_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));
  EXPECT_EQ(1UL, impression_observer.WaitForImpression().impression_data);

  ImpressionObserver reload_observer(web_contents());
  shell()->Reload();

  // The reload navigation should not have an impression set.
  EXPECT_TRUE(reload_observer.WaitForNavigationWithNoImpression());
}

// Same as the above test but via a renderer initiated reload.
IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       RendererReloadImpressionNavigation_NoImpression) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  ImpressionObserver impression_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));
  EXPECT_EQ(1UL, impression_observer.WaitForImpression().impression_data);

  ImpressionObserver reload_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), "window.location.reload()"));

  // The reload navigation should not have an impression set.
  EXPECT_TRUE(reload_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       BackNavigateToImpressionNavigation_NoImpression) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  ImpressionObserver impression_observer(web_contents());

  // Click the default impression on the page.
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'impression_tag\');"));
  EXPECT_EQ(1UL, impression_observer.WaitForImpression().impression_data);

  // Navigate away so we can back navigate to the impression's navigated page.
  EXPECT_TRUE(NavigateToURL(web_contents(), GURL("about:blank")));

  // The back navigation should not have an impression set.
  ImpressionObserver back_nav_observer(web_contents());
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(back_nav_observer.WaitForNavigationWithNoImpression());

  // Navigate back to the original page and ensure subsequent clicks also log
  // impressions.
  ImpressionObserver second_back_nav_observer(web_contents());
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(second_back_nav_observer.WaitForNavigationWithNoImpression());

  // Wait for the page to load and render the impression tag.
  WaitForLoadStop(web_contents());
  ImpressionObserver second_impression_observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'impression_tag\');"));
  EXPECT_EQ(1UL,
            second_impression_observer.WaitForImpression().impression_data);
}

}  // namespace content
