// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_management_url_checker_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kClassifyUrlApiPath[] =
    "https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/people/"
    "me:classifyUrl";

const char kEmail[] = "account@gmail.com";

std::string ClassificationToString(
    safe_search_api::ClientClassification classification) {
  switch (classification) {
    case safe_search_api::ClientClassification::kAllowed:
      return "allowed";
    case safe_search_api::ClientClassification::kRestricted:
      return "restricted";
    case safe_search_api::ClientClassification::kUnknown:
      return std::string();
  }
}

// Build fake JSON string with a response according to |classification|.
std::string BuildResponse(
    safe_search_api::ClientClassification classification) {
  base::DictionaryValue dict;
  dict.SetKey("displayClassification",
              base::Value(ClassificationToString(classification)));
  std::string result;
  base::JSONWriter::Write(dict, &result);
  return result;
}

}  // namespace

class KidsManagementURLCheckerClientTest : public testing::Test {
 public:
  KidsManagementURLCheckerClientTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    AccountInfo account_info =
        identity_test_env_.MakePrimaryAccountAvailable(kEmail);
    account_id_ = account_info.account_id;
    url_classifier_ = std::make_unique<KidsManagementURLCheckerClient>(
        test_shared_loader_factory_, std::string(),
        identity_test_env_.identity_manager());
  }

 protected:
  void IssueAccessTokens() {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        account_id_, "access_token",
        /*expiration_time*/ base::Time::Now() + base::TimeDelta::FromHours(1));
  }

  void IssueAccessTokenErrors() {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        account_id_, GoogleServiceAuthError::FromServiceError("Error!"));
  }

  void SetupResponse(net::HttpStatusCode status, const std::string& response) {
    test_url_loader_factory_.AddResponse(kClassifyUrlApiPath, response, status);
  }

  void CheckURL(const GURL& url) {
    url_classifier_->CheckURL(
        url, base::BindOnce(&KidsManagementURLCheckerClientTest::OnCheckDone,
                            base::Unretained(this)));
  }

  void CheckURLWithResponse(
      const GURL& url,
      const safe_search_api::ClientClassification classification) {
    CheckURL(url);
    SetupResponse(net::HTTP_OK, BuildResponse(classification));

    IssueAccessTokens();
    WaitForResponse();
  }

  // Only use this if setting response manually. CheckURLWithResponse already
  // uses this.
  void WaitForResponse() { base::RunLoop().RunUntilIdle(); }

  MOCK_METHOD2(OnCheckDone,
               void(const GURL& url,
                    safe_search_api::ClientClassification classification));

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::string account_id_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<KidsManagementURLCheckerClient> url_classifier_;
};

TEST_F(KidsManagementURLCheckerClientTest, Simple) {
  {
    GURL url("http://randomurl1.com");

    safe_search_api::ClientClassification classification =
        safe_search_api::ClientClassification::kAllowed;

    EXPECT_CALL(*this, OnCheckDone(url, classification));

    CheckURLWithResponse(url, classification);
  }
  {
    GURL url("http://randomurl2.com");

    safe_search_api::ClientClassification classification =
        safe_search_api::ClientClassification::kRestricted;

    EXPECT_CALL(*this, OnCheckDone(url, classification));

    CheckURLWithResponse(url, classification);
  }
}

TEST_F(KidsManagementURLCheckerClientTest, AccessTokenError) {
  GURL url("http://randomurl.com");

  // Our callback should get called immediately on an error.
  EXPECT_CALL(
      *this, OnCheckDone(url, safe_search_api::ClientClassification::kUnknown));
  CheckURL(url);
  IssueAccessTokenErrors();
}

TEST_F(KidsManagementURLCheckerClientTest, NetworkError) {
  GURL url("http://randomurl.com");
  CheckURL(url);

  IssueAccessTokens();

  // Our callback should get called on an error.
  EXPECT_CALL(
      *this, OnCheckDone(url, safe_search_api::ClientClassification::kUnknown));

  SetupResponse(net::HTTP_FORBIDDEN, std::string());
  WaitForResponse();
}
