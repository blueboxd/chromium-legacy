// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <WebKit/WebKit.h>

#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "ios/testing/embedded_test_server_handlers.h"
#include "ios/web/public/browsing_data/cookie_blocking_mode.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/js_test_storage_util.h"
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

NSString* const kLocalStorageErrorMessage =
    @"Failed to read the 'localStorage' property from 'window': Access is "
    @"denied for this document";
NSString* const kSessionStorageErrorMessage =
    @"Failed to read the 'sessionStorage' property from 'window': Access is "
    @"denied for this document";
}

namespace web {

// A test fixture for testing that the cookie blocking feature works correctly.
class CookieBlockingTest : public WebTestWithWebState {
 protected:
  CookieBlockingTest() : WebTestWithWebState() {}
  CookieBlockingTest(const CookieBlockingTest&) = delete;
  CookieBlockingTest& operator=(const CookieBlockingTest&) = delete;


  void SetUp() override {
    WebTestWithWebState::SetUp();
    server_.RegisterRequestHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/iframe",
                            base::BindRepeating(&testing::HandleIFrame)));
    ASSERT_TRUE(server_.Start());
    ASSERT_TRUE(third_party_server_.Start());
  }

  std::string FailureMessage(WebFrame* frame) {
    std::string message = "Failure in ";
    message += (frame->IsMainFrame() ? "main frame." : "child frame.");
    return message;
  }

  net::EmbeddedTestServer server_;
  net::EmbeddedTestServer third_party_server_;
};

// Tests that cookies are accessible from JavaScript in all frames
// when the blocking mode is set to allow.
TEST_F(CookieBlockingTest, CookiesAllowed) {
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kAllow);

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
    EXPECT_TRUE(
        web::test::SetCookie(frame, @"someCookieName", @"someCookieValue"))
        << FailureMessage(frame);

    NSString* result;
    EXPECT_TRUE(web::test::GetCookies(frame, &result)) << FailureMessage(frame);
    EXPECT_NSEQ(result, @"someCookieName=someCookieValue")
        << FailureMessage(frame);
  }
}

// Tests that cookies are inaccessable from JavaScript in all frames
// when the blocking mode is set to block.
TEST_F(CookieBlockingTest, CookiesBlocked) {
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlock);

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
    EXPECT_TRUE(
        web::test::SetCookie(frame, @"someCookieName", @"someCookieValue"))
        << FailureMessage(frame);

    NSString* result;
    EXPECT_TRUE(web::test::GetCookies(frame, &result)) << FailureMessage(frame);
    EXPECT_NSEQ(result, @"") << FailureMessage(frame);
  }
}

// Tests that cookies are accessible from JavaScript on the main page, but
// inaccessible from a third-party iframe when third party cookies are blocked.
TEST_F(CookieBlockingTest, ThirdPartyCookiesBlocked) {
  GetBrowserState()->SetCookieBlockingMode(
      CookieBlockingMode::kBlockThirdParty);

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
    EXPECT_TRUE(
        web::test::SetCookie(frame, @"someCookieName", @"someCookieValue"))
        << FailureMessage(frame);

    NSString* result;
    EXPECT_TRUE(web::test::GetCookies(frame, &result)) << FailureMessage(frame);
    if (frame->IsMainFrame()) {
      EXPECT_NSEQ(result, @"someCookieName=someCookieValue")
          << FailureMessage(frame);
    } else {
      EXPECT_NSEQ(result, @"") << FailureMessage(frame);
    }
  }
}

// Tests that a first-party iframe can still access cookies when third party
// cookies are blocked.
TEST_F(CookieBlockingTest, FirstPartyCookiesNotBlockedWhenThirdPartyBlocked) {
  GetBrowserState()->SetCookieBlockingMode(
      CookieBlockingMode::kBlockThirdParty);

  GURL iframe_url = server_.GetURL(kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  for (WebFrame* frame :
       web_state()->GetWebFramesManager()->GetAllWebFrames()) {
    EXPECT_TRUE(
        web::test::SetCookie(frame, @"someCookieName", @"someCookieValue"))
        << FailureMessage(frame);

    NSString* result;
    EXPECT_TRUE(web::test::GetCookies(frame, &result)) << FailureMessage(frame);
    EXPECT_NSEQ(result, @"someCookieName=someCookieValue")
        << FailureMessage(frame);
  }
}

// Tests that the document.cookie override cannot be deleted by external
// JavaScript.
TEST_F(CookieBlockingTest, CookiesBlockedUndeletable) {
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlock);

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL(kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  web_state()->ExecuteJavaScript(base::UTF8ToUTF16("delete docuemnt.cookie"));

  WebFrame* main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
  EXPECT_TRUE(web::test::SetCookie(main_frame, @"x", @"value"));

  NSString* result;
  EXPECT_TRUE(web::test::GetCookies(main_frame, &result));
  EXPECT_NSEQ(result, @"");
}

// Tests that localStorage is accessible from JavaScript in all frames
// when the blocking mode is set to allow.
TEST_F(CookieBlockingTest, LocalStorageAllowed) {
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kAllow);

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
    NSString* error_message;
    EXPECT_TRUE(
        web::test::SetLocalStorage(frame, @"x", @"value", &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(nil, error_message) << FailureMessage(frame);

    error_message = nil;
    NSString* result;
    EXPECT_TRUE(
        web::test::GetLocalStorage(frame, @"x", &result, &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(nil, error_message) << FailureMessage(frame);
    EXPECT_NSEQ(@"value", result) << FailureMessage(frame);
  }
}

// Tests that localStorage is inaccessable from JavaScript in all frames
// when the blocking mode is set to block.
TEST_F(CookieBlockingTest, LocalStorageBlocked) {
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlock);

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
    NSString* error_message;
    EXPECT_TRUE(
        web::test::SetLocalStorage(frame, @"x", @"value", &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(error_message, kLocalStorageErrorMessage)
        << FailureMessage(frame);

    error_message = nil;
    NSString* result;
    EXPECT_TRUE(
        web::test::GetLocalStorage(frame, @"x", &result, &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(error_message, kLocalStorageErrorMessage)
        << FailureMessage(frame);
    EXPECT_NSEQ(nil, result) << FailureMessage(frame);
  }
}

// Tests that the localStorage override is undeletable via extra JavaScript.
TEST_F(CookieBlockingTest, LocalStorageBlockedUndeletable) {
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlock);

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL(kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  web_state()->ExecuteJavaScript(base::UTF8ToUTF16("delete localStorage"));

  WebFrame* main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();

  NSString* error_message;
  EXPECT_TRUE(
      web::test::SetLocalStorage(main_frame, @"x", @"value", &error_message));
  EXPECT_NSEQ(error_message, kLocalStorageErrorMessage);

  error_message = nil;
  NSString* result;
  EXPECT_TRUE(
      web::test::GetLocalStorage(main_frame, @"x", &result, &error_message));
  EXPECT_NSEQ(error_message, kLocalStorageErrorMessage);
  EXPECT_NSEQ(nil, result);
}

// Tests that sessionStorage is accessible from JavaScript in all frames
// when the blocking mode is set to allow.
TEST_F(CookieBlockingTest, SessionStorageAllowed) {
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kAllow);

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
    NSString* error_message;
    EXPECT_TRUE(
        web::test::SetSessionStorage(frame, @"x", @"value", &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(nil, error_message) << FailureMessage(frame);

    error_message = nil;
    NSString* result;
    EXPECT_TRUE(
        web::test::GetSessionStorage(frame, @"x", &result, &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(nil, error_message) << FailureMessage(frame);
    EXPECT_NSEQ(@"value", result) << FailureMessage(frame);
  }
}

// Tests that sessionStorage is inaccessable from JavaScript in all frames
// when the blocking mode is set to block.
TEST_F(CookieBlockingTest, SessionStorageBlocked) {
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlock);

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
    NSString* error_message;
    EXPECT_TRUE(
        web::test::SetSessionStorage(frame, @"x", @"value", &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(error_message, kSessionStorageErrorMessage)
        << FailureMessage(frame);

    error_message = nil;
    NSString* result;
    EXPECT_TRUE(
        web::test::GetSessionStorage(frame, @"x", &result, &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(error_message, kSessionStorageErrorMessage)
        << FailureMessage(frame);
    EXPECT_NSEQ(nil, result) << FailureMessage(frame);
  }
}

// Tests that the sessionStorage override is undeletable via extra JavaScript.
TEST_F(CookieBlockingTest, SessionStorageBlockedUndeletable) {
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlock);

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL(kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  web_state()->ExecuteJavaScript(base::UTF8ToUTF16("delete sessionStorage"));

  NSString* error_message;
  WebFrame* main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
  EXPECT_TRUE(
      web::test::SetSessionStorage(main_frame, @"x", @"value", &error_message));
  EXPECT_NSEQ(error_message, kSessionStorageErrorMessage);

  error_message = nil;
  NSString* result;
  EXPECT_TRUE(
      web::test::GetSessionStorage(main_frame, @"x", &result, &error_message));
  EXPECT_NSEQ(error_message, kSessionStorageErrorMessage);
  EXPECT_NSEQ(nil, result);
}

}  // namespace web
