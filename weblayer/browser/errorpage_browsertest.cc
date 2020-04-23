// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "weblayer/common/features.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

#if defined(OS_ANDROID)
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace weblayer {

using ErrorPageBrowserTest = WebLayerBrowserTest;

IN_PROC_BROWSER_TEST_F(ErrorPageBrowserTest, NameNotResolved) {
  GURL error_page_url =
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);

  NavigateAndWaitForFailure(error_page_url, shell());

  // Currently, interstitials for error pages are displayed only on Android.
#if defined(OS_ANDROID)
  base::string16 expected_title =
      l10n_util::GetStringUTF16(IDS_ANDROID_ERROR_PAGE_WEBPAGE_NOT_AVAILABLE);
  EXPECT_EQ(expected_title, GetTitle(shell()));
#endif
}

// Verifies that navigating to a URL that returns a 404 with an empty body
// results in the navigation failing.
IN_PROC_BROWSER_TEST_F(ErrorPageBrowserTest, 404WithEmptyBody) {
  EXPECT_TRUE(embedded_test_server()->Start());

  GURL error_page_url = embedded_test_server()->GetURL("/empty404.html");

  NavigateAndWaitForFailure(error_page_url, shell());
}

class ErrorPageReloadBrowserTest : public ErrorPageBrowserTest {
 public:
  ErrorPageReloadBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kEnableAutoReload);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class MockNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override {
    return net::NetworkChangeNotifier::CONNECTION_4G;
  }
};

IN_PROC_BROWSER_TEST_F(ErrorPageReloadBrowserTest, ReloadOnNetworkChanged) {
  // Make sure the renderer thinks it's online, since that is a necessary
  // condition for the reload.
  net::NetworkChangeNotifier::DisableForTest disable;
  MockNetworkChangeNotifier mock_notifier;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/error_page");
  // We send net::ERR_NETWORK_CHANGED on the first load, and the reload should
  // get a net::OK response.
  bool first_try = true;
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&url, &first_try](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == url) {
          if (first_try) {
            first_try = false;
            params->client->OnComplete(
                network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED));
          } else {
            content::URLLoaderInterceptor::WriteResponse(
                "weblayer/test/data/simple_page.html", params->client.get());
          }
          return true;
        }
        return false;
      }));

  NavigateAndWaitForCompletion(url, shell());
}

}  // namespace weblayer
