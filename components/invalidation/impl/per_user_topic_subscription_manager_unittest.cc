// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_subscription_manager.h"

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace syncer {

namespace {

size_t kInvalidationObjectIdsCount = 5;

const char kInvalidationRegistrationScope[] =
    "https://firebaseperusertopics-pa.googleapis.com";

const char kProjectId[] = "8181035976";

const char kTypeSubscribedForInvalidation[] =
    "invalidation.per_sender_registered_for_invalidation";

const char kActiveRegistrationTokens[] =
    "invalidation.per_sender_active_registration_tokens";

const char kFakeInstanceIdToken[] = "fake_instance_id_token";

std::string IndexToName(size_t index) {
  char name[2] = "a";
  name[0] += static_cast<char>(index);
  return name;
}

Topics GetSequenceOfTopicsStartingAt(size_t start, size_t count) {
  Topics ids;
  for (size_t i = start; i < start + count; ++i)
    ids.emplace(IndexToName(i), TopicMetadata{false});
  return ids;
}

Topics GetSequenceOfTopics(size_t count) {
  return GetSequenceOfTopicsStartingAt(0, count);
}

TopicSet TopicSetFromTopics(const Topics& topics) {
  TopicSet topic_set;
  for (auto& topic : topics) {
    topic_set.insert(topic.first);
  }
  return topic_set;
}

network::mojom::URLResponseHeadPtr CreateHeadersForTest(int responce_code) {
  auto head = network::mojom::URLResponseHead::New();
  head->headers = new net::HttpResponseHeaders(base::StringPrintf(
      "HTTP/1.1 %d OK\nContent-type: text/html\n\n", responce_code));
  head->mime_type = "text/html";
  return head;
}

GURL FullSubscriptionUrl(const std::string& token) {
  return GURL(base::StringPrintf(
      "%s/v1/perusertopics/%s/rel/topics/?subscriber_token=%s",
      kInvalidationRegistrationScope, kProjectId, token.c_str()));
}

GURL FullUnSubscriptionUrlForTopic(const std::string& topic) {
  return GURL(base::StringPrintf(
      "%s/v1/perusertopics/%s/rel/topics/%s?subscriber_token=%s",
      kInvalidationRegistrationScope, kProjectId, topic.c_str(),
      kFakeInstanceIdToken));
}

network::URLLoaderCompletionStatus CreateStatusForTest(
    int status,
    const std::string& response_body) {
  network::URLLoaderCompletionStatus response_status(status);
  response_status.decoded_body_length = response_body.size();
  return response_status;
}

}  // namespace

class RegistrationManagerStateObserver
    : public PerUserTopicSubscriptionManager::Observer {
 public:
  void OnSubscriptionChannelStateChanged(
      SubscriptionChannelState state) override {
    state_ = state;
  }

  SubscriptionChannelState observed_state() const { return state_; }

 private:
  SubscriptionChannelState state_ = SubscriptionChannelState::NOT_STARTED;
};

class PerUserTopicSubscriptionManagerTest : public testing::Test {
 protected:
  PerUserTopicSubscriptionManagerTest() {}

  ~PerUserTopicSubscriptionManagerTest() override {}

  void SetUp() override {
    PerUserTopicSubscriptionManager::RegisterProfilePrefs(
        pref_service_.registry());
    AccountInfo account =
        identity_test_env_.MakePrimaryAccountAvailable("example@gmail.com");
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
    identity_provider_ =
        std::make_unique<invalidation::ProfileIdentityProvider>(
            identity_test_env_.identity_manager());
    identity_provider_->SetActiveAccountId(account.account_id);
  }

  std::unique_ptr<PerUserTopicSubscriptionManager> BuildRegistrationManager(
      bool migrate_prefs = true) {
    auto reg_manager = std::make_unique<PerUserTopicSubscriptionManager>(
        identity_provider_.get(), &pref_service_, url_loader_factory(),
        kProjectId, migrate_prefs);
    reg_manager->Init();
    reg_manager->AddObserver(&state_observer_);
    return reg_manager;
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  const base::Value* GetSubscribedTopics() {
    return pref_service()
        ->GetDictionary(kTypeSubscribedForInvalidation)
        ->FindDictKey(kProjectId);
  }

  SubscriptionChannelState observed_state() {
    return state_observer_.observed_state();
  }

  void AddCorrectSubscriptionResponce(
      const std::string& private_topic = std::string(),
      const std::string& token = kFakeInstanceIdToken,
      int http_responce_code = net::HTTP_OK) {
    std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
    value->SetString("privateTopicName",
                     private_topic.empty() ? "test-pr" : private_topic.c_str());
    std::string serialized_response;
    JSONStringValueSerializer serializer(&serialized_response);
    serializer.Serialize(*value);
    url_loader_factory()->AddResponse(
        FullSubscriptionUrl(token), CreateHeadersForTest(http_responce_code),
        serialized_response, CreateStatusForTest(net::OK, serialized_response));
  }

  void AddCorrectUnSubscriptionResponceForTopic(const std::string& topic) {
    url_loader_factory()->AddResponse(
        FullUnSubscriptionUrlForTopic(topic),
        CreateHeadersForTest(net::HTTP_OK), std::string() /* response_body */,
        CreateStatusForTest(net::OK, std::string() /* response_body */));
  }

  void FastForwardTimeBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  network::TestURLLoaderFactory url_loader_factory_;
  TestingPrefServiceSimple pref_service_;

  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<invalidation::ProfileIdentityProvider> identity_provider_;

  RegistrationManagerStateObserver state_observer_;

  DISALLOW_COPY_AND_ASSIGN(PerUserTopicSubscriptionManagerTest);
};

TEST_F(PerUserTopicSubscriptionManagerTest,
       EmptyPrivateTopicShouldNotUpdateSubscribedTopics) {
  auto ids = GetSequenceOfTopics(kInvalidationObjectIdsCount);

  auto per_user_topic_subscription_manager = BuildRegistrationManager();

  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  // Empty response body should result in no successful registrations.
  std::string response_body;

  url_loader_factory()->AddResponse(
      FullSubscriptionUrl(kFakeInstanceIdToken),
      CreateHeadersForTest(net::HTTP_OK), response_body,
      CreateStatusForTest(net::OK, response_body));

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();

  // The response didn't contain non-empty topic name. So nothing was
  // registered.
  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());
}

TEST_F(PerUserTopicSubscriptionManagerTest, ShouldUpdateSubscribedTopics) {
  auto ids = GetSequenceOfTopics(kInvalidationObjectIdsCount);

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  ASSERT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  AddCorrectSubscriptionResponce();

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(TopicSetFromTopics(ids),
            per_user_topic_subscription_manager->GetSubscribedTopicsForTest());
  EXPECT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  for (const auto& id : ids) {
    const base::Value* topics = GetSubscribedTopics();
    const base::Value* private_topic_value =
        topics->FindKeyOfType(id.first, base::Value::Type::STRING);
    ASSERT_NE(private_topic_value, nullptr);
  }
}

TEST_F(PerUserTopicSubscriptionManagerTest, ShouldRepeatRequestsOnFailure) {
  auto ids = GetSequenceOfTopics(kInvalidationObjectIdsCount);

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  ASSERT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  // The first subscription attempt will fail.
  AddCorrectSubscriptionResponce(
      /*private_topic=*/std::string(), kFakeInstanceIdToken,
      net::HTTP_INTERNAL_SERVER_ERROR);

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();

  // Since the subscriptions failed, the requests should still be pending.
  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());
  EXPECT_FALSE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // The second attempt will succeed.
  AddCorrectSubscriptionResponce();

  // Initial backoff is 2 seconds with 20% jitter, so the minimum possible delay
  // is 1600ms. Advance time to just before that; nothing should have changed
  // yet.
  FastForwardTimeBy(base::TimeDelta::FromMilliseconds(1500));
  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());
  EXPECT_FALSE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // The maximum backoff is 2 seconds; advance to just past that. Now all
  // subscriptions should have finished.
  FastForwardTimeBy(base::TimeDelta::FromMilliseconds(600));
  EXPECT_FALSE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                   .empty());
  EXPECT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());
}

TEST_F(PerUserTopicSubscriptionManagerTest,
       ShouldInvalidateAccessTokenOnUnauthorized) {
  // For this test, we need to manually control when access tokens are returned.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);

  auto ids = GetSequenceOfTopics(kInvalidationObjectIdsCount);

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  ASSERT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  // The first subscription attempt will fail with an "unauthorized" error.
  AddCorrectSubscriptionResponce(
      /*private_topic=*/std::string(), kFakeInstanceIdToken,
      net::HTTP_UNAUTHORIZED);

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      ids, kFakeInstanceIdToken);
  // This should have resulted in a request for an access token. Return one
  // (which is considered invalid, e.g. already expired).
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "invalid_access_token", base::Time::Now());

  // Now the subscription requests should be scheduled.
  ASSERT_FALSE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // Wait for the subscription requests to happen.
  base::RunLoop().RunUntilIdle();

  // Since the subscriptions failed, the requests should still be pending.
  ASSERT_FALSE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // A new access token should have been requested. (Note: We'd really want to
  // check that the previous token got invalidated before a new one was
  // requested, but the identity test code doesn't expose that.)
  // Serving a new access token will trigger another subscription attempt; let
  // this one succeed.
  AddCorrectSubscriptionResponce();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "valid_access_token", base::Time::Max());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                   .empty());
  EXPECT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());
}

TEST_F(PerUserTopicSubscriptionManagerTest,
       ShouldInvalidateAccessTokenOnlyOnce) {
  // For this test, we need to manually control when access tokens are returned.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);

  auto ids = GetSequenceOfTopics(kInvalidationObjectIdsCount);

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  ASSERT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  // The first subscription attempt will fail with an "unauthorized" error.
  AddCorrectSubscriptionResponce(
      /*private_topic=*/std::string(), kFakeInstanceIdToken,
      net::HTTP_UNAUTHORIZED);

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      ids, kFakeInstanceIdToken);
  // This should have resulted in a request for an access token. Return one
  // (which is considered invalid, e.g. already expired).
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "invalid_access_token", base::Time::Now());

  // Now the subscription requests should be scheduled.
  ASSERT_FALSE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // Wait for the subscription requests to happen.
  base::RunLoop().RunUntilIdle();

  // Since the subscriptions failed, the requests should still be pending.
  ASSERT_FALSE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // A new access token should have been requested. (Note: We'd really want to
  // check that the previous token got invalidated before a new one was
  // requested, but the identity test code doesn't expose that.)
  // Serving a new access token will trigger another subscription attempt, but
  // it'll fail again with the same error.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "invalid_access_token_2", base::Time::Max());
  base::RunLoop().RunUntilIdle();

  // On the second auth failure, we should have given up - no new access token
  // request should have happened, and all the pending subscriptions should have
  // been dropped, even though still no topics are subscribed.
  EXPECT_FALSE(identity_test_env()->IsAccessTokenRequestPending());
  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());
  EXPECT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());
}

TEST_F(PerUserTopicSubscriptionManagerTest, ShouldRepeatRequestsOnForbidden) {
  auto ids = GetSequenceOfTopics(kInvalidationObjectIdsCount);

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  ASSERT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  AddCorrectSubscriptionResponce(
      /* private_topic */ std::string(), kFakeInstanceIdToken,
      net::HTTP_FORBIDDEN);

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());
  EXPECT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());
}

TEST_F(PerUserTopicSubscriptionManagerTest,
       ShouldDisableIdsAndDeleteFromPrefs) {
  auto ids = GetSequenceOfTopics(kInvalidationObjectIdsCount);

  AddCorrectSubscriptionResponce();

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(TopicSetFromTopics(ids),
            per_user_topic_subscription_manager->GetSubscribedTopicsForTest());

  // Disable some ids.
  auto disabled_ids = GetSequenceOfTopics(3);
  auto enabled_ids =
      GetSequenceOfTopicsStartingAt(3, kInvalidationObjectIdsCount - 3);
  for (const auto& id : disabled_ids)
    AddCorrectUnSubscriptionResponceForTopic(id.first);

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      enabled_ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();

  // ids were disabled, check that they're not in the prefs.
  for (const auto& id : disabled_ids) {
    const base::Value* topics = GetSubscribedTopics();
    const base::Value* private_topic_value = topics->FindKey(id.first);
    ASSERT_EQ(private_topic_value, nullptr);
  }

  // Check that enable ids are still in the prefs.
  for (const auto& id : enabled_ids) {
    const base::Value* topics = GetSubscribedTopics();
    const base::Value* private_topic_value =
        topics->FindKeyOfType(id.first, base::Value::Type::STRING);
    ASSERT_NE(private_topic_value, nullptr);
  }
}

TEST_F(PerUserTopicSubscriptionManagerTest,
       ShouldDropSavedTopicsOnTokenChange) {
  auto ids = GetSequenceOfTopics(kInvalidationObjectIdsCount);

  auto per_user_topic_subscription_manager = BuildRegistrationManager();

  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  AddCorrectSubscriptionResponce("old-token-topic");

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(TopicSetFromTopics(ids),
            per_user_topic_subscription_manager->GetSubscribedTopicsForTest());

  for (const auto& id : ids) {
    const base::Value* topics = GetSubscribedTopics();
    const base::Value* private_topic_value =
        topics->FindKeyOfType(id.first, base::Value::Type::STRING);
    ASSERT_NE(private_topic_value, nullptr);
    std::string private_topic;
    private_topic_value->GetAsString(&private_topic);
    EXPECT_EQ(private_topic, "old-token-topic");
  }

  EXPECT_EQ(kFakeInstanceIdToken,
            *pref_service()
                 ->GetDictionary(kActiveRegistrationTokens)
                 ->FindStringKey(kProjectId));

  std::string token = "new-fake-token";
  AddCorrectSubscriptionResponce("new-token-topic", token);

  per_user_topic_subscription_manager->UpdateSubscribedTopics(ids, token);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(token, *pref_service()
                        ->GetDictionary(kActiveRegistrationTokens)
                        ->FindStringKey(kProjectId));
  EXPECT_EQ(TopicSetFromTopics(ids),
            per_user_topic_subscription_manager->GetSubscribedTopicsForTest());

  for (const auto& id : ids) {
    const base::Value* topics = GetSubscribedTopics();
    const base::Value* private_topic_value =
        topics->FindKeyOfType(id.first, base::Value::Type::STRING);
    ASSERT_NE(private_topic_value, nullptr);
    std::string private_topic;
    private_topic_value->GetAsString(&private_topic);
    EXPECT_EQ(private_topic, "new-token-topic");
  }
}

TEST_F(PerUserTopicSubscriptionManagerTest,
       ShouldDeletTopicsFromPrefsWhenRequestFails) {
  auto ids = GetSequenceOfTopics(kInvalidationObjectIdsCount);

  AddCorrectSubscriptionResponce();

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(TopicSetFromTopics(ids),
            per_user_topic_subscription_manager->GetSubscribedTopicsForTest());

  // Disable some ids.
  auto disabled_ids = GetSequenceOfTopics(3);
  auto enabled_ids =
      GetSequenceOfTopicsStartingAt(3, kInvalidationObjectIdsCount - 3);
  // Without configuring the responce, the request will not happen.
  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      enabled_ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();

  // Ids should still be removed from prefs.
  for (const auto& id : disabled_ids) {
    const base::Value* topics = GetSubscribedTopics();
    const base::Value* private_topic_value = topics->FindKey(id.first);
    ASSERT_EQ(private_topic_value, nullptr);
  }

  // Check that enable ids are still in the prefs.
  for (const auto& id : enabled_ids) {
    const base::Value* topics = GetSubscribedTopics();
    const base::Value* private_topic_value =
        topics->FindKeyOfType(id.first, base::Value::Type::STRING);
    ASSERT_NE(private_topic_value, nullptr);
  }
}

TEST_F(
    PerUserTopicSubscriptionManagerTest,
    ShouldNotChangeStatusToDisabledWhenTopicsRegistrationFailedFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      invalidation::switches::kFCMInvalidationsConservativeEnabling);

  auto ids = GetSequenceOfTopics(kInvalidationObjectIdsCount);

  AddCorrectSubscriptionResponce();

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  ASSERT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(TopicSetFromTopics(ids),
            per_user_topic_subscription_manager->GetSubscribedTopicsForTest());
  EXPECT_EQ(observed_state(), SubscriptionChannelState::ENABLED);

  // Disable some ids.
  auto disabled_ids = GetSequenceOfTopics(3);
  auto enabled_ids =
      GetSequenceOfTopicsStartingAt(3, kInvalidationObjectIdsCount - 3);
  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      enabled_ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();

  // Clear previously configured correct response. So next requests will fail.
  url_loader_factory()->ClearResponses();
  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      ids, kFakeInstanceIdToken);
  url_loader_factory()->AddResponse(
      FullSubscriptionUrl(kFakeInstanceIdToken).spec(),
      std::string() /* content */, net::HTTP_NOT_FOUND);

  EXPECT_EQ(observed_state(), SubscriptionChannelState::ENABLED);
}

TEST_F(PerUserTopicSubscriptionManagerTest,
       ShouldChangeStatusToDisabledWhenTopicsRegistrationFailed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      invalidation::switches::kFCMInvalidationsConservativeEnabling);
  auto ids = GetSequenceOfTopics(kInvalidationObjectIdsCount);

  AddCorrectSubscriptionResponce();

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  ASSERT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(TopicSetFromTopics(ids),
            per_user_topic_subscription_manager->GetSubscribedTopicsForTest());
  EXPECT_EQ(observed_state(), SubscriptionChannelState::ENABLED);

  // Disable some ids.
  auto disabled_ids = GetSequenceOfTopics(3);
  auto enabled_ids =
      GetSequenceOfTopicsStartingAt(3, kInvalidationObjectIdsCount - 3);
  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      enabled_ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();

  // Clear previously configured correct response. So next requests will fail.
  url_loader_factory()->ClearResponses();
  url_loader_factory()->AddResponse(
      FullSubscriptionUrl(kFakeInstanceIdToken).spec(),
      std::string() /* content */, net::HTTP_NOT_FOUND);

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observed_state(), SubscriptionChannelState::SUBSCRIPTION_FAILURE);

  // Configure correct response and retry.
  AddCorrectSubscriptionResponce();
  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observed_state(), SubscriptionChannelState::ENABLED);
}

TEST_F(PerUserTopicSubscriptionManagerTest, ShouldRecordTokenStateHistogram) {
  const char kTokenStateHistogram[] =
      "FCMInvalidations.TokenStateOnRegistrationRequest2";
  enum class TokenStateOnSubscriptionRequest {
    kTokenWasEmpty = 0,
    kTokenUnchanged = 1,
    kTokenChanged = 2,
    kTokenCleared = 3,
  };

  const Topics topics = GetSequenceOfTopics(kInvalidationObjectIdsCount);
  auto per_user_topic_subscription_manager = BuildRegistrationManager();

  // Subscribe to some topics (and provide an InstanceID token).
  {
    base::HistogramTester histograms;

    AddCorrectSubscriptionResponce(/*private_topic=*/"", "original_token");
    per_user_topic_subscription_manager->UpdateSubscribedTopics(
        topics, "original_token");
    base::RunLoop().RunUntilIdle();

    histograms.ExpectUniqueSample(
        kTokenStateHistogram, TokenStateOnSubscriptionRequest::kTokenWasEmpty,
        1);
  }

  ASSERT_EQ(TopicSetFromTopics(topics),
            per_user_topic_subscription_manager->GetSubscribedTopicsForTest());
  ASSERT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // Call UpdateSubscribedTopics again with the same token.
  {
    base::HistogramTester histograms;

    per_user_topic_subscription_manager->UpdateSubscribedTopics(
        topics, "original_token");
    base::RunLoop().RunUntilIdle();

    histograms.ExpectUniqueSample(
        kTokenStateHistogram, TokenStateOnSubscriptionRequest::kTokenUnchanged,
        1);
  }

  // Topic subscriptions are unchanged.
  ASSERT_EQ(TopicSetFromTopics(topics),
            per_user_topic_subscription_manager->GetSubscribedTopicsForTest());
  ASSERT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // Call UpdateSubscribedTopics again, but now with a different token.
  {
    base::HistogramTester histograms;

    AddCorrectSubscriptionResponce(/*private_topic=*/"", "different_token");
    per_user_topic_subscription_manager->UpdateSubscribedTopics(
        topics, "different_token");
    base::RunLoop().RunUntilIdle();

    histograms.ExpectUniqueSample(
        kTokenStateHistogram, TokenStateOnSubscriptionRequest::kTokenChanged,
        1);
  }

  // Topic subscriptions are still the same (all topics were re-subscribed).
  ASSERT_EQ(TopicSetFromTopics(topics),
            per_user_topic_subscription_manager->GetSubscribedTopicsForTest());
  ASSERT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // Call ClearInstanceIDToken.
  {
    base::HistogramTester histograms;

    per_user_topic_subscription_manager->ClearInstanceIDToken();
    base::RunLoop().RunUntilIdle();

    histograms.ExpectUniqueSample(
        kTokenStateHistogram, TokenStateOnSubscriptionRequest::kTokenCleared,
        1);
  }

  // Topic subscriptions are gone now.
  ASSERT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());
  ASSERT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());
}

}  // namespace syncer
