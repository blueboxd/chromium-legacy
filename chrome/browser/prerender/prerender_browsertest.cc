// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_link_manager.h"
#include "chrome/browser/prerender/prerender_link_manager_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/prerender/prerender_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tags_manager.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/safe_browsing/core/db/util.h"
#include "components/safe_browsing/core/features.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/ppapi_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "media/base/media_switches.h"
#include "net/base/escape.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::OpenURLParams;
using content::Referrer;
using content::RenderFrameHost;
using content::TestNavigationObserver;
using content::WebContents;
using content::WebContentsObserver;
using prerender::test_utils::TestPrerender;
using prerender::test_utils::TestPrerenderContents;
using task_manager::browsertest_util::WaitForTaskManagerRows;

// crbug.com/708158
#if !defined(OS_MACOSX) || !defined(ADDRESS_SANITIZER)

// Prerender tests work as follows:
//
// A page with a prefetch link to the test page is loaded.  Once prerendered,
// its Javascript function DidPrerenderPass() is called, which returns true if
// the page behaves as expected when prerendered.
//
// The prerendered page is then displayed on a tab.  The Javascript function
// DidDisplayPass() is called, and returns true if the page behaved as it
// should while being displayed.

namespace prerender {

namespace {

const char kPrefetchJpeg[] = "/prerender/image.jpeg";

std::string CreateServerRedirect(const std::string& dest_url) {
  const char* const kServerRedirectBase = "/server-redirect?";
  return kServerRedirectBase + net::EscapeQueryParamValue(dest_url, false);
}

// Returns true if the prerender is expected to abort on its own, before
// attempting to swap it.
bool ShouldAbortPrerenderBeforeSwap(FinalStatus status) {
  switch (status) {
    case FINAL_STATUS_USED:
    case FINAL_STATUS_APP_TERMINATING:
    case FINAL_STATUS_PROFILE_DESTROYED:
    case FINAL_STATUS_CACHE_OR_HISTORY_CLEARED:
    // We'll crash the renderer after it's loaded.
    case FINAL_STATUS_RENDERER_CRASHED:
    case FINAL_STATUS_CANCELLED:
      return false;
    default:
      return true;
  }
}

// Waits for the destruction of a RenderProcessHost's IPC channel.
// Used to make sure the PrerenderLinkManager's OnChannelClosed function has
// been called, before checking its state.
class ChannelDestructionWatcher {
 public:
  ChannelDestructionWatcher() : channel_destroyed_(false) {}

  ~ChannelDestructionWatcher() = default;

  void WatchChannel(content::RenderProcessHost* host) {
    host->AddFilter(new DestructionMessageFilter(this));
  }

  void WaitForChannelClose() {
    run_loop_.Run();
    EXPECT_TRUE(channel_destroyed_);
  }

 private:
  // When destroyed, calls ChannelDestructionWatcher::OnChannelDestroyed.
  // Ignores all messages.
  class DestructionMessageFilter : public content::BrowserMessageFilter {
   public:
    explicit DestructionMessageFilter(ChannelDestructionWatcher* watcher)
        : BrowserMessageFilter(0), watcher_(watcher) {}

   private:
    ~DestructionMessageFilter() override {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&ChannelDestructionWatcher::OnChannelDestroyed,
                         base::Unretained(watcher_)));
    }

    bool OnMessageReceived(const IPC::Message& message) override {
      return false;
    }

    ChannelDestructionWatcher* watcher_;

    DISALLOW_COPY_AND_ASSIGN(DestructionMessageFilter);
  };

  void OnChannelDestroyed() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    EXPECT_FALSE(channel_destroyed_);
    channel_destroyed_ = true;
    run_loop_.Quit();
  }

  bool channel_destroyed_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ChannelDestructionWatcher);
};

// A navigation observer to wait on either a new load or a swap of a
// WebContents. On swap, if the new WebContents is still loading, wait for that
// load to complete as well. Note that the load must begin after the observer is
// attached.
class NavigationOrSwapObserver : public WebContentsObserver,
                                 public TabStripModelObserver {
 public:
  // Waits for either a new load or a swap of |tab_strip_model|'s active
  // WebContents.
  NavigationOrSwapObserver(TabStripModel* tab_strip_model,
                           WebContents* web_contents)
      : WebContentsObserver(web_contents),
        tab_strip_model_(tab_strip_model),
        did_start_loading_(false),
        number_of_loads_(1) {
    EXPECT_NE(TabStripModel::kNoTab,
              tab_strip_model->GetIndexOfWebContents(web_contents));
    tab_strip_model_->AddObserver(this);
  }

  // Waits for either |number_of_loads| loads or a swap of |tab_strip_model|'s
  // active WebContents.
  NavigationOrSwapObserver(TabStripModel* tab_strip_model,
                           WebContents* web_contents,
                           int number_of_loads)
      : WebContentsObserver(web_contents),
        tab_strip_model_(tab_strip_model),
        did_start_loading_(false),
        number_of_loads_(number_of_loads) {
    EXPECT_NE(TabStripModel::kNoTab,
              tab_strip_model->GetIndexOfWebContents(web_contents));
    tab_strip_model_->AddObserver(this);
  }

  ~NavigationOrSwapObserver() override {
    tab_strip_model_->RemoveObserver(this);
  }

  void set_did_start_loading() { did_start_loading_ = true; }

  void Wait() { loop_.Run(); }

  // WebContentsObserver implementation:
  void DidStartLoading() override { did_start_loading_ = true; }
  void DidStopLoading() override {
    if (!did_start_loading_)
      return;
    number_of_loads_--;
    if (number_of_loads_ == 0)
      loop_.Quit();
  }

  // TabStripModelObserver implementation:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kReplaced)
      return;

    auto* replace = change.GetReplace();
    if (replace->old_contents != web_contents())
      return;

    // Switch to observing the new WebContents.
    Observe(replace->new_contents);
    if (replace->new_contents->IsLoading()) {
      // If the new WebContents is still loading, wait for it to complete.
      // Only one load post-swap is supported.
      did_start_loading_ = true;
      number_of_loads_ = 1;
    } else {
      loop_.Quit();
    }
  }

 private:
  TabStripModel* tab_strip_model_;
  bool did_start_loading_;
  int number_of_loads_;
  base::RunLoop loop_;
};

// Waits for a new tab to open and a navigation or swap in it.
class NewTabNavigationOrSwapObserver : public TabStripModelObserver,
                                       public BrowserListObserver {
 public:
  NewTabNavigationOrSwapObserver() {
    BrowserList::AddObserver(this);
    for (const Browser* browser : *BrowserList::GetInstance())
      browser->tab_strip_model()->AddObserver(this);
  }

  ~NewTabNavigationOrSwapObserver() override {
    BrowserList::RemoveObserver(this);
  }

  void Wait() {
    new_tab_run_loop_.Run();
    swap_observer_->Wait();
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kInserted || swap_observer_)
      return;

    WebContents* new_tab = change.GetInsert()->contents[0].contents;
    swap_observer_ =
        std::make_unique<NavigationOrSwapObserver>(tab_strip_model, new_tab);
    swap_observer_->set_did_start_loading();

    new_tab_run_loop_.Quit();
  }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    browser->tab_strip_model()->AddObserver(this);
  }

 private:
  base::RunLoop new_tab_run_loop_;
  std::unique_ptr<NavigationOrSwapObserver> swap_observer_;

  DISALLOW_COPY_AND_ASSIGN(NewTabNavigationOrSwapObserver);
};

}  // namespace

class PrerenderBrowserTest : public test_utils::PrerenderInProcessBrowserTest {
 public:
  PrerenderBrowserTest()
      : check_load_events_(true),
        loader_path_("/prerender/prerender_loader.html") {}

  ~PrerenderBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);

    test_utils::PrerenderInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  std::unique_ptr<TestPrerender> PrerenderTestURL(
      const std::string& html_file,
      FinalStatus expected_final_status,
      int expected_number_of_loads) {
    GURL url = src_server()->GetURL(MakeAbsolute(html_file));
    return PrerenderTestURL(url, expected_final_status,
                            expected_number_of_loads);
  }

  std::unique_ptr<TestPrerender> PrerenderTestURL(
      const GURL& url,
      FinalStatus expected_final_status,
      int expected_number_of_loads) {
    std::vector<FinalStatus> expected_final_status_queue(1,
                                                         expected_final_status);
    auto prerenders = PrerenderTestURLImpl(url, expected_final_status_queue,
                                           expected_number_of_loads);
    CHECK_EQ(1u, prerenders.size());
    return std::move(prerenders[0]);
  }

  std::vector<std::unique_ptr<TestPrerender>> PrerenderTestURL(
      const std::string& html_file,
      const std::vector<FinalStatus>& expected_final_status_queue,
      int expected_number_of_loads) {
    GURL url = src_server()->GetURL(MakeAbsolute(html_file));
    return PrerenderTestURLImpl(url, expected_final_status_queue,
                                expected_number_of_loads);
  }

  void SetUpOnMainThread() override {
    test_utils::PrerenderInProcessBrowserTest::SetUpOnMainThread();
    prerender::PrerenderManager::SetMode(
        prerender::PrerenderManager::DEPRECATED_PRERENDER_MODE_ENABLED);
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    // This one test fails with the host resolver redirecting all hosts.
    if (std::string(test_info->name()) != "PrerenderServerRedirectInIframe")
      host_resolver()->AddRule("*", "127.0.0.1");
  }

  void TearDownOnMainThread() override {
    test_utils::PrerenderInProcessBrowserTest::TearDownOnMainThread();
    interceptor_.reset();
  }

  void NavigateToDestURL() const {
    NavigateToDestURLWithDisposition(WindowOpenDisposition::CURRENT_TAB, true);
  }

  // Opens the url in a new tab, with no opener.
  void NavigateToDestURLWithDisposition(WindowOpenDisposition disposition,
                                        bool expect_swap_to_succeed) const {
    NavigateToURLWithParams(
        content::OpenURLParams(dest_url_, Referrer(), disposition,
                               ui::PAGE_TRANSITION_TYPED, false),
        expect_swap_to_succeed);
  }

  void NavigateToURLWithParams(const content::OpenURLParams& params,
                               bool expect_swap_to_succeed) const {
    NavigateToURLImpl(params, expect_swap_to_succeed);
  }

  void OpenDestURLViaClick() const { OpenURLViaClick(dest_url_); }

  void OpenURLViaClick(const GURL& url) const {
    OpenURLWithJSImpl("Click", url, GURL(), false);
  }

  void OpenDestURLViaClickTarget() const {
    OpenURLWithJSImpl("ClickTarget", dest_url_, GURL(), true);
  }

  void OpenDestURLViaClickPing(const GURL& ping_url) const {
    OpenURLWithJSImpl("ClickPing", dest_url_, ping_url, false);
  }

  void OpenDestURLViaWindowOpen() const { OpenURLViaWindowOpen(dest_url_); }

  void OpenURLViaWindowOpen(const GURL& url) const {
    OpenURLWithJSImpl("WindowOpen", url, GURL(), true);
  }

  void RemoveLinkElement(int i) const {
    GetActiveWebContents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(base::StringPrintf("RemoveLinkElement(%d)", i)),
        base::NullCallback());
  }

  void ClickToNextPageAfterPrerender() {
    TestNavigationObserver nav_observer(GetActiveWebContents());
    RenderFrameHost* render_frame_host = GetActiveWebContents()->GetMainFrame();
    render_frame_host->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("ClickOpenLink()"), base::NullCallback());
    nav_observer.Wait();
  }

  void NavigateToNextPageAfterPrerender() const {
    ui_test_utils::NavigateToURL(
        current_browser(),
        embedded_test_server()->GetURL("/prerender/prerender_page.html"));
  }

  // Called after the prerendered page has been navigated to and then away from.
  // Navigates back through the history to the prerendered page.
  void GoBackToPrerender() {
    TestNavigationObserver back_nav_observer(GetActiveWebContents());
    chrome::GoBack(current_browser(), WindowOpenDisposition::CURRENT_TAB);
    back_nav_observer.Wait();
    bool original_prerender_page = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        GetActiveWebContents(),
        "window.domAutomationController.send(IsOriginalPrerenderPage())",
        &original_prerender_page));
    EXPECT_TRUE(original_prerender_page);
  }

  void DisableLoadEventCheck() { check_load_events_ = false; }

  const PrerenderLinkManager* GetPrerenderLinkManager() const {
    PrerenderLinkManager* prerender_link_manager =
        PrerenderLinkManagerFactory::GetForBrowserContext(
            current_browser()->profile());
    return prerender_link_manager;
  }

  // Synchronization note: The IPCs used to communicate DOM events back to the
  // referring web page (see blink::mojom::PrerenderHandleClient) may race w/
  // the IPCs used here to inject script. The WaitFor* variants should be used
  // when an event was expected to happen or to happen soon.

  int GetPrerenderEventCount(int index, const std::string& type) const {
    int event_count;
    std::string expression = base::StringPrintf(
        "window.domAutomationController.send("
        "    GetPrerenderEventCount(%d, '%s'))",
        index, type.c_str());

    CHECK(content::ExecuteScriptAndExtractInt(GetActiveWebContents(),
                                              expression, &event_count));
    return event_count;
  }

  bool DidReceivePrerenderStartEventForLinkNumber(int index) const {
    return GetPrerenderEventCount(index, "webkitprerenderstart") > 0;
  }

  int GetPrerenderLoadEventCountForLinkNumber(int index) const {
    return GetPrerenderEventCount(index, "webkitprerenderload");
  }

  bool DidReceivePrerenderStopEventForLinkNumber(int index) const {
    return GetPrerenderEventCount(index, "webkitprerenderstop") > 0;
  }

  void WaitForPrerenderEventCount(int index,
                                  const std::string& type,
                                  int count) const {
    int dummy;
    std::string expression = base::StringPrintf(
        "WaitForPrerenderEventCount(%d, '%s', %d,"
        "    window.domAutomationController.send.bind("
        "        window.domAutomationController, 0))",
        index, type.c_str(), count);

    CHECK(content::ExecuteScriptAndExtractInt(GetActiveWebContents(),
                                              expression, &dummy));
    CHECK_EQ(0, dummy);
  }

  void WaitForPrerenderStartEventForLinkNumber(int index) const {
    WaitForPrerenderEventCount(index, "webkitprerenderstart", 1);
  }

  void WaitForPrerenderStopEventForLinkNumber(int index) const {
    WaitForPrerenderEventCount(index, "webkitprerenderstart", 1);
  }

  bool HadPrerenderEventErrors() const {
    bool had_prerender_event_errors;
    CHECK(content::ExecuteScriptAndExtractBool(
        GetActiveWebContents(),
        "window.domAutomationController.send(Boolean("
        "    hadPrerenderEventErrors))",
        &had_prerender_event_errors));
    return had_prerender_event_errors;
  }

  // Asserting on this can result in flaky tests.  PrerenderHandles are
  // removed from the PrerenderLinkManager when the prerender is canceled from
  // the browser, when the prerenders are cancelled from the renderer process,
  // or the channel for the renderer process is closed on the IO thread.  In the
  // last case, the code must be careful to wait for the channel to close, as it
  // is done asynchronously after swapping out the old process.  See
  // ChannelDestructionWatcher.
  bool IsEmptyPrerenderLinkManager() const {
    return GetPrerenderLinkManager()->IsEmpty();
  }

  size_t GetLinkPrerenderCount() const {
    return GetPrerenderLinkManager()->prerenders_.size();
  }

  size_t GetRunningLinkPrerenderCount() const {
    return GetPrerenderLinkManager()->CountRunningPrerenders();
  }

  void set_loader_path(const std::string& path) { loader_path_ = path; }

  GURL GetCrossDomainTestUrl(const std::string& path) {
    static const std::string secondary_domain = "www.foo.com";
    std::string url_str(base::StringPrintf(
        "http://%s:%d/%s", secondary_domain.c_str(),
        embedded_test_server()->host_port_pair().port(), path.c_str()));
    return GURL(url_str);
  }

  const GURL& dest_url() const { return dest_url_; }

  bool DidPrerenderPass(WebContents* web_contents) const {
    bool prerender_test_result = false;
    if (!content::ExecuteScriptAndExtractBool(
            web_contents,
            "window.domAutomationController.send(DidPrerenderPass())",
            &prerender_test_result))
      return false;
    return prerender_test_result;
  }

  bool DidDisplayPass(WebContents* web_contents) const {
    bool display_test_result = false;
    if (!content::ExecuteScriptAndExtractBool(
            web_contents,
            "window.domAutomationController.send(DidDisplayPass())",
            &display_test_result))
      return false;
    return display_test_result;
  }

  void AddPrerender(const GURL& url, int index) {
    std::string javascript =
        base::StringPrintf("AddPrerender('%s', %d)", url.spec().c_str(), index);
    RenderFrameHost* render_frame_host = GetActiveWebContents()->GetMainFrame();
    render_frame_host->ExecuteJavaScriptForTests(base::ASCIIToUTF16(javascript),
                                                 base::NullCallback());
  }

  base::SimpleTestTickClock* OverridePrerenderManagerTimeTicks() {
    // The default zero time causes the prerender manager to do strange things.
    clock_.Advance(base::TimeDelta::FromSeconds(1));
    GetPrerenderManager()->SetTickClockForTesting(&clock_);
    return &clock_;
  }

  // Makes |url| never respond on the first load, and then with the contents of
  // |file| afterwards. When the first load has been scheduled, runs
  // |callback_io| on the IO thread.
  void CreateHangingFirstRequestInterceptor(const GURL& url,
                                            const base::FilePath& file,
                                            base::Closure closure) {
    DCHECK(!interceptor_);
    interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindLambdaForTesting(
            [=](content::URLLoaderInterceptor::RequestParams* params) {
              if (params->url_request.url == url) {
                static bool first = true;
                if (first) {
                  first = false;
                  // Need to leak the client pipe, or else the renderer will
                  // get a disconnect error and load the error page.
                  (void)params->client.Unbind().PassPipe().release();
                  closure.Run();
                  return true;
                }
              }
              return false;
            }));
  }

 private:
  // TODO(davidben): Remove this altogether so the tests don't globally assume
  // only one prerender.
  TestPrerenderContents* GetPrerenderContents() const {
    return GetPrerenderContentsFor(dest_url_);
  }

  std::vector<std::unique_ptr<TestPrerender>> PrerenderTestURLImpl(
      const GURL& prerender_url,
      const std::vector<FinalStatus>& expected_final_status_queue,
      int expected_number_of_loads) {
    dest_url_ = prerender_url;

    GURL loader_url = ServeLoaderURL(loader_path_, "REPLACE_WITH_PRERENDER_URL",
                                     prerender_url, "");

    std::vector<std::unique_ptr<TestPrerender>> prerenders =
        NavigateWithPrerenders(loader_url, expected_final_status_queue);
    prerenders[0]->WaitForLoads(expected_number_of_loads);

    // Ensure that the referring page receives the right start and load events.
    WaitForPrerenderStartEventForLinkNumber(0);
    if (check_load_events_) {
      EXPECT_EQ(expected_number_of_loads, prerenders[0]->number_of_loads());
      WaitForPrerenderEventCount(0, "webkitprerenderload",
                                 expected_number_of_loads);
    }

    FinalStatus expected_final_status = expected_final_status_queue.front();
    if (ShouldAbortPrerenderBeforeSwap(expected_final_status)) {
      // The prerender will abort on its own. Assert it does so correctly.
      prerenders[0]->WaitForStop();
      EXPECT_FALSE(prerenders[0]->contents());
      WaitForPrerenderStopEventForLinkNumber(0);
    } else {
      // Otherwise, check that it prerendered correctly.
      TestPrerenderContents* prerender_contents = prerenders[0]->contents();
      CHECK(prerender_contents);
      EXPECT_EQ(FINAL_STATUS_UNKNOWN, prerender_contents->final_status());
      EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
    }

    // Test for proper event ordering.
    EXPECT_FALSE(HadPrerenderEventErrors());

    return prerenders;
  }

  void NavigateToURLImpl(const content::OpenURLParams& params,
                         bool expect_swap_to_succeed) const {
    ASSERT_TRUE(GetPrerenderManager());
    // Make sure in navigating we have a URL to use in the PrerenderManager.
    ASSERT_TRUE(GetPrerenderContents());

    WebContents* web_contents = GetPrerenderContents()->prerender_contents();

    // Navigate and wait for either the load to finish normally or for a swap to
    // occur.
    // TODO(davidben): The only handles CURRENT_TAB navigations, which is the
    // only case tested or prerendered right now.
    CHECK_EQ(WindowOpenDisposition::CURRENT_TAB, params.disposition);
    NavigationOrSwapObserver swap_observer(current_browser()->tab_strip_model(),
                                           GetActiveWebContents());
    WebContents* target_web_contents = current_browser()->OpenURL(params);
    swap_observer.Wait();

    if (web_contents && expect_swap_to_succeed) {
      EXPECT_EQ(web_contents, target_web_contents);
    }
  }

  // Opens the prerendered page using javascript functions in the loader
  // page. |javascript_function_name| should be a 0 argument function which is
  // invoked. |new_web_contents| is true if the navigation is expected to
  // happen in a new WebContents via OpenURL.
  void OpenURLWithJSImpl(const std::string& javascript_function_name,
                         const GURL& url,
                         const GURL& ping_url,
                         bool new_web_contents) const {
    WebContents* web_contents = GetActiveWebContents();
    RenderFrameHost* render_frame_host = web_contents->GetMainFrame();
    // Extra arguments in JS are ignored.
    std::string javascript =
        base::StringPrintf("%s('%s', '%s')", javascript_function_name.c_str(),
                           url.spec().c_str(), ping_url.spec().c_str());

    if (new_web_contents) {
      NewTabNavigationOrSwapObserver observer;
      render_frame_host->ExecuteJavaScriptWithUserGestureForTests(
          base::ASCIIToUTF16(javascript));
      observer.Wait();
    } else {
      NavigationOrSwapObserver observer(current_browser()->tab_strip_model(),
                                        web_contents);
      render_frame_host->ExecuteJavaScriptForTests(
          base::ASCIIToUTF16(javascript), base::NullCallback());
      observer.Wait();
    }
  }

  // Test TickClock that is set by OverridePrerenderManagerTimeTicks().
  base::SimpleTestTickClock clock_;

  GURL dest_url_;
  bool check_load_events_;
  std::string loader_path_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::URLLoaderInterceptor> interceptor_;
};

// Renders a page that contains a prerender link to a page that contains an
// iframe with a source that requires http authentication. This should not
// prerender successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHttpAuthentication) {
  PrerenderTestURL("/prerender/prerender_http_auth_container.html",
                   FINAL_STATUS_AUTH_NEEDED, 0);
}

// Checks that server-issued redirects within an iframe in a prerendered
// page will not count as an "alias" for the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderServerRedirectInIframe) {
  std::string redirect_path =
      CreateServerRedirect("//prerender/prerender_embedded_content.html");
  base::StringPairs replacement_text;
  replacement_text.push_back(std::make_pair("REPLACE_WITH_URL", redirect_path));
  std::string replacement_path = net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_iframe.html", replacement_text);
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  EXPECT_FALSE(
      UrlIsInPrerenderManager("/prerender/prerender_embedded_content.html"));
  NavigateToDestURL();
}

// Checks that the referrer is set when prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderReferrer) {
  PrerenderTestURL("/prerender/prerender_referrer.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that the referrer is not set when prerendering and the source page is
// HTTPS.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNoSSLReferrer) {
  // Use http:// url for the prerendered page main resource.
  GURL url(
      embedded_test_server()->GetURL("/prerender/prerender_no_referrer.html"));

  // Use https:// for all other resources.
  UseHttpsSrcServer();

  PrerenderTestURL(url, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, OpenTaskManagerBeforePrerender) {
  const base::string16 any_prerender = MatchTaskManagerPrerender("*");
  const base::string16 any_tab = MatchTaskManagerTab("*");
  const base::string16 original = MatchTaskManagerTab("Preloader");
  const base::string16 prerender = MatchTaskManagerPrerender("Prerender Page");
  const base::string16 final = MatchTaskManagerTab("Prerender Page");

  // Show the task manager. This populates the model.
  chrome::OpenTaskManager(current_browser());
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, any_prerender));

  // Prerender a page in addition to the original tab.
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  // A TaskManager entry should appear like "Prerender: Prerender Page"
  // alongside the original tab entry. There should be just these two entries.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, original));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, final));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));

  // Swap in the prerendered content.
  NavigateToDestURL();

  // The "Prerender: " TaskManager entry should disappear, being replaced by a
  // "Tab: Prerender Page" entry, and nothing else.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, original));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, final));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, any_prerender));
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, OpenTaskManagerAfterPrerender) {
  const base::string16 any_prerender = MatchTaskManagerPrerender("*");
  const base::string16 any_tab = MatchTaskManagerTab("*");
  const base::string16 original = MatchTaskManagerTab("Preloader");
  const base::string16 prerender = MatchTaskManagerPrerender("Prerender Page");
  const base::string16 final = MatchTaskManagerTab("Prerender Page");

  // Start with two resources.
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  // Show the task manager. This populates the model. Importantly, we're doing
  // this after the prerender WebContents already exists - the task manager
  // needs to find it, it can't just listen for creation.
  chrome::OpenTaskManager(current_browser());

  // A TaskManager entry should appear like "Prerender: Prerender Page"
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, original));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, final));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));

  // Swap in the tab.
  NavigateToDestURL();

  // The "Prerender: Prerender Page" TaskManager row should disappear, being
  // replaced by "Tab: Prerender Page"
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, original));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, final));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, any_prerender));
}

// Checks that the referrer policy is used when prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderReferrerPolicy) {
  set_loader_path("/prerender/prerender_loader_with_referrer_policy.html");
  PrerenderTestURL("/prerender/prerender_referrer_policy.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that the referrer policy is used when prerendering on HTTPS.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLReferrerPolicy) {
  UseHttpsSrcServer();
  set_loader_path("/prerender/prerender_loader_with_referrer_policy.html");
  PrerenderTestURL("/prerender/prerender_referrer_policy.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Test interaction of Safe Browsing with prerender. Parametrized to enable
// SafeBrowsing Delayed Warnings experiment. The experiment shouldn't delay
// prerender page loads. Otherwise, the tests will crash or timeout.
// The experiment only delays phishing warnings so the tests must use a phishing
// resource.
class PrerenderSafeBrowsingTest
    : public PrerenderBrowserTest,
      public testing::WithParamInterface<
          testing::tuple<bool /* Enable delayed warnings experiment */>> {
 public:
  PrerenderSafeBrowsingTest() {
    if (testing::get<0>(GetParam())) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{safe_browsing::kDelayedWarnings},
          /*disabled_features=*/{});
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensures that we do not prerender pages with a safe browsing
// interstitial.
IN_PROC_BROWSER_TEST_P(PrerenderSafeBrowsingTest, TopLevel) {
  GURL url = embedded_test_server()->GetURL("/prerender/prerender_page.html");
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      url, safe_browsing::SB_THREAT_TYPE_URL_PHISHING);
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_SAFE_BROWSING,
                   0);
}

// Ensures that server redirects to a malware page will cancel prerenders.
IN_PROC_BROWSER_TEST_P(PrerenderSafeBrowsingTest, ServerRedirect) {
  GURL url = embedded_test_server()->GetURL("/prerender/prerender_page.html");
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      url, safe_browsing::SB_THREAT_TYPE_URL_PHISHING);
  PrerenderTestURL(CreateServerRedirect("/prerender/prerender_page.html"),
                   FINAL_STATUS_SAFE_BROWSING, 0);
}

// Ensures that we do not prerender pages which have a malware subresource.
IN_PROC_BROWSER_TEST_P(PrerenderSafeBrowsingTest, Subresource) {
  GURL image_url = embedded_test_server()->GetURL(kPrefetchJpeg);
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      image_url, safe_browsing::SB_THREAT_TYPE_URL_PHISHING);
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", image_url.spec()));
  std::string replacement_path = net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_image.html", replacement_text);
  PrerenderTestURL(replacement_path, FINAL_STATUS_SAFE_BROWSING, 0);
}

// Ensures that we do not prerender pages which have a malware iframe.
IN_PROC_BROWSER_TEST_P(PrerenderSafeBrowsingTest, Iframe) {
  GURL iframe_url = embedded_test_server()->GetURL(
      "/prerender/prerender_embedded_content.html");
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      iframe_url, safe_browsing::SB_THREAT_TYPE_URL_PHISHING);
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", iframe_url.spec()));
  std::string replacement_path = net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_iframe.html", replacement_text);
  PrerenderTestURL(replacement_path, FINAL_STATUS_SAFE_BROWSING, 0);
}

INSTANTIATE_TEST_SUITE_P(PrerenderSafeBrowsingTest,
                         PrerenderSafeBrowsingTest,
                         testing::Combine(testing::Values(
                             false,
                             true)) /* Enable delayed warnings experiment */
);

}  // namespace prerender

#endif  // !defined(OS_MACOSX) || !defined(ADDRESS_SANITIZER)
