// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_top_host_provider_impl.h"

#include "base/values.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/previews/content/previews_hints_util.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

// Class to test the TopHostProvider and the HintsFetcherTopHostBlacklist.
class PreviewsTopHostProviderImplTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    top_host_provider_ =
        std::make_unique<PreviewsTopHostProviderImpl>(profile());
    service_ = SiteEngagementService::Get(profile());
    pref_service_ = profile()->GetPrefs();
  }

  void AddEngagedHosts(size_t num_hosts) {
    for (size_t i = 1; i <= num_hosts; i++) {
      AddEngagedHost(GURL(base::StringPrintf("https://domain%zu.com", i)),
                     static_cast<int>(i));
    }
  }

  void AddEngagedHost(GURL url, int num_points) {
    service_->AddPointsForTesting(url, num_points);
  }

  bool IsHostBlacklisted(const std::string& host) {
    const base::DictionaryValue* top_host_blacklist =
        pref_service_->GetDictionary(
            optimization_guide::prefs::kHintsFetcherTopHostBlacklist);
    return top_host_blacklist->FindKey(previews::HashHostForDictionary(host));
  }

  void PopulateTopHostBlacklist(size_t num_hosts) {
    std::unique_ptr<base::DictionaryValue> top_host_filter =
        pref_service_
            ->GetDictionary(
                optimization_guide::prefs::kHintsFetcherTopHostBlacklist)
            ->CreateDeepCopy();

    for (size_t i = 1; i <= num_hosts; i++) {
      top_host_filter->SetBoolKey(
          HashHostForDictionary(base::StringPrintf("domain%zu.com", i)), true);
    }
    pref_service_->Set(optimization_guide::prefs::kHintsFetcherTopHostBlacklist,
                       *top_host_filter);
  }

  void AddHostToBlackList(const std::string& host) {
    std::unique_ptr<base::DictionaryValue> top_host_filter =
        pref_service_
            ->GetDictionary(
                optimization_guide::prefs::kHintsFetcherTopHostBlacklist)
            ->CreateDeepCopy();
    top_host_filter->SetBoolKey(previews::HashHostForDictionary(host), true);
    pref_service_->Set(optimization_guide::prefs::kHintsFetcherTopHostBlacklist,
                       *top_host_filter);
  }

  void SimulateUniqueNavigationsToTopHosts(size_t num_hosts) {
    for (size_t i = 1; i <= num_hosts; i++) {
      SimulateNavigation(GURL(base::StringPrintf("https://domain%zu.com", i)));
    }
  }

  void SimulateNavigation(GURL url) {
    std::unique_ptr<content::MockNavigationHandle> test_handle_ =
        std::make_unique<content::MockNavigationHandle>(url, main_rfh());
    PreviewsTopHostProviderImpl::MaybeUpdateTopHostBlacklist(
        test_handle_.get());
  }

  void RemoveHostsFromBlacklist(size_t num_hosts_navigated) {
    std::unique_ptr<base::DictionaryValue> top_host_filter =
        pref_service_
            ->GetDictionary(
                optimization_guide::prefs::kHintsFetcherTopHostBlacklist)
            ->CreateDeepCopy();

    for (size_t i = 1; i <= num_hosts_navigated; i++) {
      top_host_filter->RemoveKey(
          HashHostForDictionary(base::StringPrintf("domain%zu.com", i)));
    }
    pref_service_->Set(optimization_guide::prefs::kHintsFetcherTopHostBlacklist,
                       *top_host_filter);
  }

  void SetTopHostBlacklistState(
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState
          blacklist_state) {
    profile()->GetPrefs()->SetInteger(
        optimization_guide::prefs::kHintsFetcherTopHostBlacklistState,
        static_cast<int>(blacklist_state));
  }

  optimization_guide::prefs::HintsFetcherTopHostBlacklistState
  GetCurrentTopHostBlacklistState() {
    return static_cast<
        optimization_guide::prefs::HintsFetcherTopHostBlacklistState>(
        pref_service_->GetInteger(
            optimization_guide::prefs::kHintsFetcherTopHostBlacklistState));
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  PreviewsTopHostProviderImpl* top_host_provider() {
    return top_host_provider_.get();
  }

 private:
  std::unique_ptr<PreviewsTopHostProviderImpl> top_host_provider_;
  SiteEngagementService* service_;
  PrefService* pref_service_;
};

TEST_F(PreviewsTopHostProviderImplTest, GetTopHostsMaxSites) {
  SetTopHostBlacklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlacklistState::kInitialized);
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 3;
  AddEngagedHosts(engaged_hosts);

  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);

  EXPECT_EQ(max_top_hosts, hosts.size());
}

TEST_F(PreviewsTopHostProviderImplTest,
       GetTopHostsFiltersPrivacyBlackedlistedHosts) {
  SetTopHostBlacklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlacklistState::kInitialized);
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_hosts_blacklisted = 2;
  AddEngagedHosts(engaged_hosts);

  PopulateTopHostBlacklist(num_hosts_blacklisted);

  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);

  EXPECT_EQ(hosts.size(), engaged_hosts - num_hosts_blacklisted);
}

TEST_F(PreviewsTopHostProviderImplTest, GetTopHostsInitializeBlacklistState) {
  EXPECT_EQ(GetCurrentTopHostBlacklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
                kNotInitialized);
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  AddEngagedHosts(engaged_hosts);

  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);
  // On initialization, GetTopHosts should always return zero hosts.
  EXPECT_EQ(hosts.size(), 0u);
  EXPECT_EQ(GetCurrentTopHostBlacklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
                kInitialized);
}

TEST_F(PreviewsTopHostProviderImplTest,
       GetTopHostsBlacklistStateNotInitializedToInitialized) {
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_hosts_blacklisted = 5;
  AddEngagedHosts(engaged_hosts);
  // TODO(mcrouse): Remove once the blacklist is populated on initialization.
  // The expected behavior will be that all hosts in the engagement service will
  // be blacklisted on initialization.
  PopulateTopHostBlacklist(num_hosts_blacklisted);

  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), 0u);

  // Blacklist should now have items removed.
  size_t num_navigations = 2;
  RemoveHostsFromBlacklist(num_navigations);

  hosts = top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(),
            engaged_hosts - (num_hosts_blacklisted - num_navigations));
  EXPECT_EQ(GetCurrentTopHostBlacklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
                kInitialized);
}

TEST_F(PreviewsTopHostProviderImplTest,
       GetTopHostsBlacklistStateNotInitializedToEmpty) {
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_hosts_blacklisted = 5;
  AddEngagedHosts(engaged_hosts);
  // TODO(mcrouse): Remove once the blacklist is populated on initialization.
  // The expected behavior will be that all hosts in the engagement service will
  // be blacklisted on initialization.
  PopulateTopHostBlacklist(num_hosts_blacklisted);

  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), 0u);

  // Blacklist should now have items removed.
  size_t num_navigations = 5;
  RemoveHostsFromBlacklist(num_navigations);

  hosts = top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(),
            engaged_hosts - (num_hosts_blacklisted - num_navigations));
  EXPECT_EQ(
      GetCurrentTopHostBlacklistState(),
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState::kEmpty);
}

TEST_F(PreviewsTopHostProviderImplTest,
       MaybeUpdateTopHostBlacklistNavigationsOnBlacklist) {
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_hosts_blacklisted = 5;
  size_t num_top_hosts = 3;
  AddEngagedHosts(engaged_hosts);
  // TODO(mcrouse): Remove once the blacklist is populated on initialization.
  // The expected behavior will be that all hosts in the engagement service will
  // be blacklisted on initialization.
  PopulateTopHostBlacklist(num_hosts_blacklisted);

  // Verify that all engaged hosts are blacklisted.
  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), 0u);

  // Navigate to some engaged hosts to trigger their removal from the top host
  // blacklist.
  SimulateUniqueNavigationsToTopHosts(num_top_hosts);

  hosts = top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), num_top_hosts);
}

TEST_F(PreviewsTopHostProviderImplTest,
       MaybeUpdateTopHostBlacklistEmptyBlacklist) {
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_hosts_blacklisted = 5;
  size_t num_top_hosts = 5;
  AddEngagedHosts(engaged_hosts);
  // TODO(mcrouse): Remove once the blacklist is populated on initialization.
  // The expected behavior will be that all hosts in the engagement service will
  // be blacklisted on initialization.
  PopulateTopHostBlacklist(num_hosts_blacklisted);

  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), 0u);

  SimulateUniqueNavigationsToTopHosts(num_top_hosts);

  EXPECT_EQ(
      GetCurrentTopHostBlacklistState(),
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState::kEmpty);

  hosts = top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), num_top_hosts);
}

TEST_F(PreviewsTopHostProviderImplTest,
       HintsFetcherTopHostBlacklistNonHTTPOrHTTPSHost) {
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_hosts_blacklisted = 5;
  GURL http_url = GURL("http://anyscheme.com");
  GURL file_url = GURL("file://anyscheme.com");
  AddEngagedHosts(engaged_hosts);
  AddEngagedHost(http_url, 5);
  // TODO(mcrouse): Remove once the blacklist is populated on initialization.
  // The expected behavior will be that all hosts in the engagement service will
  // be blacklisted on initialization.
  PopulateTopHostBlacklist(num_hosts_blacklisted);
  AddHostToBlackList(http_url.host());

  SetTopHostBlacklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlacklistState::kInitialized);

  // A Non HTTP/HTTPS navigation should not remove a host from the blacklist.
  SimulateNavigation(file_url);
  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), 0u);
  // The host, anyscheme.com, should still be on the blacklist.
  EXPECT_TRUE(IsHostBlacklisted(file_url.host()));

  // TopHostProviderImpl prevents HTTP hosts from being returned.
  SimulateNavigation(http_url);
  hosts = top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), 0u);

  EXPECT_FALSE(IsHostBlacklisted(http_url.host()));
}

}  // namespace previews
