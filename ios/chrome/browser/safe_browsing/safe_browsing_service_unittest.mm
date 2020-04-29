// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/safe_browsing/safe_browsing_service_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/safe_browsing/core/db/metadata.pb.h"
#include "components/safe_browsing/core/db/util.h"
#include "components/safe_browsing/core/db/v4_database.h"
#include "components/safe_browsing/core/db/v4_get_hash_protocol_manager.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/db/v4_test_util.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kSafePage[] = "http://example.test/safe.html";
const char kMalwarePage[] = "http://example.test/malware.html";

class TestUrlCheckerClient {
 public:
  TestUrlCheckerClient(SafeBrowsingService* safe_browsing_service)
      : safe_browsing_service_(safe_browsing_service) {}

  ~TestUrlCheckerClient() = default;

  TestUrlCheckerClient(const TestUrlCheckerClient&) = delete;
  TestUrlCheckerClient& operator=(const TestUrlCheckerClient&) = delete;

  bool url_is_unsafe() const { return url_is_unsafe_; }

  void CheckUrl(const GURL& url) {
    result_pending_ = true;
    url_checker_ = safe_browsing_service_->CreateUrlChecker(
        safe_browsing::ResourceType::kMainFrame, &web_state_);
    base::PostTask(FROM_HERE, {web::WebThread::IO},
                   base::BindOnce(&TestUrlCheckerClient::CheckUrlOnIOThread,
                                  base::Unretained(this), url));
  }

  bool result_pending() const { return result_pending_; }

  void WaitForResult() {
    while (result_pending()) {
      base::RunLoop().RunUntilIdle();
    }
  }

 private:
  void CheckUrlOnIOThread(const GURL& url) {
    url_checker_->CheckUrl(
        url, "GET",
        base::BindOnce(&TestUrlCheckerClient::OnCheckUrlResult,
                       base::Unretained(this)));
  }

  void OnCheckUrlResult(
      safe_browsing::SafeBrowsingUrlCheckerImpl::NativeUrlCheckNotifier*
          slow_check_notifier,
      bool proceed,
      bool showed_interstitial) {
    if (slow_check_notifier) {
      *slow_check_notifier =
          base::BindOnce(&TestUrlCheckerClient::OnCheckUrlResult,
                         base::Unretained(this), nullptr);
      return;
    }
    url_is_unsafe_ = !proceed;
    result_pending_ = false;
    url_checker_.reset();
  }

  void CheckDone() { result_pending_ = false; }

  bool result_pending_ = false;
  bool url_is_unsafe_ = false;
  SafeBrowsingService* safe_browsing_service_;
  web::TestWebState web_state_;
  std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> url_checker_;
};

}  // namespace

class SafeBrowsingServiceTest : public PlatformTest {
 public:
  SafeBrowsingServiceTest()
      : task_environment_(web::WebTaskEnvironment::IO_MAINLOOP) {
    safe_browsing::RegisterProfilePrefs(local_state_.registry());

    store_factory_ = new safe_browsing::TestV4StoreFactory();
    safe_browsing::V4Database::RegisterStoreFactoryForTest(
        base::WrapUnique(store_factory_));

    v4_db_factory_ = new safe_browsing::TestV4DatabaseFactory();
    safe_browsing::V4Database::RegisterDatabaseFactoryForTest(
        base::WrapUnique(v4_db_factory_));

    v4_get_hash_factory_ =
        new safe_browsing::TestV4GetHashProtocolManagerFactory();
    safe_browsing::V4GetHashProtocolManager::RegisterFactory(
        base::WrapUnique(v4_get_hash_factory_));

    safe_browsing_service_ = base::MakeRefCounted<SafeBrowsingServiceImpl>();

    CHECK(temp_dir_.CreateUniqueTempDir());
    safe_browsing_service_->Initialize(&local_state_, temp_dir_.GetPath());
    base::RunLoop().RunUntilIdle();
  }

  ~SafeBrowsingServiceTest() override {
    safe_browsing_service_->ShutDown();

    safe_browsing::V4GetHashProtocolManager::RegisterFactory(nullptr);
    safe_browsing::V4Database::RegisterDatabaseFactoryForTest(nullptr);
    safe_browsing::V4Database::RegisterStoreFactoryForTest(nullptr);
  }

  void MarkUrlAsMalware(const GURL& bad_url) {
    base::PostTask(
        FROM_HERE, {web::WebThread::IO},
        base::BindOnce(&SafeBrowsingServiceTest::MarkUrlAsMalwareOnIOThread,
                       base::Unretained(this), bad_url));
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  scoped_refptr<SafeBrowsingService> safe_browsing_service_;

 private:
  void MarkUrlAsMalwareOnIOThread(const GURL& bad_url) {
    safe_browsing::FullHashInfo full_hash_info =
        safe_browsing::GetFullHashInfoWithMetadata(
            bad_url, safe_browsing::GetUrlMalwareId(),
            safe_browsing::ThreatMetadata());
    v4_db_factory_->MarkPrefixAsBad(safe_browsing::GetUrlMalwareId(),
                                    full_hash_info.full_hash);
    v4_get_hash_factory_->AddToFullHashCache(full_hash_info);
  }

  base::ScopedTempDir temp_dir_;

  // Owned by V4Database.
  safe_browsing::TestV4DatabaseFactory* v4_db_factory_;
  // Owned by V4GetHashProtocolManager.
  safe_browsing::TestV4GetHashProtocolManagerFactory* v4_get_hash_factory_;
  // Owned by V4Database.
  safe_browsing::TestV4StoreFactory* store_factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceTest);
};

TEST_F(SafeBrowsingServiceTest, SafeAndUnsafePages) {
  // Verify that queries to the Safe Browsing database owned by
  // SafeBrowsingService receive responses.
  TestUrlCheckerClient client(safe_browsing_service_.get());
  GURL safe_url = GURL(kSafePage);
  client.CheckUrl(safe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());

  GURL unsafe_url = GURL(kMalwarePage);
  MarkUrlAsMalware(unsafe_url);
  client.CheckUrl(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_TRUE(client.url_is_unsafe());

  // Disable Safe Browsing, and ensure that unsafe URLs are no longer flagged.
  local_state_.SetUserPref(prefs::kSafeBrowsingEnabled,
                           std::make_unique<base::Value>(false));
  client.CheckUrl(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());
}
