// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

class ClientHintsBrowserTest : public WebLayerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    WebLayerBrowserTest::SetUpOnMainThread();
    BrowserProcess::GetInstance()
        ->GetNetworkQualityTracker()
        ->ReportRTTsAndThroughputForTesting(
            base::TimeDelta::FromMilliseconds(500), 100);
  }

  std::string GetBody() {
    return ExecuteScript(shell(), "document.body.innerText", true).GetString();
  }
};

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, Navigation) {
  EXPECT_TRUE(embedded_test_server()->Start());
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL(
          "/set-header?Accept-CH: device-memory,rtt&Accept-CH-Lifetime: 86400"),
      shell());

  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/echoheader?device-memory"), shell());

  double device_memory = 0.0;
  ASSERT_TRUE(base::StringToDouble(GetBody(), &device_memory));
  EXPECT_GT(device_memory, 0.0);

  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/echoheader?rtt"), shell());
  int rtt = 0;
  ASSERT_TRUE(base::StringToInt(GetBody(), &rtt));
  EXPECT_GT(rtt, 0);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, Subresource) {
  EXPECT_TRUE(embedded_test_server()->Start());
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL(
          "/set-header?Accept-CH: device-memory,rtt&Accept-CH-Lifetime: 86400"),
      shell());

  constexpr char kScript[] = R"(
    new Promise(function (resolve, reject) {
      const xhr = new XMLHttpRequest();
      xhr.open("GET", "/echoheader?" + $1);
      xhr.onload = () => {
        resolve(xhr.response);
      };
      xhr.send();
    })
  )";
  content::WebContents* web_contents =
      static_cast<TabImpl*>(shell()->tab())->web_contents();
  double device_memory = 0.0;
  ASSERT_TRUE(base::StringToDouble(
      content::EvalJs(web_contents,
                      content::JsReplace(kScript, "device-memory"))
          .ExtractString(),
      &device_memory));
  EXPECT_GT(device_memory, 0.0);

  int rtt = 0;
  ASSERT_TRUE(base::StringToInt(
      content::EvalJs(web_contents, content::JsReplace(kScript, "rtt"))
          .ExtractString(),
      &rtt));
  EXPECT_GT(rtt, 0);
}

}  // namespace weblayer
