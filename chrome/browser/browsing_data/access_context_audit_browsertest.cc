// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/path_service.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/browsing_data/access_context_audit_service.h"
#include "chrome/browser/browsing_data/access_context_audit_service_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace {

// Use host names that are explicitly included in test certificates.
constexpr char kTopLevelHost[] = "a.test";
constexpr char kEmbeddedHost[] = "b.test";

std::string GetPathWithHostAndPortReplaced(const std::string& original_path,
                                           net::HostPortPair host_port_pair) {
  base::StringPairs replacement_text = {
      {"REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()}};
  return net::test_server::GetFilePathWithReplacements(original_path,
                                                       replacement_text);
}

// Checks if the cookie defined by |name|, |domain| and |path| is present in
// |cookies|, and that the record associating access to it to |top_frame_origin|
// is present in |record_list|. If |compare_host_only| is set, only the host
// portion of |top_frame_origin| will be used for comparison.
void CheckContainsCookieAndRecord(
    const std::vector<net::CanonicalCookie>& cookies,
    const std::vector<AccessContextAuditDatabase::AccessRecord>& record_list,
    const url::Origin& top_frame_origin,
    const std::string& name,
    const std::string& domain,
    const std::string& path,
    bool compare_host_only = false) {
  EXPECT_NE(
      std::find_if(
          record_list.begin(), record_list.end(),
          [=](const AccessContextAuditDatabase::AccessRecord& record) {
            return record.type ==
                       AccessContextAuditDatabase::StorageAPIType::kCookie &&
                   (compare_host_only
                        ? record.top_frame_origin.host() ==
                              top_frame_origin.host()
                        : record.top_frame_origin == top_frame_origin) &&
                   record.name == name && record.domain == domain &&
                   record.path == path;
          }),
      record_list.end());

  EXPECT_NE(std::find_if(cookies.begin(), cookies.end(),
                         [=](const net::CanonicalCookie& cookie) {
                           return cookie.Name() == name &&
                                  cookie.Domain() == domain &&
                                  cookie.Path() == path;
                         }),
            cookies.end());
}

}  // namespace

class AccessContextAuditBrowserTest : public InProcessBrowserTest {
 public:
  AccessContextAuditBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kClientStorageAccessContextAuditing);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    top_level_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    embedded_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    top_level_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    embedded_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(embedded_.Start());
    ASSERT_TRUE(top_level_.Start());
  }

  std::vector<AccessContextAuditDatabase::AccessRecord> GetAllAccessRecords() {
    base::RunLoop run_loop;
    std::vector<AccessContextAuditDatabase::AccessRecord> records_out;
    AccessContextAuditServiceFactory::GetForProfile(browser()->profile())
        ->GetAllAccessRecords(base::BindLambdaForTesting(
            [&](std::vector<AccessContextAuditDatabase::AccessRecord> records) {
              records_out = records;
              run_loop.QuitWhenIdle();
            }));
    run_loop.Run();
    return records_out;
  }

  std::vector<net::CanonicalCookie> GetAllCookies() {
    base::RunLoop run_loop;
    std::vector<net::CanonicalCookie> cookies_out;
    content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
        ->GetCookieManagerForBrowserProcess()
        ->GetAllCookies(base::BindLambdaForTesting(
            [&](const std::vector<net::CanonicalCookie>& cookies) {
              cookies_out = cookies;
              run_loop.QuitWhenIdle();
            }));
    run_loop.Run();
    return cookies_out;
  }

  url::Origin top_level_origin() {
    return url::Origin::Create(top_level_.GetURL(kTopLevelHost, "/"));
  }
  url::Origin embedded_origin() {
    return url::Origin::Create(embedded_.GetURL(kEmbeddedHost, "/"));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer top_level_{net::EmbeddedTestServer::TYPE_HTTPS};
  net::EmbeddedTestServer embedded_{net::EmbeddedTestServer::TYPE_HTTPS};

  std::vector<AccessContextAuditDatabase::AccessRecord> records_;
};

IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, PRE_PRE_RemoveRecords) {
  // Navigate to a page that accesses storage APIs and also embeds a site which
  // accesses storage APIs.
  std::string replacement_path = GetPathWithHostAndPortReplaced(
      "/browsing_data/embeds_storage_accessor.html",
      net::HostPortPair::FromURL(embedded_.GetURL(kEmbeddedHost, "/")));
  ui_test_utils::NavigateToURL(
      browser(), top_level_.GetURL(kTopLevelHost, replacement_path));

  // Navigate directly to the embedded page, such that accesses are also
  // recorded against its top frame origin.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_.GetURL(kEmbeddedHost, "/browsing_data/storage_accessor.html"));
  base::RunLoop().RunUntilIdle();

  // Check storage accesses have been correctly recorded.
  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();
  EXPECT_EQ(records.size(), 5u);
  EXPECT_EQ(cookies.size(), 3u);
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(), "embedder",
                               kTopLevelHost, "/");
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(),
                               "persistent", kEmbeddedHost, "/");
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(),
                               "session_only", kEmbeddedHost, "/");
  CheckContainsCookieAndRecord(cookies, records, embedded_origin(),
                               "persistent", kEmbeddedHost, "/");
  CheckContainsCookieAndRecord(cookies, records, embedded_origin(),
                               "session_only", kEmbeddedHost, "/");
}

IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, PRE_RemoveRecords) {
  // Check that only persistent records have been persisted across restart.
  // Unfortunately the correct top frame origin is lost in the test as the
  // embedded test servers will have changed port, so only the host can be
  // reliably compared.
  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();
  EXPECT_EQ(records.size(), 3u);
  EXPECT_EQ(cookies.size(), 2u);
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(), "embedder",
                               kTopLevelHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(),
                               "persistent", kEmbeddedHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsCookieAndRecord(cookies, records, embedded_origin(),
                               "persistent", kEmbeddedHost, "/",
                               /*compare_host_only*/ true);
}

IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, RemoveRecords) {
  // Immediately remove all records and ensure no record remains.
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(browser()->profile());
  content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
  remover->RemoveAndReply(base::Time(), base::Time::Max(),
                          ChromeBrowsingDataRemoverDelegate::ALL_DATA_TYPES,
                          ChromeBrowsingDataRemoverDelegate::ALL_ORIGIN_TYPES,
                          &completion_observer);
  completion_observer.BlockUntilCompletion();

  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();
  EXPECT_EQ(records.size(), 0u);
  EXPECT_EQ(cookies.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, PRE_CheckSessionOnly) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  map->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                ContentSetting::CONTENT_SETTING_SESSION_ONLY);

  std::string replacement_path = GetPathWithHostAndPortReplaced(
      "/browsing_data/embeds_storage_accessor.html",
      net::HostPortPair::FromURL(embedded_.GetURL(kEmbeddedHost, "/")));
  ui_test_utils::NavigateToURL(
      browser(), top_level_.GetURL(kTopLevelHost, replacement_path));

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_.GetURL(kEmbeddedHost, "/browsing_data/storage_accessor.html"));
  base::RunLoop().RunUntilIdle();

  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();
  EXPECT_EQ(records.size(), 5u);
  EXPECT_EQ(cookies.size(), 3u);
}

IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, CheckSessionOnly) {
  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();
  EXPECT_EQ(records.size(), 0u);
  EXPECT_EQ(cookies.size(), 0u);
}

class AccessContextAuditSessionRestoreBrowserTest
    : public AccessContextAuditBrowserTest {
 public:
  void SetUpOnMainThread() override {
    SessionStartupPref::SetStartupPref(
        browser()->profile(), SessionStartupPref(SessionStartupPref::LAST));
    AccessContextAuditBrowserTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(AccessContextAuditSessionRestoreBrowserTest,
                       PRE_RestoreSession) {
  // Navigate to test URLS which set a mixture of persistent and non-persistent
  // cookies.
  std::string replacement_path = GetPathWithHostAndPortReplaced(
      "/browsing_data/embeds_storage_accessor.html",
      net::HostPortPair::FromURL(embedded_.GetURL(kEmbeddedHost, "/")));
  ui_test_utils::NavigateToURL(
      browser(), top_level_.GetURL(kTopLevelHost, replacement_path));

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_.GetURL(kEmbeddedHost, "/browsing_data/storage_accessor.html"));
  base::RunLoop().RunUntilIdle();

  // Check storage accesses have been correctly recorded.
  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();
  EXPECT_EQ(records.size(), 5u);
  EXPECT_EQ(cookies.size(), 3u);
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(), "embedder",
                               kTopLevelHost, "/");
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(),
                               "persistent", kEmbeddedHost, "/");
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(),
                               "session_only", kEmbeddedHost, "/");
  CheckContainsCookieAndRecord(cookies, records, embedded_origin(),
                               "persistent", kEmbeddedHost, "/");
  CheckContainsCookieAndRecord(cookies, records, embedded_origin(),
                               "session_only", kEmbeddedHost, "/");
}

IN_PROC_BROWSER_TEST_F(AccessContextAuditSessionRestoreBrowserTest,
                       RestoreSession) {
  // Check all access records have been correctly persisted across restarts.
  auto records = GetAllAccessRecords();
  auto cookies = GetAllCookies();
  EXPECT_EQ(records.size(), 5u);
  EXPECT_EQ(cookies.size(), 3u);
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(), "embedder",
                               kTopLevelHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(),
                               "session_only", kEmbeddedHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsCookieAndRecord(cookies, records, top_level_origin(),
                               "persistent", kEmbeddedHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsCookieAndRecord(cookies, records, embedded_origin(),
                               "persistent", kEmbeddedHost, "/",
                               /*compare_host_only*/ true);
  CheckContainsCookieAndRecord(cookies, records, embedded_origin(),
                               "session_only", kEmbeddedHost, "/",
                               /*compare_host_only*/ true);
}
