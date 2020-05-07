// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/render_document_host_user_data.h"

#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

namespace {

int next_id = 0;

// Example class which inherits the RenderDocumentHostUserData, all the data is
// associated to the lifetime of the document.
class Data : public RenderDocumentHostUserData<Data> {
 public:
  ~Data() override = default;

  base::WeakPtr<Data> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  int unique_id() { return unique_id_; }

 private:
  explicit Data(RenderFrameHost* render_frame_host) { unique_id_ = ++next_id; }

  friend class content::RenderDocumentHostUserData<Data>;

  int unique_id_;

  base::WeakPtrFactory<Data> weak_ptr_factory_{this};

  RENDER_DOCUMENT_HOST_USER_DATA_KEY_DECL();
};

RENDER_DOCUMENT_HOST_USER_DATA_KEY_IMPL(Data)

}  // namespace

class RenderDocumentHostUserDataTest : public ContentBrowserTest {
 public:
  ~RenderDocumentHostUserDataTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* top_frame_host() {
    return web_contents()->GetFrameTree()->root()->current_frame_host();
  }
};

// Test basic functionality of RenderDocumentHostUserData.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostUserDataTest,
                       GetAndCreateForCurrentDocument) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = top_frame_host();

  // 2) Get the Data associated with this RenderFrameHost. It should be null
  // before creation.
  Data* data = Data::GetForCurrentDocument(rfh_a);
  EXPECT_FALSE(data);

  // 3) Create Data and check that GetForCurrentDocument shouldn't return null
  // now.
  Data::CreateForCurrentDocument(rfh_a);
  data = Data::GetForCurrentDocument(rfh_a);
  EXPECT_TRUE(data);
}

// Tests that RenderDocumentHostUserData objects are different for each
// RenderFrameHost in FrameTree when there are multiple frames.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostUserDataTest,
                       CheckForMultipleRFHsInFrameTree) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));

  // 1) Navigate to a(b).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = top_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();

  // 2) Create RenderDocumentHostUserData associated with both RenderFrameHosts
  // a and b.
  Data::CreateForCurrentDocument(rfh_a);
  Data* data_a = Data::GetForCurrentDocument(rfh_a);
  Data::CreateForCurrentDocument(rfh_b);
  Data* data_b = Data::GetForCurrentDocument(rfh_b);
  EXPECT_TRUE(data_a);
  EXPECT_TRUE(data_b);

  // 3) Check that RDHUD objects for both RenderFrameHost a and b have different
  // unique_id's.
  EXPECT_NE(data_a->unique_id(), data_b->unique_id());
}

// Tests that RenderDocumentHostUserData objects are cleared when the renderer
// process crashes even when having RenderFrameHost that still exists.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostUserDataTest, CheckForCrashedFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = top_frame_host();

  // 2) Create RenderDocumentHostUserData associated with A.
  Data::CreateForCurrentDocument(rfh_a);
  base::WeakPtr<Data> data = Data::GetForCurrentDocument(rfh_a)->GetWeakPtr();
  EXPECT_TRUE(data);

  // 3) Make the renderer crash.
  RenderProcessHost* renderer_process = rfh_a->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0);
  crash_observer.Wait();

  // 4) Check if the RDHUD object is deleted after renderer process crashes even
  // when RFH is around.
  EXPECT_EQ(top_frame_host(), rfh_a);
  EXPECT_FALSE(rfh_a->IsRenderFrameLive());
  EXPECT_FALSE(data);
}

// Tests that RenderDocumentHostUserData object is created for speculative
// RenderFrameHost and check if they point to same object before and after
// commit.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostUserDataTest,
                       CheckIDsForSpeculativeRFHBeforeAndAfterCommit) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // Isolate "b.com" so we are guaranteed to get a different process
  // for navigations to this origin on Android. Doing this ensures that a
  // speculative RenderFrameHost is used.
  IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                           {"b.com"});

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // 2) Start navigation to B, but don't commit yet.
  TestNavigationManager manager(shell()->web_contents(), url_b);
  shell()->LoadURL(url_b);
  EXPECT_TRUE(manager.WaitForRequestStart());

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderFrameHostImpl* pending_rfh =
      root->render_manager()->speculative_frame_host();
  NavigationRequest* navigation_request = root->navigation_request();
  EXPECT_EQ(navigation_request->associated_site_instance_type(),
            NavigationRequest::AssociatedSiteInstanceType::SPECULATIVE);
  EXPECT_TRUE(pending_rfh);

  // 3) While there is a speculative RenderFrameHost in the root FrameTreeNode,
  // get the Data associated with this RenderFrameHost.
  Data::CreateForCurrentDocument(pending_rfh);
  Data* data_before_commit = Data::GetForCurrentDocument(pending_rfh);
  EXPECT_TRUE(data_before_commit);

  // 4) Let the navigation finish and make sure it is succeeded.
  manager.WaitForNavigationFinished();
  EXPECT_EQ(url_b, web_contents()->GetMainFrame()->GetLastCommittedURL());

  RenderFrameHostImpl* rfh_b = top_frame_host();
  EXPECT_EQ(pending_rfh, rfh_b);
  Data* data_after_commit = Data::GetForCurrentDocument(rfh_b);
  EXPECT_TRUE(data_after_commit);

  // 5) Check |data_before_commit| and |data_after_commit| have same ID.
  EXPECT_EQ(data_before_commit->unique_id(), data_after_commit->unique_id());
}

// Tests that RenderDocumentHostUserData object is deleted when the speculative
// RenderFrameHost gets deleted before being able to commit.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostUserDataTest, SpeculativeRFHDeleted) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/hung"));
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // 1) Initial state: A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = web_contents()->GetMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();

  // RFH B has an unload handler.
  auto detach_filter_b = base::MakeRefCounted<DropMessageFilter>(
      FrameMsgStart, FrameHostMsg_Detach::ID);
  rfh_b->GetProcess()->AddFilter(detach_filter_b.get());
  EXPECT_TRUE(ExecJs(rfh_b, "onunload=function(){}"));

  // 2) Navigation from B to C. The server is slow to respond.
  TestNavigationManager navigation_observer(web_contents(), url_c);
  EXPECT_TRUE(ExecJs(rfh_b, JsReplace("location.href=$1;", url_c)));
  EXPECT_TRUE(navigation_observer.WaitForRequestStart());
  RenderFrameHostImpl* pending_rfh_c =
      rfh_b->frame_tree_node()->render_manager()->speculative_frame_host();

  // 3) While there is a speculative RenderFrameHost get the Data associated
  // with it.
  Data::CreateForCurrentDocument(pending_rfh_c);
  base::WeakPtr<Data> data =
      Data::GetForCurrentDocument(pending_rfh_c)->GetWeakPtr();
  EXPECT_TRUE(data);

  // 4) Delete the speculative RenderFrameHost by navigating from a
  // RenderFrameHost in pending deletion.
  RenderFrameDeletedObserver delete_speculative_c(pending_rfh_c);
  EXPECT_TRUE(
      ExecJs(rfh_a, JsReplace("document.querySelector('iframe').remove();")));
  delete_speculative_c.WaitUntilDeleted();
  EXPECT_TRUE(delete_speculative_c.deleted());

  // 5) Once the speculative RenderFrameHost is deleted, the associated
  // RenderDocumentHostUserData should be deleted.
  EXPECT_FALSE(data);
}

// Tests that RenderDocumentHostUserData is cleared when the RenderFrameHost is
// deleted.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostUserDataTest, RenderFrameHostDeleted) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));

  // 1) Navigate to a(b).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderFrameHostImpl* rfh_a = top_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 2) Get the Data associated with the rfh_b and check the data gets cleared
  // on RenderFrameHost deletion.
  Data::CreateForCurrentDocument(rfh_b);
  base::WeakPtr<Data> data = Data::GetForCurrentDocument(rfh_b)->GetWeakPtr();
  EXPECT_TRUE(data);

  // 3) Detach the child frame.
  EXPECT_TRUE(ExecJs(root, "document.querySelector('iframe').remove()"));

  // 4) Once the RenderFrameHost is deleted, the associated
  // RenderDocumentHostUserData should be deleted.
  delete_observer_rfh_b.WaitUntilDeleted();
  EXPECT_FALSE(data);
}

// Tests that RenderDocumentHostUserData object is not cleared when the
// RenderFrameHost is in pending deletion state for both main frame and sub
// frame.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostUserDataTest,
                       CheckInPendingDeletionState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  std::string onunload_script = "window.onunload = function(){ while(1); }";
  GURL url_ab(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_ab));
  RenderFrameHostImpl* rfh_a = top_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();

  // 2) Act as if there is a slow unload handler on rfh_a and rfh_b.
  LeaveInPendingDeletionState(rfh_a);
  rfh_b->SetSubframeUnloadTimeoutForTesting(base::TimeDelta::FromSeconds(30));
  auto detach_filter = base::MakeRefCounted<DropMessageFilter>(
      FrameMsgStart, FrameHostMsg_Detach::ID);
  rfh_b->GetProcess()->AddFilter(detach_filter.get());
  EXPECT_TRUE(ExecuteScript(rfh_b->frame_tree_node(), onunload_script));

  // 3) Create RDHUD object for both rfh_a and rfh_b before running unload
  // handlers.
  Data::CreateForCurrentDocument(rfh_a);
  Data::CreateForCurrentDocument(rfh_b);

  base::WeakPtr<Data> data_a = Data::GetForCurrentDocument(rfh_a)->GetWeakPtr();
  base::WeakPtr<Data> data_b = Data::GetForCurrentDocument(rfh_b)->GetWeakPtr();
  EXPECT_TRUE(data_a);
  EXPECT_TRUE(data_b);

  // 4) Navigate from A(B) to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));

  // 5) Check RDHUD objects |data_a| and |data_b| are not cleared when rfh_a and
  // rfh_b are in pending deletion state.
  EXPECT_EQ(rfh_a->lifecycle_state(),
            RenderFrameHostImpl::LifecycleState::kRunningUnloadHandlers);
  EXPECT_EQ(rfh_b->lifecycle_state(),
            RenderFrameHostImpl::LifecycleState::kRunningUnloadHandlers);
  EXPECT_TRUE(data_a);
  EXPECT_TRUE(data_b);

  EXPECT_FALSE(rfh_a->IsCurrent());
  EXPECT_FALSE(rfh_b->IsCurrent());
}

// Tests that RenderDocumentHostUserData associated with RenderFrameHost is not
// cleared on same document navigation.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostUserDataTest,
                       CommitSameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title1.html#2"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = top_frame_host();

  // 2) Get the Data associated with this RenderFrameHost.
  Data::CreateForCurrentDocument(rfh_a);
  Data* data = Data::GetForCurrentDocument(rfh_a);
  EXPECT_TRUE(data);

  // 3) Navigate to A#2 (same document navigation).
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location = $1", url_a2.spec())));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(url_a2, web_contents()->GetMainFrame()->GetLastCommittedURL());

  // 4) Check if the RDHUD objects are pointing to the same instance after
  // navigation.
  Data* data2 = Data::GetForCurrentDocument(rfh_a);
  EXPECT_TRUE(data2);
  EXPECT_EQ(data->unique_id(), data2->unique_id());
}

// Tests that RenderDocumentHostUserData object is not cleared when navigation
// is cancelled.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostUserDataTest, CancelledNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = top_frame_host();

  // 2) Get the Data associated with this RenderFrameHost.
  Data::CreateForCurrentDocument(rfh_a);
  Data* data = Data::GetForCurrentDocument(rfh_a);
  EXPECT_TRUE(data);

  // 3) Cancel all navigation attempts.
  TestNavigationThrottleInserter throttle_inserter(
      shell()->web_contents(),
      base::BindLambdaForTesting(
          [&](NavigationHandle* handle) -> std::unique_ptr<NavigationThrottle> {
            auto throttle = std::make_unique<TestNavigationThrottle>(handle);
            throttle->SetResponse(TestNavigationThrottle::WILL_START_REQUEST,
                                  TestNavigationThrottle::SYNCHRONOUS,
                                  NavigationThrottle::CANCEL_AND_IGNORE);
            return throttle;
          }));

  // 4) Try navigating to B.
  EXPECT_FALSE(NavigateToURL(shell(), url_b));

  // 5) We should still be showing page A.
  EXPECT_EQ(rfh_a, top_frame_host());

  // 6) The data shouldn't be cleared in the case of cancelled navigations and
  // should be pointing to the same instances.
  Data* data2 = Data::GetForCurrentDocument(rfh_a);
  EXPECT_TRUE(data2);
  EXPECT_EQ(data->unique_id(), data2->unique_id());
}

// Tests that RenderDocumentHostUserData object is cleared when a failed
// navigation results in an error page.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostUserDataTest, FailedNavigation) {
  // This test is only valid if error page isolation is enabled.
  if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(true))
    return;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL error_url(embedded_test_server()->GetURL("/close-socket"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(error_url,
                                                   net::ERR_DNS_TIMED_OUT);

  // 1) Start with a successful navigation to a document.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* rfh_a = top_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Get the Data associated with RenderFrameHost associated with url.
  Data::CreateForCurrentDocument(rfh_a);
  base::WeakPtr<Data> data = Data::GetForCurrentDocument(rfh_a)->GetWeakPtr();
  EXPECT_TRUE(data);

  // 3) Browser-initiated navigation to an error page.
  NavigationHandleObserver observer(shell()->web_contents(), error_url);
  EXPECT_FALSE(NavigateToURL(shell(), error_url));
  EXPECT_TRUE(observer.is_error());
  EXPECT_EQ(net::ERR_DNS_TIMED_OUT, observer.net_error_code());

  // 4) The associated RenderDocumentHostUserData object should be deleted.
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_FALSE(data);
}

// Tests that RenderDocumentHostUserData object is cleared when it is neither a
// same document navigation nor when it is stored in back-forward cache after
// navigating away.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostUserDataTest, CrossSiteNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = top_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Get the Data associated with this RenderFrameHost.
  Data::CreateForCurrentDocument(rfh_a);
  base::WeakPtr<Data> data = Data::GetForCurrentDocument(rfh_a)->GetWeakPtr();
  EXPECT_TRUE(data);

  // 3) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_NE(rfh_a, top_frame_host());

  // 4) Both rfh_a and RDHUD should be deleted.
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_FALSE(data);
}

// Tests that RenderDocumentHostUserData object is cleared on performing same
// site navigation.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostUserDataTest, SameSiteNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  RenderFrameHostImpl* rfh_a1 = top_frame_host();

  // 2) Get the Data associated with this RenderFrameHost.
  Data::CreateForCurrentDocument(rfh_a1);
  base::WeakPtr<Data> data = Data::GetForCurrentDocument(rfh_a1)->GetWeakPtr();

  EXPECT_TRUE(data);

  // 3) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  EXPECT_EQ(rfh_a1, top_frame_host());

  // 4) The associated RenderDocumentHostUserData should be deleted.
  EXPECT_FALSE(data);
}

// Test RenderDocumentHostUserData with BackForwardCache feature enabled.
class RenderDocumentHostUserDataWithBackForwardCacheTest
    : public RenderDocumentHostUserDataTest {
 public:
  RenderDocumentHostUserDataWithBackForwardCacheTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kBackForwardCache,
        {
            // Set a very long TTL before expiration (longer than the test
            // timeout) so tests that are expecting deletion don't pass when
            // they shouldn't.
            {"TimeToLiveInBackForwardCacheInSeconds", "3600"},
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that RenderDocumentHostUserData object is not cleared on storing and
// restoring a page from back-forward cache.
IN_PROC_BROWSER_TEST_F(RenderDocumentHostUserDataWithBackForwardCacheTest,
                       BackForwardCacheNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = top_frame_host();

  // 2) Get the Data associated with this RenderFrameHost.
  Data::CreateForCurrentDocument(rfh_a);
  Data* data = Data::GetForCurrentDocument(rfh_a);
  EXPECT_TRUE(data);

  //  3) Navigate to B. A should be stored in back-forward cache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Data associated with document shouldn't have been cleared on navigating
  // away with BackForwardCache.
  data = Data::GetForCurrentDocument(rfh_a);
  EXPECT_TRUE(data);

  // 5) Go back to A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  Data* data_after_restore =
      Data::GetForCurrentDocument(web_contents()->GetMainFrame());
  EXPECT_TRUE(data);

  // 6) Both the instances of Data before and after restore should point to the
  // same object and make sure they aren't null.
  EXPECT_EQ(data_after_restore->unique_id(), data->unique_id());
}

}  // namespace content
