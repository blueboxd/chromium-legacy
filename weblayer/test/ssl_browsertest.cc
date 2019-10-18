// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/files/file_path.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

IN_PROC_BROWSER_TEST_F(WebLayerBrowserTest, Https) {
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

  // Now do a navigation that should result in an SSL error.
  GURL url_with_mismatched_cert =
      https_server_mismatched.GetURL("/simple_page.html");

  NavigateAndWaitForFailure(url_with_mismatched_cert, shell());

  // TODO(blundell): Adapt the testing of the interstitial appearing from
  // //chrome's ssl_browsertest.cc:(1462-1465) once the interstitial
  // functionality is brought up.
}

}  // namespace weblayer
