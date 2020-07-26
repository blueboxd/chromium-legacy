// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_scripts_fetcher_impl.h"

#include "base/test/task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace {

constexpr char kOriginWithScript1[] = "https://example.com";
constexpr char kOriginWithScript2[] = "https://test.com";
constexpr char kOriginWithoutScript[] = "https://no-script.com";

constexpr char kTestResponseContent[] =
    "{\"https://example.com\" : {}, \"https://test.com\" : {}}";

url::Origin GetOriginWithScript1() {
  return url::Origin::Create(GURL(kOriginWithScript1));
}

url::Origin GetOriginWithScript2() {
  return url::Origin::Create(GURL(kOriginWithScript2));
}

url::Origin GetOriginWithoutScript() {
  return url::Origin::Create(GURL(kOriginWithoutScript));
}
}  // namespace

namespace password_manager {
class PasswordScriptsFetcherImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Recreate all classes as they are stateful.
    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_.get());
    fetcher_ = std::make_unique<PasswordScriptsFetcherImpl>(
        test_shared_loader_factory_);
  }

  void TearDown() override { EXPECT_EQ(0, GetNumberOfPendingRequests()); }

  void SimulateResponse() { SimulateResponseWithContent(kTestResponseContent); }

  void SimulateResponseWithContent(const std::string& content) {
    EXPECT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
        kChangePasswordScriptsListUrl, content));
  }

  void SimulateFailedResponse() {
    EXPECT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
        kChangePasswordScriptsListUrl, kTestResponseContent,
        net::HttpStatusCode::HTTP_BAD_REQUEST));
  }

  void RequestAllScriptsAvailability() {
    RequestSingleScriptAvailability(GetOriginWithScript1());
    RequestSingleScriptAvailability(GetOriginWithScript2());
    RequestSingleScriptAvailability(GetOriginWithoutScript());
  }

  int GetNumberOfPendingRequests() {
    return test_url_loader_factory_->NumPending();
  }

  base::flat_map<url::Origin, bool>& recorded_responses() {
    return recorded_responses_;
  }

  PasswordScriptsFetcherImpl* fetcher() { return fetcher_.get(); }

 private:
  void RequestSingleScriptAvailability(const url::Origin& origin) {
    fetcher_->GetPasswordScriptAvailability(origin,
                                            GenerateResponseCallback(origin));
  }

  void RecordResponse(url::Origin origin, bool has_script) {
    const auto& it = recorded_responses_.find(origin);
    if (it != recorded_responses_.end()) {
      EXPECT_EQ(recorded_responses_[origin], has_script)
          << "Responses for " << origin << " differ";
    } else {
      recorded_responses_[origin] = has_script;
    }
  }

  PasswordScriptsFetcher::ResponseCallback GenerateResponseCallback(
      url::Origin origin) {
    return base::BindOnce(&PasswordScriptsFetcherImplTest::RecordResponse,
                          base::Unretained(this), origin);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::flat_map<url::Origin, bool> recorded_responses_;
  std::unique_ptr<PasswordScriptsFetcherImpl> fetcher_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

TEST_F(PasswordScriptsFetcherImplTest, PrewarmCache) {
  fetcher()->PrewarmCache();
  EXPECT_EQ(1, GetNumberOfPendingRequests());
  SimulateResponse();
  EXPECT_EQ(0, GetNumberOfPendingRequests());

  // The cache is not stale yet. So, no new request is expected.
  fetcher()->PrewarmCache();
  EXPECT_EQ(0, GetNumberOfPendingRequests());

  RequestAllScriptsAvailability();
  EXPECT_THAT(recorded_responses(),
              UnorderedElementsAre(Pair(GetOriginWithScript1(), true),
                                   Pair(GetOriginWithScript2(), true),
                                   Pair(GetOriginWithoutScript(), false)));
  EXPECT_EQ(0, GetNumberOfPendingRequests());

  // Make cache stale and re-fetch the map.
  fetcher()->make_cache_stale_for_testing();
  recorded_responses().clear();
  RequestAllScriptsAvailability();
  EXPECT_EQ(1, GetNumberOfPendingRequests());
  // OriginWithScript2 (test.com) is not available anymore.
  SimulateResponseWithContent("{\"https://example.com\" : {}}");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(recorded_responses(),
              UnorderedElementsAre(Pair(GetOriginWithScript1(), true),
                                   Pair(GetOriginWithScript2(), false),
                                   Pair(GetOriginWithoutScript(), false)));
  EXPECT_EQ(0, GetNumberOfPendingRequests());
}

TEST_F(PasswordScriptsFetcherImplTest, NoPrewarmCache) {
  RequestAllScriptsAvailability();  // Without preceding |PrewarmCache|.
  EXPECT_EQ(1, GetNumberOfPendingRequests());
  SimulateResponse();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(recorded_responses(),
              UnorderedElementsAre(Pair(GetOriginWithScript1(), true),
                                   Pair(GetOriginWithScript2(), true),
                                   Pair(GetOriginWithoutScript(), false)));

  EXPECT_EQ(0, GetNumberOfPendingRequests());
}

TEST_F(PasswordScriptsFetcherImplTest, InvalidJson) {
  const char* const kTestCases[] = {"", "{{{", "[\"1\", \"2\"]"};
  for (auto* test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "test_case=" << test_case);
    fetcher()->make_cache_stale_for_testing();
    recorded_responses().clear();

    RequestAllScriptsAvailability();
    SimulateResponseWithContent(test_case);
    base::RunLoop().RunUntilIdle();

    EXPECT_THAT(recorded_responses(),
                UnorderedElementsAre(Pair(GetOriginWithScript1(), false),
                                     Pair(GetOriginWithScript2(), false),
                                     Pair(GetOriginWithoutScript(), false)));
  }
}

TEST_F(PasswordScriptsFetcherImplTest, ServerError) {
  RequestAllScriptsAvailability();
  SimulateFailedResponse();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(recorded_responses(),
              UnorderedElementsAre(Pair(GetOriginWithScript1(), false),
                                   Pair(GetOriginWithScript2(), false),
                                   Pair(GetOriginWithoutScript(), false)));
}

}  // namespace password_manager
