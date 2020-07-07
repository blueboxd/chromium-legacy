// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/access_context_audit_service.h"

#include "base/files/scoped_temp_dir.h"
#include "base/i18n/time_formatting.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/browsing_data/access_context_audit_database.h"
#include "chrome/browser/browsing_data/access_context_audit_service_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/content/local_shared_objects_container.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Checks that a record exists in |records| that matches both |cookie| and
// |top_frame_origin|.
void CheckContainsCookieRecord(
    net::CanonicalCookie* cookie,
    url::Origin top_frame_origin,
    const std::vector<AccessContextAuditDatabase::AccessRecord>& records) {
  EXPECT_NE(
      std::find_if(
          records.begin(), records.end(),
          [=](const AccessContextAuditDatabase::AccessRecord& record) {
            return record.type ==
                       AccessContextAuditDatabase::StorageAPIType::kCookie &&
                   record.top_frame_origin == top_frame_origin &&
                   record.name == cookie->Name() &&
                   record.domain == cookie->Domain() &&
                   record.path == cookie->Path() &&
                   record.last_access_time == cookie->LastAccessDate() &&
                   record.is_persistent == cookie->IsPersistent();
          }),
      records.end());
}

// Checks that info in |record| matches storage API access defined by
// |storage_origin|, |type| and |top_frame_origin|
void CheckContainsStorageAPIRecord(
    url::Origin storage_origin,
    AccessContextAuditDatabase::StorageAPIType type,
    url::Origin top_frame_origin,
    const std::vector<AccessContextAuditDatabase::AccessRecord>& records) {
  EXPECT_NE(
      std::find_if(records.begin(), records.end(),
                   [=](const AccessContextAuditDatabase::AccessRecord& record) {
                     return record.type == type &&
                            record.origin == storage_origin &&
                            record.top_frame_origin == top_frame_origin;
                   }),
      records.end());
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
        new AccessContextAuditService(static_cast<Profile*>(context)));
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
  std::string kTestNonPersistentCookieName = "test-non-persistent";
  base::Time initial_cookie_access_time = base::Time::Now();

  auto test_cookie = net::CanonicalCookie::Create(
      kTestCookieURL, kTestCookieName + "=1; max-age=3600",
      initial_cookie_access_time, base::nullopt /* server_time */);
  auto test_non_persistent_cookie = net::CanonicalCookie::Create(
      kTestCookieURL, kTestNonPersistentCookieName + "=1",
      initial_cookie_access_time, base::nullopt /* server_time */);
  // Record access to these cookies against a URL.
  url::Origin kTopFrameOrigin = url::Origin::Create(GURL("https://test.com"));
  service()->RecordCookieAccess({*test_cookie, *test_non_persistent_cookie},
                                kTopFrameOrigin);

  // Ensure that the record of these accesses is correctly returned.
  service()->GetAllAccessRecords(
      base::BindOnce(&AccessContextAuditServiceTest::AccessRecordCallback,
                     base::Unretained(this)));
  browser_task_environment_.RunUntilIdle();

  EXPECT_EQ(2u, GetReturnedRecords().size());
  CheckContainsCookieRecord(test_cookie.get(), kTopFrameOrigin,
                            GetReturnedRecords());
  CheckContainsCookieRecord(test_non_persistent_cookie.get(), kTopFrameOrigin,
                            GetReturnedRecords());

  // Check that informing the service of non-deletion changes to the cookies
  // via the CookieChangeInterface is a no-op.
  service()->OnCookieChange(
      net::CookieChangeInfo(*test_cookie, net::CookieAccessSemantics::UNKNOWN,
                            net::CookieChangeCause::OVERWRITE));
  service()->OnCookieChange(net::CookieChangeInfo(
      *test_non_persistent_cookie, net::CookieAccessSemantics::UNKNOWN,
      net::CookieChangeCause::OVERWRITE));

  service()->GetAllAccessRecords(
      base::BindOnce(&AccessContextAuditServiceTest::AccessRecordCallback,
                     base::Unretained(this)));
  browser_task_environment_.RunUntilIdle();

  EXPECT_EQ(2u, GetReturnedRecords().size());
  CheckContainsCookieRecord(test_cookie.get(), kTopFrameOrigin,
                            GetReturnedRecords());
  CheckContainsCookieRecord(test_non_persistent_cookie.get(), kTopFrameOrigin,
                            GetReturnedRecords());

  // Check that a repeated access correctly updates associated timestamp.
  base::Time repeat_cookie_access_time =
      initial_cookie_access_time + base::TimeDelta::FromHours(2);
  test_cookie->SetLastAccessDate(repeat_cookie_access_time);
  test_non_persistent_cookie->SetLastAccessDate(repeat_cookie_access_time);
  service()->RecordCookieAccess({*test_cookie, *test_non_persistent_cookie},
                                kTopFrameOrigin);

  ClearReturnedRecords();
  service()->GetAllAccessRecords(
      base::BindOnce(&AccessContextAuditServiceTest::AccessRecordCallback,
                     base::Unretained(this)));
  browser_task_environment_.RunUntilIdle();

  EXPECT_EQ(2u, GetReturnedRecords().size());
  CheckContainsCookieRecord(test_cookie.get(), kTopFrameOrigin,
                            GetReturnedRecords());
  CheckContainsCookieRecord(test_non_persistent_cookie.get(), kTopFrameOrigin,
                            GetReturnedRecords());

  // Inform the service the cookies have been deleted and check they are no
  // longer returned.
  service()->OnCookieChange(
      net::CookieChangeInfo(*test_cookie, net::CookieAccessSemantics::UNKNOWN,
                            net::CookieChangeCause::EXPLICIT));
  service()->OnCookieChange(net::CookieChangeInfo(
      *test_non_persistent_cookie, net::CookieAccessSemantics::UNKNOWN,
      net::CookieChangeCause::EXPLICIT));
  ClearReturnedRecords();
  service()->GetAllAccessRecords(
      base::BindOnce(&AccessContextAuditServiceTest::AccessRecordCallback,
                     base::Unretained(this)));
  browser_task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, GetReturnedRecords().size());
}

TEST_F(AccessContextAuditServiceTest, ExpiredCookies) {
  // Check that no accesses are recorded for cookies which have already expired.
  const GURL kTestURL("https://test.com");
  auto test_cookie_expired = net::CanonicalCookie::Create(
      kTestURL, "test_1=1; expires=Thu, 01 Jan 1970 00:00:00 GMT",
      base::Time::Now(), base::nullopt /* server_time */);

  service()->RecordCookieAccess({*test_cookie_expired},
                                url::Origin::Create(kTestURL));

  service()->GetAllAccessRecords(
      base::BindOnce(&AccessContextAuditServiceTest::AccessRecordCallback,
                     base::Unretained(this)));
  browser_task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, GetReturnedRecords().size());
}

TEST_F(AccessContextAuditServiceTest, SessionOnlyRecords) {
  // Check that data for cookie domains and storage origins are cleared on
  // service shutdown when the associated content settings indicate they should.
  const GURL kTestPersistentURL("https://persistent.com");
  const GURL kTestSessionOnlyExplicitURL("https://explicit-session-only.com");
  const GURL kTestSessionOnlyContentSettingURL("https://content-setting.com");
  url::Origin kTopFrameOrigin = url::Origin::Create(GURL("https://test.com"));
  std::string kTestCookieName = "test";
  const auto kTestStorageType =
      AccessContextAuditDatabase::StorageAPIType::kWebDatabase;

  // Create a cookie that will persist after shutdown.
  auto test_cookie_persistent = net::CanonicalCookie::Create(
      kTestPersistentURL, kTestCookieName + "=1; max-age=3600",
      base::Time::Now(), base::nullopt /* server_time */);

  // Create a cookie that will persist (be cleared on next startup) because it
  // is explicitly session only.
  auto test_cookie_session_only_explicit = net::CanonicalCookie::Create(
      kTestSessionOnlyExplicitURL, kTestCookieName + "=1", base::Time::Now(),
      base::nullopt /* server_time */);

  // Create a cookie that will be cleared because the content setting associated
  // with the cookie domain is set to session only.
  auto test_cookie_session_only_content_setting = net::CanonicalCookie::Create(
      kTestSessionOnlyContentSettingURL, kTestCookieName + "=1; max-age=3600",
      base::Time::Now(), base::nullopt /* server_time */);

  service()->RecordCookieAccess(
      {*test_cookie_persistent, *test_cookie_session_only_explicit,
       *test_cookie_session_only_content_setting},
      kTopFrameOrigin);

  // Record storage APIs for both persistent and content setting based session
  // only URLs.
  service()->RecordStorageAPIAccess(url::Origin::Create(kTestPersistentURL),
                                    kTestStorageType, kTopFrameOrigin);
  service()->RecordStorageAPIAccess(
      url::Origin::Create(kTestSessionOnlyContentSettingURL), kTestStorageType,
      kTopFrameOrigin);

  // Ensure all records have been initially recorded.
  service()->GetAllAccessRecords(
      base::BindOnce(&AccessContextAuditServiceTest::AccessRecordCallback,
                     base::Unretained(this)));
  browser_task_environment_.RunUntilIdle();
  EXPECT_EQ(5u, GetReturnedRecords().size());

  // Apply Session Only exception.
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(
          kTestSessionOnlyContentSettingURL, GURL(),
          ContentSettingsType::COOKIES, std::string(),
          ContentSetting::CONTENT_SETTING_SESSION_ONLY);

  // Instruct service to clear session only records and check that they are
  // correctly removed.
  service()->ClearSessionOnlyRecords();

  ClearReturnedRecords();
  service()->GetAllAccessRecords(
      base::BindOnce(&AccessContextAuditServiceTest::AccessRecordCallback,
                     base::Unretained(this)));
  browser_task_environment_.RunUntilIdle();

  ASSERT_EQ(3u, GetReturnedRecords().size());
  CheckContainsCookieRecord(test_cookie_persistent.get(), kTopFrameOrigin,
                            GetReturnedRecords());
  CheckContainsCookieRecord(test_cookie_session_only_explicit.get(),
                            kTopFrameOrigin, GetReturnedRecords());
  CheckContainsStorageAPIRecord(url::Origin::Create(GURL(kTestPersistentURL)),
                                kTestStorageType, kTopFrameOrigin,
                                GetReturnedRecords());
}
