// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_helper.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/signin/scoped_account_consistency.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kChromeManageAccountsHeader[] = "X-Chrome-Manage-Accounts";
const char kMirrorAction[] = "action=ADDSESSION";
const GURL kGaiaUrl("https://accounts.google.com");

// URLRequestInterceptor adding a account consistency response header to Gaia
// responses.
class TestRequestInterceptor : public net::URLRequestInterceptor {
 public:
  explicit TestRequestInterceptor(const std::string& header_name,
                                  const std::string& header_value)
      : header_name_(header_name), header_value_(header_value) {}
  ~TestRequestInterceptor() override = default;

 private:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    std::string response_headers =
        base::StringPrintf("HTTP/1.1 200 OK\n\n%s: %s\n", header_name_.c_str(),
                           header_value_.c_str());
    return new net::URLRequestTestJob(request, network_delegate,
                                      response_headers, "", true);
  }

  const std::string header_name_;
  const std::string header_value_;
};


}  // namespace

class ChromeSigninHelperTest : public testing::Test {
 protected:
  ChromeSigninHelperTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

  ~ChromeSigninHelperTest() override {}

  void SetupRequest(const std::string& header_name,
                    const std::string& header_value,
                    bool is_main_frame) {
    test_request_delegate_ = std::make_unique<net::TestDelegate>();
    request_ = url_request_context_.CreateRequest(
        kGaiaUrl, net::DEFAULT_PRIORITY, test_request_delegate_.get(),
        TRAFFIC_ANNOTATION_FOR_TESTS);
    content::ResourceRequestInfo::AllocateForTesting(
        request_.get(),
        is_main_frame ? content::ResourceType::kMainFrame
                      : content::ResourceType::kSubFrame,
        nullptr, -1, -1, -1, true, content::ResourceInterceptPolicy::kAllowNone,
        true, content::PREVIEWS_OFF, nullptr);
    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        kGaiaUrl,
        std::make_unique<TestRequestInterceptor>(header_name, header_value));
    request_->Start();
    base::RunLoop().RunUntilIdle();
    net::URLRequestFilter::GetInstance()->RemoveUrlHandler(kGaiaUrl);

    // Check that the response header is correctly set.
    net::HttpResponseHeaders* response_headers = request_->response_headers();
    ASSERT_TRUE(response_headers);
    ASSERT_TRUE(request_->response_headers()->HasHeader(header_name));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  net::TestURLRequestContext url_request_context_;
  std::unique_ptr<net::TestDelegate> test_request_delegate_;
  std::unique_ptr<net::URLRequest> request_;
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Tests that Dice response headers are removed after being processed.
TEST_F(ChromeSigninHelperTest, RemoveDiceSigninHeader) {
  ScopedAccountConsistencyDiceMigration scoped_dice_migration;

  // Create a response with the Dice header.
  SetupRequest(signin::kDiceResponseHeader, "Foo", /*is_main_frame=*/false);

  // Process the header.
  signin::ResponseAdapter response_adapter(request_.get());
  signin::ProcessAccountConsistencyResponseHeaders(&response_adapter, GURL(),
                                                   false /* is_incognito */);

  // Check that the header has been removed.
  EXPECT_FALSE(
      request_->response_headers()->HasHeader(signin::kDiceResponseHeader));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Tests that user data is set on Mirror requests.
TEST_F(ChromeSigninHelperTest, MirrorMainFrame) {
  ScopedAccountConsistencyMirror scoped_mirror;

  // Create a response with the Mirror header.
  SetupRequest(kChromeManageAccountsHeader, kMirrorAction,
               /*is_main_frame=*/true);

  // Process the header.
  signin::ResponseAdapter response_adapter(request_.get());
  signin::ProcessAccountConsistencyResponseHeaders(&response_adapter, GURL(),
                                                   false /* is_incognito */);
  // Check that the header has not been removed.
  EXPECT_TRUE(
      request_->response_headers()->HasHeader(kChromeManageAccountsHeader));
  // Request was flagged with the user data.
  EXPECT_TRUE(
      request_->GetUserData(signin::kManageAccountsHeaderReceivedUserDataKey));
}

// Tests that user data is not set on Mirror requests for sub frames.
TEST_F(ChromeSigninHelperTest, MirrorSubFrame) {
  ScopedAccountConsistencyMirror scoped_mirror;

  // Create a response with the Mirror header.
  SetupRequest(kChromeManageAccountsHeader, kMirrorAction,
               /*is_main_frame=*/false);

  // Process the header.
  signin::ResponseAdapter response_adapter(request_.get());
  signin::ProcessAccountConsistencyResponseHeaders(&response_adapter, GURL(),
                                                   false /* is_incognito */);
  // Request was not flagged with the user data.
  EXPECT_FALSE(
      request_->GetUserData(signin::kManageAccountsHeaderReceivedUserDataKey));
}
