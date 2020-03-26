// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/hats_survey_status_checker.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {

class TestHatsSurveyStatusChecker : public HatsSurveyStatusChecker {
 public:
  explicit TestHatsSurveyStatusChecker(Profile* profile)
      : HatsSurveyStatusChecker(profile) {}
  TestHatsSurveyStatusChecker(const TestHatsSurveyStatusChecker&) = delete;
  TestHatsSurveyStatusChecker& operator=(const TestHatsSurveyStatusChecker&) =
      delete;
  ~TestHatsSurveyStatusChecker() override = default;

  std::string HatsSurveyURLWithoutId() override {
    return HatsSurveyStatusChecker::HatsSurveyURLWithoutId();
  }
};

class FakeHatsSurveyStatusChecker : public TestHatsSurveyStatusChecker {
 public:
  static constexpr int kTimeoutSecs = 1;

  FakeHatsSurveyStatusChecker(Profile* profile, const std::string& url)
      : TestHatsSurveyStatusChecker(profile), url_(url) {}
  FakeHatsSurveyStatusChecker(const FakeHatsSurveyStatusChecker&) = delete;
  FakeHatsSurveyStatusChecker& operator=(const FakeHatsSurveyStatusChecker&) =
      delete;
  ~FakeHatsSurveyStatusChecker() override = default;

  std::string HatsSurveyURLWithoutId() override {
    return url_ + kHatsSurveyDataPath;
  }
  int SurveyCheckTimeoutSecs() override { return kTimeoutSecs; }

 private:
  const std::string url_;
};

class HatsSurveyStatusCheckerBrowserTestBase : public InProcessBrowserTest {
 public:
  static constexpr char kNonReachableSiteId[] = "01234";
  static constexpr char kErrorSiteId[] = "9999";
  static constexpr char kOutOfCapacitySiteId[] = "543210";
  static constexpr char kNormalSiteId[] = "1356789";

  HatsSurveyStatusCheckerBrowserTestBase() = default;
  HatsSurveyStatusCheckerBrowserTestBase(
      const HatsSurveyStatusCheckerBrowserTestBase&) = delete;
  HatsSurveyStatusCheckerBrowserTestBase& operator=(
      const HatsSurveyStatusCheckerBrowserTestBase&) = delete;
  ~HatsSurveyStatusCheckerBrowserTestBase() override = default;

  // Handles |request| by serving different response based on the site id in the
  // request.
  std::unique_ptr<net::test_server::HttpResponse> ResponseHandler(
      const net::test_server::HttpRequest& request) {
    std::string::size_type substr_index =
        request.relative_url.find(HatsSurveyStatusChecker::kHatsSurveyDataPath);
    if (substr_index == std::string::npos)
      return std::unique_ptr<net::test_server::HttpResponse>();

    std::string site_id = request.relative_url.substr(
        substr_index + strlen(HatsSurveyStatusChecker::kHatsSurveyDataPath));
    if (site_id.compare(kNonReachableSiteId) == 0) {
      // Directly post a call to timeout callback function.
      base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                     std::move(timeout_callback_));
      return std::unique_ptr<net::test_server::HttpResponse>();
    }

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    if (site_id.compare(kErrorSiteId) == 0) {
      // Return a response with error header.
      http_response->set_code(net::HTTP_NO_CONTENT);
    } else if (site_id.compare(kOutOfCapacitySiteId) == 0) {
      // Return header suggests 'out of capacity'.
      http_response->set_code(net::HTTP_OK);
      http_response->AddCustomHeader(
          HatsSurveyStatusChecker::kReasonHeader,
          HatsSurveyStatusChecker::kReasonOverCapacity);
    } else if (site_id.compare(kNormalSiteId) == 0) {
      // Return the normal response.
      http_response->set_code(net::HTTP_OK);
    }
    return std::move(http_response);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &HatsSurveyStatusCheckerBrowserTestBase::ResponseHandler,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetClosure(base::OnceClosure quit_closure,
                  base::OnceClosure timeout_callback) {
    quit_closure_ = std::move(quit_closure);
    timeout_callback_ = std::move(timeout_callback);
  }

  // Callback functions.
  void SurveyStatusOk() {
    result_ = HatsSurveyStatusChecker::Status::kSuccess;
    std::move(quit_closure_).Run();
  }

  void SurveyStatusFailed(HatsSurveyStatusChecker::Status status) {
    result_ = status;
    std::move(quit_closure_).Run();
  }

  HatsSurveyStatusChecker::Status result() { return result_; }

 private:
  HatsSurveyStatusChecker::Status result_ =
      HatsSurveyStatusChecker::Status::kUnreachable;
  base::OnceClosure quit_closure_;
  base::OnceClosure timeout_callback_;
};

}  // namespace

constexpr char HatsSurveyStatusCheckerBrowserTestBase::kNonReachableSiteId[];
constexpr char HatsSurveyStatusCheckerBrowserTestBase::kErrorSiteId[];
constexpr char HatsSurveyStatusCheckerBrowserTestBase::kOutOfCapacitySiteId[];
constexpr char HatsSurveyStatusCheckerBrowserTestBase::kNormalSiteId[];

// Check the url is formatted correctly such as its host name is separated from
// the path. So we can extract the expected host name from the url.
IN_PROC_BROWSER_TEST_F(HatsSurveyStatusCheckerBrowserTestBase,
                       CheckHostInStatusURLCorrect) {
  auto checker =
      std::make_unique<TestHatsSurveyStatusChecker>(browser()->profile());
  GURL url(checker->HatsSurveyURLWithoutId());
  EXPECT_STREQ("www.google.com", url.host().c_str());

  auto fake_checker = std::make_unique<FakeHatsSurveyStatusChecker>(
      browser()->profile(), embedded_test_server()->base_url().spec());
  GURL test_url(fake_checker->HatsSurveyURLWithoutId());
  EXPECT_EQ(embedded_test_server()->base_url().host(), test_url.host());
}

IN_PROC_BROWSER_TEST_F(HatsSurveyStatusCheckerBrowserTestBase,
                       CheckStatusResults) {
  struct SiteResult {
    const char* site_id;
    HatsSurveyStatusChecker::Status result;
  } site_id_with_result[] = {
      {HatsSurveyStatusCheckerBrowserTestBase::kNonReachableSiteId,
       HatsSurveyStatusChecker::Status::kUnreachable},
      {HatsSurveyStatusCheckerBrowserTestBase::kErrorSiteId,
       HatsSurveyStatusChecker::Status::kResponseHeaderError},
      {HatsSurveyStatusCheckerBrowserTestBase::kOutOfCapacitySiteId,
       HatsSurveyStatusChecker::Status::kOverCapacity},
      {HatsSurveyStatusCheckerBrowserTestBase::kNormalSiteId,
       HatsSurveyStatusChecker::Status::kSuccess}};

  auto checker = std::make_unique<FakeHatsSurveyStatusChecker>(
      browser()->profile(), embedded_test_server()->base_url().spec());
  for (const auto& item : site_id_with_result) {
    base::RunLoop request_wait;
    SetClosure(request_wait.QuitClosure(),
               checker->CreateTimeoutCallbackForTesting());
    checker->CheckSurveyStatus(
        item.site_id,
        base::BindOnce(&HatsSurveyStatusCheckerBrowserTestBase::SurveyStatusOk,
                       base::Unretained(this)),
        base::BindOnce(
            &HatsSurveyStatusCheckerBrowserTestBase::SurveyStatusFailed,
            base::Unretained(this)));
    request_wait.Run();
    EXPECT_EQ(item.result, result());
  }
}
