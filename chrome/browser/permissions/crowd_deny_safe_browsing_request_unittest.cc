// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/crowd_deny_safe_browsing_request.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/safe_browsing/db/test_database_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr char kTestOriginFoo[] = "https://foo.com";
constexpr char kTestOriginBar[] = "https://bar.com";

class FakeSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  FakeSafeBrowsingDatabaseManager() = default;

  void SetSimulatedMetadataForUrl(
      const GURL& url,
      const safe_browsing::ThreatMetadata& metadata) {
    url_to_simulated_threat_metadata_.emplace(url, metadata);
  }

  void RemoveAllBlacklistedUrls() { url_to_simulated_threat_metadata_.clear(); }

  void set_simulate_timeout(bool simulate_timeout) {
    simulate_timeout_ = simulate_timeout;
  }

  void set_simulate_synchronous_result(bool simulate_synchronous_result) {
    simulate_synchronous_result_ = simulate_synchronous_result;
  }

 protected:
  ~FakeSafeBrowsingDatabaseManager() override {
    EXPECT_THAT(pending_clients_, testing::IsEmpty());
  }

  safe_browsing::ThreatMetadata GetSimulatedMetadataOrSafe(const GURL& url) {
    auto it = url_to_simulated_threat_metadata_.find(url);
    return it != url_to_simulated_threat_metadata_.end()
               ? it->second
               : safe_browsing::ThreatMetadata();
  }

  // safe_browsing::TestSafeBrowsingDatabaseManager:
  bool CheckApiBlacklistUrl(const GURL& url, Client* client) override {
    if (simulate_synchronous_result_)
      return true;

    if (simulate_timeout_) {
      EXPECT_THAT(pending_clients_, testing::Not(testing::Contains(client)));
      pending_clients_.insert(client);
    } else {
      auto result = GetSimulatedMetadataOrSafe(url);
      client->OnCheckApiBlacklistUrlResult(url, std::move(result));
    }
    return false;
  }

  bool CancelApiCheck(Client* client) override {
    EXPECT_THAT(pending_clients_, testing::Contains(client));
    pending_clients_.erase(client);
    return true;
  }

  bool IsSupported() const override { return true; }
  bool ChecksAreAlwaysAsync() const override { return false; }

 private:
  void OnCheckUrlForSubresourceFilterComplete(Client* client, const GURL& url);

  std::set<Client*> pending_clients_;
  std::map<GURL, safe_browsing::ThreatMetadata>
      url_to_simulated_threat_metadata_;
  bool simulate_timeout_ = false;
  bool simulate_synchronous_result_ = false;

  base::WeakPtrFactory<FakeSafeBrowsingDatabaseManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingDatabaseManager);
};

}  // namespace

class CrowdDenySafeBrowsingRequestTest : public testing::Test {
 public:
  using Verdict = CrowdDenySafeBrowsingRequest::Verdict;

  CrowdDenySafeBrowsingRequestTest()
      : fake_database_manager_(
            base::MakeRefCounted<FakeSafeBrowsingDatabaseManager>()) {}
  ~CrowdDenySafeBrowsingRequestTest() override = default;

 protected:
  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

  FakeSafeBrowsingDatabaseManager* fake_database_manager() {
    return fake_database_manager_.get();
  }

  const base::Clock* test_clock() { return task_environment_.GetMockClock(); }

  void StartRequestForOriginAndExpectVerdict(const url::Origin& origin,
                                             Verdict expected_verdict) {
    base::HistogramTester histogram_tester;
    base::MockOnceCallback<void(Verdict)> mock_callback_receiver;
    CrowdDenySafeBrowsingRequest request(fake_database_manager(), test_clock(),
                                         origin, mock_callback_receiver.Get());
    EXPECT_CALL(mock_callback_receiver, Run(expected_verdict));
    task_environment()->RunUntilIdle();

    histogram_tester.ExpectTotalCount(
        "Permissions.CrowdDeny.SafeBrowsing.RequestDuration", 1);
    histogram_tester.ExpectUniqueSample(
        "Permissions.CrowdDeny.SafeBrowsing.Verdict",
        static_cast<int>(expected_verdict), 1);
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<FakeSafeBrowsingDatabaseManager> fake_database_manager_;

  DISALLOW_COPY_AND_ASSIGN(CrowdDenySafeBrowsingRequestTest);
};

TEST_F(CrowdDenySafeBrowsingRequestTest, Acceptable_SynchronousCompletion) {
  fake_database_manager()->set_simulate_synchronous_result(true);
  StartRequestForOriginAndExpectVerdict(
      url::Origin::Create(GURL(kTestOriginFoo)), Verdict::kAcceptable);
}

TEST_F(CrowdDenySafeBrowsingRequestTest, Acceptable_UnavailableMetaData) {
  StartRequestForOriginAndExpectVerdict(
      url::Origin::Create(GURL(kTestOriginFoo)), Verdict::kAcceptable);
}

TEST_F(CrowdDenySafeBrowsingRequestTest, Acceptable_UnknownAPIName) {
  const GURL kTestURL(kTestOriginFoo);

  safe_browsing::ThreatMetadata test_metadata;
  test_metadata.api_permissions.emplace("");
  test_metadata.api_permissions.emplace("Stuff");
  test_metadata.api_permissions.emplace("NOTIFICATION");   // Singular.
  test_metadata.api_permissions.emplace("notifications");  // Lowercase.
  fake_database_manager()->SetSimulatedMetadataForUrl(kTestURL, test_metadata);

  StartRequestForOriginAndExpectVerdict(url::Origin::Create(kTestURL),
                                        Verdict::kAcceptable);
}

TEST_F(CrowdDenySafeBrowsingRequestTest, Spammy) {
  const GURL kTestURL(kTestOriginFoo);

  safe_browsing::ThreatMetadata test_metadata;
  test_metadata.api_permissions.emplace("BANANAS");
  test_metadata.api_permissions.emplace("NOTIFICATIONS");
  test_metadata.api_permissions.emplace("ORANGES");
  fake_database_manager()->SetSimulatedMetadataForUrl(kTestURL, test_metadata);

  StartRequestForOriginAndExpectVerdict(
      url::Origin::Create(kTestURL),
      Verdict::kKnownToShowUnsolicitedNotificationPermissionRequests);
  StartRequestForOriginAndExpectVerdict(
      url::Origin::Create(GURL(kTestOriginBar)), Verdict::kAcceptable);
}

TEST_F(CrowdDenySafeBrowsingRequestTest, Timeout) {
  fake_database_manager()->set_simulate_timeout(true);

  base::HistogramTester histogram_tester;
  base::MockOnceCallback<void(Verdict)> mock_callback_receiver;
  CrowdDenySafeBrowsingRequest request(
      fake_database_manager(), test_clock(),
      url::Origin::Create(GURL(kTestOriginFoo)), mock_callback_receiver.Get());

  // Verify the request doesn't time out unreasonably fast.
  EXPECT_CALL(mock_callback_receiver, Run(testing::_)).Times(0);
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(100));
  testing::Mock::VerifyAndClearExpectations(&mock_callback_receiver);

  // But that it eventually does.
  EXPECT_CALL(mock_callback_receiver, Run(Verdict::kAcceptable));
  task_environment()->FastForwardUntilNoTasksRemain();

  histogram_tester.ExpectUniqueSample(
      "Permissions.CrowdDeny.SafeBrowsing.RequestDuration", 2000, 1);
  histogram_tester.ExpectUniqueSample(
      "Permissions.CrowdDeny.SafeBrowsing.Verdict",
      static_cast<int>(Verdict::kAcceptable), 1);
}

TEST_F(CrowdDenySafeBrowsingRequestTest, AbandonedImmediately) {
  fake_database_manager()->set_simulate_synchronous_result(true);

  base::HistogramTester histogram_tester;
  base::MockOnceCallback<void(Verdict)> mock_callback_receiver;
  EXPECT_CALL(mock_callback_receiver, Run(testing::_)).Times(0);

  {
    CrowdDenySafeBrowsingRequest request(
        fake_database_manager(), test_clock(),
        url::Origin::Create(GURL(kTestOriginFoo)),
        mock_callback_receiver.Get());
  }

  task_environment()->RunUntilIdle();
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("Permissions."),
              testing::IsEmpty());
}

TEST_F(CrowdDenySafeBrowsingRequestTest, AbandonedWhileCheckPending) {
  fake_database_manager()->set_simulate_timeout(true);

  base::HistogramTester histogram_tester;
  base::MockOnceCallback<void(Verdict)> mock_callback_receiver;
  EXPECT_CALL(mock_callback_receiver, Run(testing::_)).Times(0);

  CrowdDenySafeBrowsingRequest request(
      fake_database_manager(), test_clock(),
      url::Origin::Create(GURL(kTestOriginFoo)), mock_callback_receiver.Get());

  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(100));
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("Permissions."),
              testing::IsEmpty());
}
