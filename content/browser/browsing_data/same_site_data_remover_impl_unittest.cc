// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "content/browser/browsing_data/same_site_data_remover_impl.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_storage_partition.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::IsEmpty;
using testing::SizeIs;
using testing::UnorderedElementsAre;

namespace content {

struct StoragePartitionSameSiteRemovalData {
  uint32_t removal_mask = 0;
  uint32_t quota_storage_removal_mask = 0;
  StoragePartition::OriginMatcherFunction origin_matcher;
};

class SameSiteRemoverTestStoragePartition : public TestStoragePartition {
 public:
  SameSiteRemoverTestStoragePartition() {}
  ~SameSiteRemoverTestStoragePartition() override {}

  void ClearData(uint32_t removal_mask,
                 uint32_t quota_storage_removal_mask,
                 const OriginMatcherFunction& origin_matcher,
                 network::mojom::CookieDeletionFilterPtr cookie_deletion_filter,
                 bool perform_storage_cleanup,
                 const base::Time begin,
                 const base::Time end,
                 base::OnceClosure callback) override {
    storage_partition_removal_data_.removal_mask = removal_mask;
    storage_partition_removal_data_.quota_storage_removal_mask =
        quota_storage_removal_mask;
    storage_partition_removal_data_.origin_matcher = origin_matcher;
    std::move(callback).Run();
  }

  const StoragePartitionSameSiteRemovalData& GetStoragePartitionRemovalData() {
    return storage_partition_removal_data_;
  }

 private:
  StoragePartitionSameSiteRemovalData storage_partition_removal_data_;

  DISALLOW_COPY_AND_ASSIGN(SameSiteRemoverTestStoragePartition);
};

class SameSiteDataRemoverImplTest : public testing::Test {
 public:
  SameSiteDataRemoverImplTest()
      : browser_context_(std::make_unique<TestBrowserContext>()),
        same_site_remover_(
            std::make_unique<SameSiteDataRemoverImpl>(browser_context_.get())) {
  }

  void TearDown() override { browser_context_.reset(); }

  void CreateCookieForTest(
      const std::string& cookie_name,
      const std::string& cookie_domain,
      net::CookieSameSite same_site,
      net::CookieOptions::SameSiteCookieContext cookie_context) {
    network::mojom::CookieManager* cookie_manager = GetCookieManager();

    base::RunLoop run_loop;
    net::CookieOptions options;
    options.set_same_site_cookie_context(cookie_context);
    bool result_out;
    cookie_manager->SetCanonicalCookie(
        net::CanonicalCookie(cookie_name, "1", cookie_domain, "/", base::Time(),
                             base::Time(), base::Time(), false, false,
                             same_site, net::COOKIE_PRIORITY_LOW),
        "https", options,
        base::BindLambdaForTesting(
            [&](net::CanonicalCookie::CookieInclusionStatus result) {
              result_out =
                  (result ==
                   net::CanonicalCookie::CookieInclusionStatus::INCLUDE);
              run_loop.Quit();
            }));
    run_loop.Run();
    EXPECT_TRUE(result_out);
  }

  std::vector<net::CanonicalCookie> GetAllCookies(
      network::mojom::CookieManager* cookie_manager) {
    base::RunLoop run_loop;
    std::vector<net::CanonicalCookie> cookies_out;
    cookie_manager->GetAllCookies(base::BindLambdaForTesting(
        [&](const std::vector<net::CanonicalCookie>& cookies) {
          cookies_out = cookies;
          run_loop.Quit();
        }));
    run_loop.Run();
    return cookies_out;
  }

  SameSiteDataRemoverImpl* GetSameSiteDataRemoverImpl() {
    return same_site_remover_.get();
  }

  network::mojom::CookieManager* GetCookieManager() {
    StoragePartition* storage_partition =
        BrowserContext::GetDefaultStoragePartition(browser_context_.get());
    return storage_partition->GetCookieManagerForBrowserProcess();
  }

  void DeleteSameSiteNoneCookies() {
    base::RunLoop run_loop;
    GetSameSiteDataRemoverImpl()->DeleteSameSiteNoneCookies(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void ClearStoragePartitionData() {
    base::RunLoop run_loop;
    GetSameSiteDataRemoverImpl()->ClearStoragePartitionData(
        run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<BrowserContext> browser_context_;
  std::unique_ptr<SameSiteDataRemoverImpl> same_site_remover_;

  DISALLOW_COPY_AND_ASSIGN(SameSiteDataRemoverImplTest);
};

TEST_F(SameSiteDataRemoverImplTest, TestRemoveSameSiteNoneCookies) {
  CreateCookieForTest("TestCookie1", "www.google.com",
                      net::CookieSameSite::NO_RESTRICTION,
                      net::CookieOptions::SameSiteCookieContext::CROSS_SITE);
  CreateCookieForTest("TestCookie2", "www.gmail.google.com",
                      net::CookieSameSite::NO_RESTRICTION,
                      net::CookieOptions::SameSiteCookieContext::CROSS_SITE);

  DeleteSameSiteNoneCookies();

  EXPECT_THAT(GetSameSiteDataRemoverImpl()->GetDeletedDomainsForTesting(),
              UnorderedElementsAre("www.google.com", "www.gmail.google.com"));

  const std::vector<net::CanonicalCookie>& cookies =
      GetAllCookies(GetCookieManager());
  EXPECT_THAT(cookies, IsEmpty());
}

TEST_F(SameSiteDataRemoverImplTest, TestRemoveOnlySameSiteNoneCookies) {
  CreateCookieForTest("TestCookie1", "www.google.com",
                      net::CookieSameSite::NO_RESTRICTION,
                      net::CookieOptions::SameSiteCookieContext::CROSS_SITE);
  // The second cookie has SameSite value STRICT_MODE instead of NO_RESTRICTION.
  CreateCookieForTest(
      "TestCookie2", "www.gmail.google.com", net::CookieSameSite::STRICT_MODE,
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);

  DeleteSameSiteNoneCookies();

  EXPECT_THAT(GetSameSiteDataRemoverImpl()->GetDeletedDomainsForTesting(),
              UnorderedElementsAre("www.google.com"));

  const std::vector<net::CanonicalCookie>& cookies =
      GetAllCookies(GetCookieManager());
  ASSERT_EQ(1u, cookies.size());
  ASSERT_EQ(cookies[0].Name(), "TestCookie2");
}

TEST_F(SameSiteDataRemoverImplTest, TestRemoveSameDomainCookies) {
  CreateCookieForTest("TestCookie1", "www.google.com",
                      net::CookieSameSite::NO_RESTRICTION,
                      net::CookieOptions::SameSiteCookieContext::CROSS_SITE);
  // The second cookie has the same domain as the first cookie, but also has
  // SameSite value STRICT_MODE instead of NO_RESTRICTION.
  CreateCookieForTest(
      "TestCookie2", "www.google.com", net::CookieSameSite::STRICT_MODE,
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);

  DeleteSameSiteNoneCookies();

  EXPECT_THAT(GetSameSiteDataRemoverImpl()->GetDeletedDomainsForTesting(),
              UnorderedElementsAre("www.google.com"));

  const std::vector<net::CanonicalCookie>& cookies =
      GetAllCookies(GetCookieManager());
  ASSERT_EQ(1u, cookies.size());
  ASSERT_EQ(cookies[0].Name(), "TestCookie2");
}

TEST_F(SameSiteDataRemoverImplTest, TestKeepSameSiteCookies) {
  CreateCookieForTest("TestCookie1", "www.google.com",
                      net::CookieSameSite::LAX_MODE,
                      net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX);
  CreateCookieForTest(
      "TestCookie2", "www.gmail.google.com", net::CookieSameSite::STRICT_MODE,
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);

  DeleteSameSiteNoneCookies();

  ASSERT_THAT(GetSameSiteDataRemoverImpl()->GetDeletedDomainsForTesting(),
              IsEmpty());

  const std::vector<net::CanonicalCookie>& cookies =
      GetAllCookies(GetCookieManager());
  EXPECT_THAT(2u, cookies.size());
}

TEST_F(SameSiteDataRemoverImplTest, TestCookieRemovalUnaffectedByParameters) {
  network::mojom::CookieManager* cookie_manager = GetCookieManager();

  base::RunLoop run_loop1;
  net::CookieOptions options;
  options.set_include_httponly();
  bool result_out = false;
  cookie_manager->SetCanonicalCookie(
      net::CanonicalCookie("TestCookie1", "20", "google.com", "/",
                           base::Time::Now(), base::Time(), base::Time(), false,
                           true, net::CookieSameSite::NO_RESTRICTION,
                           net::COOKIE_PRIORITY_HIGH),
      "https", options,
      base::BindLambdaForTesting(
          [&](net::CanonicalCookie::CookieInclusionStatus result) {
            result_out = (result ==
                          net::CanonicalCookie::CookieInclusionStatus::INCLUDE);
            run_loop1.Quit();
          }));
  run_loop1.Run();
  EXPECT_TRUE(result_out);

  base::RunLoop run_loop2;
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX);
  result_out = false;
  cookie_manager->SetCanonicalCookie(
      net::CanonicalCookie("TestCookie2", "10", "gmail.google.com", "/",
                           base::Time(), base::Time::Max(), base::Time(), false,
                           true, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_HIGH),
      "https", options,
      base::BindLambdaForTesting(
          [&](net::CanonicalCookie::CookieInclusionStatus result) {
            result_out = (result ==
                          net::CanonicalCookie::CookieInclusionStatus::INCLUDE);
            run_loop2.Quit();
          }));
  run_loop2.Run();
  EXPECT_TRUE(result_out);

  DeleteSameSiteNoneCookies();

  EXPECT_THAT(GetSameSiteDataRemoverImpl()->GetDeletedDomainsForTesting(),
              UnorderedElementsAre("google.com"));

  const std::vector<net::CanonicalCookie>& cookies =
      GetAllCookies(cookie_manager);
  ASSERT_EQ(1u, cookies.size());
  ASSERT_EQ(cookies[0].Name(), "TestCookie2");
}

TEST_F(SameSiteDataRemoverImplTest, TestStoragePartitionDataRemoval) {
  SameSiteRemoverTestStoragePartition storage_partition;
  storage_partition.set_cookie_manager_for_browser_process(GetCookieManager());
  GetSameSiteDataRemoverImpl()->OverrideStoragePartitionForTesting(
      &storage_partition);

  CreateCookieForTest("TestCookie1", ".google.com",
                      net::CookieSameSite::NO_RESTRICTION,
                      net::CookieOptions::SameSiteCookieContext::CROSS_SITE);
  DeleteSameSiteNoneCookies();

  ClearStoragePartitionData();
  StoragePartitionSameSiteRemovalData removal_data =
      storage_partition.GetStoragePartitionRemovalData();

  const uint32_t expected_removal_mask =
      content::StoragePartition::REMOVE_DATA_MASK_ALL &
      ~content::StoragePartition::REMOVE_DATA_MASK_COOKIES;
  EXPECT_EQ(removal_data.removal_mask, expected_removal_mask);

  const uint32_t expected_quota_storage_mask =
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL;
  EXPECT_EQ(removal_data.quota_storage_removal_mask,
            expected_quota_storage_mask);

  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy =
      new MockSpecialStoragePolicy;
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://www.google.com/test")),
      special_storage_policy.get()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://google.com")),
      special_storage_policy.get()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://youtube.com")),
      special_storage_policy.get()));
}

}  // namespace content
