// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <WebKit/WebKit.h>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "ios/testing/embedded_test_server_handlers.h"
#include "ios/web/public/browsing_data/cookie_blocking_mode.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "ios/web/public/web_state.h"
#include "net/base/escape.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {
// Page with text "Main frame body" and iframe with src URL equal to the URL
// query string.
const char kPageUrl[] = "/iframe?";
// URL of iframe.
const char kIFrameUrl[] = "/foo";
}

namespace web {

// A test fixture for testing that the cookie blocking feature works correctly.
class CookieBlockingTest : public WebTestWithWebState {
 protected:
  CookieBlockingTest()
      : WebTestWithWebState(std::make_unique<TestWebClient>()) {}
  CookieBlockingTest(const CookieBlockingTest&) = delete;
  CookieBlockingTest& operator=(const CookieBlockingTest&) = delete;

  TestWebClient* GetWebClient() override {
    return static_cast<TestWebClient*>(WebTest::GetWebClient());
  }

  void SetUp() override {
    WebTestWithWebState::SetUp();
    server_.RegisterRequestHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/iframe",
                            base::BindRepeating(&testing::HandleIFrame)));
    ASSERT_TRUE(server_.Start());
    ASSERT_TRUE(third_party_server_.Start());

    // Inject code to read and write cookies in child frames.
    GetWebClient()->SetEarlyAllFramesScript(
        @"__gCrWeb.test = {}; __gCrWeb.test.getCookies = function() { return "
        @"document.cookie; }; __gCrWeb.test.setCookie = function(newCookie) { "
        @"document.cookie = newCookie; };");
  }

  // Sets a persistent cookie with key, value on |web_view|.
  void SetCookie(NSString* key, NSString* value, WebFrame* web_frame) {
    std::vector<base::Value> set_params;
    NSString* cookie =
        [NSString stringWithFormat:
                      @"%@=%@; Expires=Tue, 05-May-9999 02:18:23 GMT; Path=/",
                      key, value];
    set_params.push_back(base::Value(base::SysNSStringToUTF8(cookie)));
    web_frame->CallJavaScriptFunction("test.setCookie", set_params);
  }

  // Returns a csv list of all cookies from |web_frame|.
  NSString* GetCookies(WebFrame* web_frame) {
    __block NSString* result = nil;
    __block bool completed = false;
    std::vector<base::Value> params;
    web_frame->CallJavaScriptFunction(
        "test.getCookies", params, base::BindOnce(^(const base::Value* value) {
          ASSERT_TRUE(value->is_string());
          result = base::SysUTF8ToNSString(value->GetString());
          completed = true;
        }),
        base::TimeDelta::FromSeconds(kWaitForJSCompletionTimeout));

    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
      return completed;
    }));
    return result;
  }

  net::EmbeddedTestServer server_;
  net::EmbeddedTestServer third_party_server_;
};

// Tests that cookies are accessible from Javascript in all frames
// when the blocking mode is set to allow.
TEST_F(CookieBlockingTest, CookiesAllowed) {
  BrowserState* browser_state = GetBrowserState();
  browser_state->SetCookieBlockingMode(CookieBlockingMode::kAllow);

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL(kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  for (WebFrame* frame :
       web_state()->GetWebFramesManager()->GetAllWebFrames()) {
    SetCookie(@"someCookieName", @"someCookieValue", frame);

    NSString* result = GetCookies(frame);
    EXPECT_NSEQ(result, @"someCookieName=someCookieValue");
  }
}

// Tests that cookies are inaccessable from Javascript in all frames
// when the blocking mode is set to block.
TEST_F(CookieBlockingTest, CookiesBlocked) {
  BrowserState* browser_state = GetBrowserState();
  browser_state->SetCookieBlockingMode(CookieBlockingMode::kBlock);

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL(kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  for (WebFrame* frame :
       web_state()->GetWebFramesManager()->GetAllWebFrames()) {
    SetCookie(@"someCookieName", @"someCookieValue", frame);

    NSString* result = GetCookies(frame);
    EXPECT_NSEQ(result, @"");
  }
}

// Tests that cookies are accessible from Javascript on the main page, but
// inaccessible from a third-party iframe when third party cookies are blocked.
TEST_F(CookieBlockingTest, ThirdPartyCookiesBlocked) {
  BrowserState* browser_state = GetBrowserState();
  browser_state->SetCookieBlockingMode(CookieBlockingMode::kBlockThirdParty);

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL(kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  for (WebFrame* frame :
       web_state()->GetWebFramesManager()->GetAllWebFrames()) {
    SetCookie(@"someCookieName", @"someCookieValue", frame);

    NSString* result = GetCookies(frame);
    if (frame->IsMainFrame()) {
      EXPECT_NSEQ(result, @"someCookieName=someCookieValue");
    } else {
      EXPECT_NSEQ(result, @"");
    }
  }
}

// Tests that a first-party iframe can still access cookies when third party
// cookies are blocked.
TEST_F(CookieBlockingTest, FirstPartyCookiesNotBlockedWhenThirdPartyBlocked) {
  BrowserState* browser_state = GetBrowserState();
  browser_state->SetCookieBlockingMode(CookieBlockingMode::kBlockThirdParty);

  GURL iframe_url = server_.GetURL(kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  for (WebFrame* frame :
       web_state()->GetWebFramesManager()->GetAllWebFrames()) {
    SetCookie(@"someCookieName", @"someCookieValue", frame);

    NSString* result = GetCookies(frame);
    EXPECT_NSEQ(result, @"someCookieName=someCookieValue");
  }
}

}  // namespace web
