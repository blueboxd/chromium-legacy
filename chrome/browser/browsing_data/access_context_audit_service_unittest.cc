// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/access_context_audit_service.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/browsing_data/access_context_audit_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/content/local_shared_objects_container.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Checks that info in |record| matches both |cookie| and |top_frame_origin|.
void CheckCookieMatchesRecord(
    net::CanonicalCookie* cookie,
    GURL top_frame_origin,
    const AccessContextAuditDatabase::AccessRecord& record) {
  EXPECT_EQ(top_frame_origin.GetOrigin(), record.top_frame_origin);
  EXPECT_EQ(AccessContextAuditDatabase::StorageAPIType::kCookie, record.type);
  EXPECT_EQ(cookie->Name(), record.name);
  EXPECT_EQ(cookie->Domain(), record.domain);
  EXPECT_EQ(cookie->Path(), record.path);
}

}  // namespace

class TestCookieManager : public network::TestCookieManager {
 public:
  void AddGlobalChangeListener(
      mojo::PendingRemote<network::mojom::CookieChangeListener>
          notification_pointer) override {
    listener_registered_ = true;
  }

  bool ListenerRegistered() { return listener_registered_; }

 protected:
  bool listener_registered_ = false;
};

class AccessContextAuditServiceTest : public testing::Test {
 public:
  AccessContextAuditServiceTest() = default;

  std::unique_ptr<KeyedService> BuildTestContextAuditService(
      content::BrowserContext* context) {
    std::unique_ptr<AccessContextAuditService> service(
        new AccessContextAuditService());
    service->SetTaskRunnerForTesting(
        browser_task_environment_.GetMainThreadTaskRunner());
    service->Init(temp_directory_.GetPath(), cookie_manager());
    return service;
  }

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kClientStorageAccessContextAuditing}, {});

    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    task_runner_ = scoped_refptr<base::TestSimpleTaskRunner>(
        new base::TestSimpleTaskRunner);

    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        AccessContextAuditServiceFactory::GetInstance(),
        base::BindRepeating(
            &AccessContextAuditServiceTest::BuildTestContextAuditService,
            base::Unretained(this)));
    builder.SetPath(temp_directory_.GetPath());

    profile_ = builder.Build();
    browser_task_environment_.RunUntilIdle();
  }

  void AccessRecordCallback(
      std::vector<AccessContextAuditDatabase::AccessRecord> records) {
    records_ = records;
  }

  std::vector<AccessContextAuditDatabase::AccessRecord> GetReturnedRecords() {
    return records_;
  }
  void ClearReturnedRecords() { records_.clear(); }

  TestCookieManager* cookie_manager() { return &cookie_manager_; }
  TestingProfile* profile() { return profile_.get(); }
  AccessContextAuditService* service() {
    return AccessContextAuditServiceFactory::GetForProfile(profile());
  }

 protected:
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_directory_;
  TestCookieManager cookie_manager_;
  base::test::ScopedFeatureList feature_list_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::vector<AccessContextAuditDatabase::AccessRecord> records_;
};

TEST_F(AccessContextAuditServiceTest, RegisterDeletionObservers) {
  // Check that the service correctly registers observers for deletion.
  EXPECT_TRUE(cookie_manager_.ListenerRegistered());
}

TEST_F(AccessContextAuditServiceTest, CookieRecords) {
  // Check that cookie access records are successfully stored and deleted.
  GURL kTestCookieURL("https://example.com");
  std::string kTestCookieName = "test";
  auto test_cookie = net::CanonicalCookie::Create(
      kTestCookieURL, kTestCookieName + "=1", base::Time::Now(),
      base::nullopt /* server_time */);
  content::CookieAccessDetails test_access_details(
      content::CookieAccessDetails::Type::kRead, kTestCookieURL, kTestCookieURL,
      {*test_cookie});

  browsing_data::LocalSharedObjectsContainer test_container(
      profile(), {}, base::NullCallback());
  test_container.cookies()->AddCookies(test_access_details);

  // Record access to this cookie against a URL.
  GURL kTopFrameURL("https://test.com");
  service()->RecordAccess(test_container, kTopFrameURL);

  // Ensure that the record of this access is correctly returned.
  service()->GetAllAccessRecords(
      base::BindOnce(&AccessContextAuditServiceTest::AccessRecordCallback,
                     base::Unretained(this)));
  browser_task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, GetReturnedRecords().size());
  CheckCookieMatchesRecord(test_cookie.get(), kTopFrameURL,
                           GetReturnedRecords()[0]);

  // Check that informing the service of access to the cookie is a no-op.
  service()->OnCookieChange(
      net::CookieChangeInfo(*test_cookie, net::CookieAccessSemantics::UNKNOWN,
                            net::CookieChangeCause::OVERWRITE));
  ClearReturnedRecords();
  service()->GetAllAccessRecords(
      base::BindOnce(&AccessContextAuditServiceTest::AccessRecordCallback,
                     base::Unretained(this)));
  browser_task_environment_.RunUntilIdle();

  EXPECT_EQ(1u, GetReturnedRecords().size());
  CheckCookieMatchesRecord(test_cookie.get(), kTopFrameURL,
                           GetReturnedRecords()[0]);

  // Inform the service the cookie has been deleted and check it is no longer
  // returned.
  service()->OnCookieChange(
      net::CookieChangeInfo(*test_cookie, net::CookieAccessSemantics::UNKNOWN,
                            net::CookieChangeCause::EXPLICIT));
  ClearReturnedRecords();
  service()->GetAllAccessRecords(
      base::BindOnce(&AccessContextAuditServiceTest::AccessRecordCallback,
                     base::Unretained(this)));
  browser_task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, GetReturnedRecords().size());
}
