// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_top_host_provider.h"

#include "base/values.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/hints_processing_util.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

// Class to test the TopHostProvider and the HintsFetcherTopHostBlacklist.
class PreviewsTopHostProviderTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    top_host_provider_ = std::make_unique<PreviewsTopHostProvider>(profile());
    service_ = SiteEngagementService::Get(profile());
    pref_service_ = profile()->GetPrefs();
  }

  void AddEngagedHosts(size_t num_hosts) {
    for (size_t i = 1; i <= num_hosts; i++) {
      AddEngagedHost(
          GURL(base::StringPrintf("https://domain%zu.com", i)),
          static_cast<int>(
              i + SiteEngagementScore::GetFirstDailyEngagementPoints()));
    }
  }

  void AddEngagedHost(GURL url, int num_points) {
    service_->AddPointsForTesting(url, num_points);
  }

  bool IsHostBlacklisted(const std::string& host) {
    const base::DictionaryValue* top_host_blacklist =
        pref_service_->GetDictionary(
            optimization_guide::prefs::kHintsFetcherTopHostBlacklist);
    return top_host_blacklist->FindKey(
        optimization_guide::HashHostForDictionary(host));
  }

  void PopulateTopHostBlacklist(size_t num_hosts) {
    std::unique_ptr<base::DictionaryValue> top_host_filter =
        pref_service_
            ->GetDictionary(
                optimization_guide::prefs::kHintsFetcherTopHostBlacklist)
            ->CreateDeepCopy();

    for (size_t i = 1; i <= num_hosts; i++) {
      top_host_filter->SetBoolKey(optimization_guide::HashHostForDictionary(
                                      base::StringPrintf("domain%zu.com", i)),
                                  true);
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
    top_host_filter->SetBoolKey(optimization_guide::HashHostForDictionary(host),
                                true);
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
    PreviewsTopHostProvider::MaybeUpdateTopHostBlacklist(test_handle_.get());
  }

  void RemoveHostsFromBlacklist(size_t num_hosts_navigated) {
    std::unique_ptr<base::DictionaryValue> top_host_filter =
        pref_service_
            ->GetDictionary(
                optimization_guide::prefs::kHintsFetcherTopHostBlacklist)
            ->CreateDeepCopy();

    for (size_t i = 1; i <= num_hosts_navigated; i++) {
      top_host_filter->RemoveKey(optimization_guide::HashHostForDictionary(
          base::StringPrintf("domain%zu.com", i)));
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

  PreviewsTopHostProvider* top_host_provider() {
    return top_host_provider_.get();
  }

 private:
  std::unique_ptr<PreviewsTopHostProvider> top_host_provider_;
  SiteEngagementService* service_;
  PrefService* pref_service_;
};

TEST_F(PreviewsTopHostProviderTest, GetTopHostsMaxSites) {
  SetTopHostBlacklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlacklistState::kInitialized);
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 3;
  AddEngagedHosts(engaged_hosts);

  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);

  EXPECT_EQ(max_top_hosts, hosts.size());
}

TEST_F(PreviewsTopHostProviderTest,
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

TEST_F(PreviewsTopHostProviderTest, GetTopHostsInitializeBlacklistState) {
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

TEST_F(PreviewsTopHostProviderTest,
       GetTopHostsBlacklistStateNotInitializedToInitialized) {
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_hosts_blacklisted = 5;
  AddEngagedHosts(engaged_hosts);

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

TEST_F(PreviewsTopHostProviderTest,
       GetTopHostsBlacklistStateNotInitializedToEmpty) {
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_hosts_blacklisted = 5;
  AddEngagedHosts(engaged_hosts);

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

TEST_F(PreviewsTopHostProviderTest,
       MaybeUpdateTopHostBlacklistNavigationsOnBlacklist) {
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_top_hosts = 3;
  AddEngagedHosts(engaged_hosts);

  // The blacklist should be populated on the first request.
  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), 0u);

  // Navigate to some engaged hosts to trigger their removal from the top host
  // blacklist.
  SimulateUniqueNavigationsToTopHosts(num_top_hosts);

  hosts = top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), num_top_hosts);
}

TEST_F(PreviewsTopHostProviderTest, MaybeUpdateTopHostBlacklistEmptyBlacklist) {
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_top_hosts = 5;
  AddEngagedHosts(engaged_hosts);

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

TEST_F(PreviewsTopHostProviderTest,
       HintsFetcherTopHostBlacklistNonHTTPOrHTTPSHost) {
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_hosts_blacklisted = 5;
  GURL http_url = GURL("http://anyscheme.com");
  GURL file_url = GURL("file://anyscheme.com");
  AddEngagedHosts(engaged_hosts);
  AddEngagedHost(http_url, 5);

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

TEST_F(PreviewsTopHostProviderTest, IntializeTopHostBlacklistWithMaxTopSites) {
  size_t engaged_hosts =
      optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize() + 1;
  size_t max_top_hosts =
      optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize() + 1;
  AddEngagedHosts(engaged_hosts);

  // Blacklist should be populated on the first request.
  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), 0u);

  hosts = top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(
      hosts.size(),
      engaged_hosts -
          optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize());
  EXPECT_EQ(GetCurrentTopHostBlacklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
                kInitialized);

  // The last host has the most engagement points so it will be blacklisted. The
  // first host has the lowest engagement score and will not be blacklisted
  // because it is not in the top MaxHintsFetcherTopHostBlacklistSize engaged
  // hosts by engagement score.
  EXPECT_TRUE(IsHostBlacklisted(base::StringPrintf(
      "domain%zu.com",
      optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize())));
  EXPECT_FALSE(IsHostBlacklisted(base::StringPrintf("domain%u.com", 1u)));
}

TEST_F(PreviewsTopHostProviderTest, TopHostsFilteredByEngagementThreshold) {
  size_t engaged_hosts =
      optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize() + 1;
  size_t max_top_hosts =
      optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize() + 1;

  AddEngagedHosts(engaged_hosts);
  // Add two hosts with very low engagement scores that should not be returned
  // by the top host provider.
  AddEngagedHost(GURL("https://lowengagement.com"), 1);
  AddEngagedHost(GURL("https://lowengagement2.com"), 1);

  // Blacklist should be populated on the first request.
  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), 0u);

  hosts = top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(
      hosts.size(),
      engaged_hosts -
          optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize());
  EXPECT_EQ(GetCurrentTopHostBlacklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
                kInitialized);

  // The hosts with engagement scores below the minimum threshold should not be
  // returned.
  EXPECT_EQ(std::find(hosts.begin(), hosts.end(), "lowengagement.com"),
            hosts.end());
  EXPECT_EQ(std::find(hosts.begin(), hosts.end(), "lowengagement2.com"),
            hosts.end());
}
