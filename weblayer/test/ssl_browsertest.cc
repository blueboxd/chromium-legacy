// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/files/file_path.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/interstitial_utils.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

IN_PROC_BROWSER_TEST_F(WebLayerBrowserTest, SSLErrorMismatchedCertificate) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("weblayer/test/data")));

  net::EmbeddedTestServer https_server_mismatched(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_mismatched.SetSSLConfig(
      net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server_mismatched.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("weblayer/test/data")));

  ASSERT_TRUE(https_server.Start());
  ASSERT_TRUE(https_server_mismatched.Start());

  // First navigate to an OK page.
  GURL initial_url = https_server.GetURL("/simple_page.html");
  ASSERT_EQ("127.0.0.1", initial_url.host());

  NavigateAndWaitForCompletion(initial_url, shell());
  EXPECT_FALSE(IsShowingSecurityInterstitial(shell()->browser_controller()));

  // Now do a navigation that should result in an SSL error.
  GURL url_with_mismatched_cert =
      https_server_mismatched.GetURL("/simple_page.html");

  NavigateAndWaitForFailure(url_with_mismatched_cert, shell());

  // First check that there *is* an interstitial.
  EXPECT_TRUE(IsShowingSecurityInterstitial(shell()->browser_controller()));

  // Now verify that the interstitial is in fact an SSL interstitial.
  EXPECT_TRUE(IsShowingSSLInterstitial(shell()->browser_controller()));

  // TODO(blundell): Check the security state once security state is available
  // via the public WebLayer API, following the example of //chrome's
  // ssl_browsertest.cc's CheckAuthenticationBrokenState() function.

  // TODO(blundell): Bring up test of the "Take me back" functionality
  // following the example of //chrome's ssl_browsertest.cc:1467, once that
  // functionality is implemented in weblayer.

  // Check that it's possible to navigate to a new page.
  NavigateAndWaitForCompletion(initial_url, shell());
  EXPECT_FALSE(IsShowingSecurityInterstitial(shell()->browser_controller()));
}

}  // namespace weblayer
