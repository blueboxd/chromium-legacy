// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/third_party_metrics_observer.h"

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kReadCookieHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.CookieRead";
const char kWriteCookieHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.CookieWrite";
const char kAccessLocalStorageHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.LocalStorageAccess";
const char kAccessSessionStorageHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.SessionStorageAccess";

class ThirdPartyMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  ThirdPartyMetricsObserverTest() {}

  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::WrapUnique(new ThirdPartyMetricsObserver()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ThirdPartyMetricsObserverTest);
};

TEST_F(ThirdPartyMetricsObserverTest, NoCookiesRead_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest, BlockedCookiesRead_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // If there are any blocked_by_policy reads, nothing should be recorded. Even
  // if there are subsequent non-blocked third-party reads.
  SimulateCookiesRead(GURL("https://a.com"), GURL("https://top.com"),
                      net::CookieList(), true /* blocked_by_policy */);
  SimulateCookiesRead(GURL("https://a.com"), GURL("https://top.com"),
                      net::CookieList(), false /* blocked_by_policy */);

  NavigateToUntrackedUrl();

  histogram_tester().ExpectTotalCount(kReadCookieHistogram, 0);
}

TEST_F(ThirdPartyMetricsObserverTest,
       NoRegistrableDomainNoHostCookiesRead_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  GURL url = GURL("data:,Hello%2C%20World!");
  ASSERT_FALSE(url.has_host());
  SimulateCookiesRead(url, GURL("https://top.com"), net::CookieList(),
                      false /* blocked_by_policy */);
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       NoRegistrableDomainWithHostCookiesRead_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  GURL url = GURL("https://127.0.0.1/cookies");
  ASSERT_TRUE(url.has_host());
  SimulateCookiesRead(url, GURL("https://top.com"), net::CookieList(),
                      false /* blocked_by_policy */);
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
}

TEST_F(ThirdPartyMetricsObserverTest, OnlyFirstPartyCookiesRead_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  SimulateCookiesRead(GURL("https://top.com"), GURL("https://top.com"),
                      net::CookieList(), false /* blocked_by_policy */);
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest, OneCookieRead_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  SimulateCookiesRead(GURL("https://a.com"), GURL("https://top.com"),
                      net::CookieList(), false /* blocked_by_policy */);
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
  histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       ThreeCookiesReadSameThirdParty_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  SimulateCookiesRead(GURL("https://a.com"), GURL("https://top.com"),
                      net::CookieList(), false /* blocked_by_policy */);
  SimulateCookiesRead(GURL("https://a.com/foo"), GURL("https://top.com"),
                      net::CookieList(), false /* blocked_by_policy */);
  SimulateCookiesRead(GURL("https://sub.a.com/bar"), GURL("https://top.com"),
                      net::CookieList(), false /* blocked_by_policy */);

  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       CookiesReadMultipleThirdParties_MultipleRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // Simulate third-party cookie reads from two different origins.
  SimulateCookiesRead(GURL("https://a.com"), GURL("https://top.com"),
                      net::CookieList(), false /* blocked_by_policy */);
  SimulateCookiesRead(GURL("https://a.com"), GURL("https://top.com"),
                      net::CookieList(), false /* blocked_by_policy */);
  SimulateCookiesRead(GURL("https://b.com"), GURL("https://top.com"),
                      net::CookieList(), false /* blocked_by_policy */);
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 2, 1);
}

TEST_F(ThirdPartyMetricsObserverTest, NoCookiesChanged_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest, BlockedCookiesChanged_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // If there are any blocked_by_policy writes, nothing should be recorded. Even
  // if there are non-blocked third-party writes.
  SimulateCookieChange(GURL("https://a.com"), GURL("https://top.com"),
                       net::CanonicalCookie(), false /* blocked_by_policy */);
  SimulateCookieChange(GURL("https://a.com"), GURL("https://top.com"),
                       net::CanonicalCookie(), true /* blocked_by_policy */);
  NavigateToUntrackedUrl();
  histogram_tester().ExpectTotalCount(kWriteCookieHistogram, 0);
}

TEST_F(ThirdPartyMetricsObserverTest,
       NoRegistrableDomainNoHostCookiesChanged_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  GURL url = GURL("data:,Hello%2C%20World!");
  ASSERT_FALSE(url.has_host());
  SimulateCookieChange(url, GURL("https://top.com"), net::CanonicalCookie(),
                       false /* blocked_by_policy */);
  NavigateToUntrackedUrl();
  histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       NoRegistrableDomainWithHostCookiesChanged_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  GURL url = GURL("https://127.0.0.1/cookies");
  ASSERT_TRUE(url.has_host());
  SimulateCookieChange(url, GURL("https://top.com"), net::CanonicalCookie(),
                       false /* blocked_by_policy */);
  NavigateToUntrackedUrl();
  histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       OnlyFirstPartyCookiesChanged_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  SimulateCookieChange(GURL("https://top.com"), GURL("https://top.com"),
                       net::CanonicalCookie(), false /* blocked_by_policy */);
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest, OneCookieChanged_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  SimulateCookieChange(GURL("https://a.com"), GURL("https://top.com"),
                       net::CanonicalCookie(), false /* blocked_by_policy */);
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
  histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       TwoCookiesChangeSameThirdParty_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  SimulateCookieChange(GURL("https://a.com"), GURL("https://top.com"),
                       net::CanonicalCookie(), false /* blocked_by_policy */);
  SimulateCookieChange(GURL("https://a.com"), GURL("https://top.com"),
                       net::CanonicalCookie(), false /* blocked_by_policy */);
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       CookiesChangeMultipleThirdParties_MultipleRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // Simulate third-party cookie reads from two different origins.
  SimulateCookieChange(GURL("https://a.com"), GURL("https://top.com"),
                       net::CanonicalCookie(), false /* blocked_by_policy */);
  SimulateCookieChange(GURL("https://a.com"), GURL("https://top.com"),
                       net::CanonicalCookie(), false /* blocked_by_policy */);
  SimulateCookieChange(GURL("https://b.com"), GURL("https://top.com"),
                       net::CanonicalCookie(), false /* blocked_by_policy */);
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 2, 1);
}

TEST_F(ThirdPartyMetricsObserverTest, ReadAndChangeCookies_BothRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // Simulate third-party cookie reads from two different origins.
  SimulateCookiesRead(GURL("https://a.com"), GURL("https://top.com"),
                      net::CookieList(), false /* blocked_by_policy */);
  SimulateCookieChange(GURL("https://b.com"), GURL("https://top.com"),
                       net::CanonicalCookie(), false /* blocked_by_policy */);
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
  histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
}

TEST_F(ThirdPartyMetricsObserverTest, NoDomStorageAccess_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(kAccessLocalStorageHistogram, 0, 1);
  histogram_tester().ExpectUniqueSample(kAccessSessionStorageHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       LocalAndSessionStorageAccess_BothRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  SimulateDomStorageAccess(GURL("https://a.com"), GURL("https://top.com"),
                           true /* local */, false /* blocked_by_policy */);
  SimulateDomStorageAccess(GURL("https://a.com"), GURL("https://top.com"),
                           false /* local */, false /* blocked_by_policy */);
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(kAccessLocalStorageHistogram, 1, 1);
  histogram_tester().ExpectUniqueSample(kAccessSessionStorageHistogram, 1, 1);
}

class ThirdPartyDomStorageAccessMetricsObserverTest
    : public ThirdPartyMetricsObserverTest,
      public ::testing::WithParamInterface<bool /* is_local_access */> {
 public:
  bool IsLocal() const { return GetParam(); }

  const char* DomStorageHistogramName() const {
    return IsLocal() ? kAccessLocalStorageHistogram
                     : kAccessSessionStorageHistogram;
  }
};

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       BlockedDomStorageAccess_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // If there are any blocked_by_policy access, nothing should be recorded. Even
  // if there are subsequent non-blocked third-party access.
  SimulateDomStorageAccess(GURL("https://a.com"), GURL("https://top.com"),
                           IsLocal(), true /* blocked_by_policy */);
  SimulateDomStorageAccess(GURL("https://a.com"), GURL("https://top.com"),
                           IsLocal(), false /* blocked_by_policy */);

  NavigateToUntrackedUrl();

  histogram_tester().ExpectTotalCount(DomStorageHistogramName(), 0);
}

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       NoRegistrableDomainDomStorageAccess_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  SimulateDomStorageAccess(GURL("data:,Hello%2C%20World!"),
                           GURL("https://top.com"), IsLocal(),
                           false /* blocked_by_policy */);
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 1, 1);
}

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       OnlyFirstPartyDomStorageAccess_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  SimulateDomStorageAccess(GURL("https://top.com"), GURL("https://top.com"),
                           IsLocal(), false /* blocked_by_policy */);
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 0, 1);
}

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       OneDomStorageAccess_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  SimulateDomStorageAccess(GURL("https://a.com"), GURL("https://top.com"),
                           IsLocal(), false /* blocked_by_policy */);
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 1, 1);
}

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       SameRegistrableDomainDifferentOrigin_TwoRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  SimulateDomStorageAccess(GURL("https://a.com"), GURL("https://top.com"),
                           IsLocal(), false /* blocked_by_policy */);
  SimulateDomStorageAccess(GURL("https://sub.a.com"), GURL("https://top.com"),
                           IsLocal(), false /* blocked_by_policy */);

  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 2, 1);
}

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       DomStorageAccessMultipleThirdParties_MultipleRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // Simulate third-party DOM storage access from two different origins.
  SimulateDomStorageAccess(GURL("https://a.com"), GURL("https://top.com"),
                           IsLocal(), false /* blocked_by_policy */);
  SimulateDomStorageAccess(GURL("https://a.com"), GURL("https://top.com"),
                           IsLocal(), false /* blocked_by_policy */);
  SimulateDomStorageAccess(GURL("https://b.com"), GURL("https://top.com"),
                           IsLocal(), false /* blocked_by_policy */);
  NavigateToUntrackedUrl();

  histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 2, 1);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ThirdPartyDomStorageAccessMetricsObserverTest,
    ::testing::Values(false, true));
