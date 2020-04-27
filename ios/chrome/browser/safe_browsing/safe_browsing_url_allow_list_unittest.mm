// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/safe_browsing_url_allow_list.h"

#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/web_state_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using safe_browsing::SBThreatType;

namespace {
// Mocked WebStateObserver for use in tests.
class MockWebStateObserver : public web::WebStateObserver {
 public:
  MockWebStateObserver() {}
  ~MockWebStateObserver() {}

  MOCK_METHOD1(DidChangeVisibleSecurityState, void(web::WebState*));
};
}

// Test fixture for SafeBrowsingUrlAllowList.
class SafeBrowsingUrlAllowListTest : public PlatformTest {
 public:
  SafeBrowsingUrlAllowListTest() {
    web_state_.AddObserver(&web_state_observer_);
    SafeBrowsingUrlAllowList::CreateForWebState(&web_state_);
  }
  ~SafeBrowsingUrlAllowListTest() override {
    web_state_.RemoveObserver(&web_state_observer_);
  }

  SafeBrowsingUrlAllowList* allow_list() {
    return SafeBrowsingUrlAllowList::FromWebState(&web_state_);
  }

 protected:
  MockWebStateObserver web_state_observer_;
  web::TestWebState web_state_;
};

// Tests that the allowed threat types are properly recorded.
TEST_F(SafeBrowsingUrlAllowListTest, AllowUnsafeNavigations) {
  const GURL url("http://www.chromium.test");

  // Unsafe navigations should not initially be allowed.
  EXPECT_FALSE(allow_list()->AreUnsafeNavigationsAllowed(url));

  // Allow navigations to |url| that encounter |threat_type|.
  SBThreatType threat_type = safe_browsing::SB_THREAT_TYPE_URL_MALWARE;
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->AllowUnsafeNavigations(url, threat_type);

  // Verify that navigations to |url| are allowed for |threat_type| and that the
  // allowed threat set is populated correctly.
  std::set<SBThreatType> allowed_threat_types;
  EXPECT_TRUE(
      allow_list()->AreUnsafeNavigationsAllowed(url, &allowed_threat_types));
  EXPECT_EQ(1U, allowed_threat_types.size());
  EXPECT_NE(allowed_threat_types.find(threat_type), allowed_threat_types.end());

  // Disallow unsafe navigations to |url| and verify that the list is updated.
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->DisallowUnsafeNavigations(url);
  EXPECT_FALSE(allow_list()->AreUnsafeNavigationsAllowed(url));
}

// Tests that pending unsafe navigation decisions are properly recorded.
TEST_F(SafeBrowsingUrlAllowListTest, AddPendingDecision) {
  const GURL url("http://www.chromium.test");

  // The URL should not initially have any pending decisions.
  EXPECT_FALSE(allow_list()->IsUnsafeNavigationDecisionPending(url));

  // Add a pending decision for |url|
  SBThreatType threat_type = safe_browsing::SB_THREAT_TYPE_URL_MALWARE;
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->AddPendingUnsafeNavigationDecision(url, threat_type);

  // Verify that the pending decision is recorded and that the pending threat
  // type set is populated correctly.
  std::set<SBThreatType> pending_threat_types;
  EXPECT_TRUE(allow_list()->IsUnsafeNavigationDecisionPending(
      url, &pending_threat_types));
  EXPECT_EQ(1U, pending_threat_types.size());
  EXPECT_NE(pending_threat_types.find(threat_type), pending_threat_types.end());
}

// Tests that the pending decisions for a threat type are erased if the threat
// has been allowed for that URL.
TEST_F(SafeBrowsingUrlAllowListTest, AllowPendingThreat) {
  const GURL url("http://www.chromium.test");
  SBThreatType threat_type = safe_browsing::SB_THREAT_TYPE_URL_MALWARE;
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->AddPendingUnsafeNavigationDecision(url, threat_type);

  // Whitelist the decision and verify that the decision is no longer pending.
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->AllowUnsafeNavigations(url, threat_type);
  EXPECT_TRUE(allow_list()->AreUnsafeNavigationsAllowed(url));
  EXPECT_FALSE(allow_list()->IsUnsafeNavigationDecisionPending(url));
}

// Tests that removing a pending decision clears state properly.
TEST_F(SafeBrowsingUrlAllowListTest, RemovePendingDecision) {
  const GURL url("http://www.chromium.test");
  SBThreatType threat_type = safe_browsing::SB_THREAT_TYPE_URL_MALWARE;
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->AddPendingUnsafeNavigationDecision(url, threat_type);

  // Remove the pending decision and verify that the whitelist is updated.
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->RemovePendingUnsafeNavigationDecisions(url);
  EXPECT_FALSE(allow_list()->IsUnsafeNavigationDecisionPending(url));
}

// Tests that whitelist decisions are recorded for the entire domain of a URL.
TEST_F(SafeBrowsingUrlAllowListTest, DomainWhitelistDecisions) {
  const GURL url("http://www.chromium.test");
  const GURL url_with_path("http://www.chromium.test/path");
  SBThreatType threat_type = safe_browsing::SB_THREAT_TYPE_URL_MALWARE;

  // Insert a pending decision and verify that it is pending for other URLs from
  // the same domain.
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->AddPendingUnsafeNavigationDecision(url, threat_type);
  EXPECT_TRUE(allow_list()->IsUnsafeNavigationDecisionPending(url_with_path));

  // Whitelist the URL and verify that it is allowed for other URLs from the
  // same domain.
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->AllowUnsafeNavigations(url, threat_type);
  EXPECT_TRUE(allow_list()->AreUnsafeNavigationsAllowed(url_with_path));
}
