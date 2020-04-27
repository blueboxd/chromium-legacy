// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_URL_ALLOW_LIST_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_URL_ALLOW_LIST_H_

#include <map>
#include <set>

#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#import "ios/web/public/web_state_user_data.h"
#include "url/gurl.h"

// SafeBrowsingUrlAllowList tracks the whitelist decisions for URLs for a given
// threat type, as well as decisions that are pending.  Decisions are stored for
// URLs with empty paths, meaning that whitelisted threats are allowed for the
// entire domain.
class SafeBrowsingUrlAllowList
    : public web::WebStateUserData<SafeBrowsingUrlAllowList> {
 public:
  // SafeBrowsingUrlAllowList is move-only.
  SafeBrowsingUrlAllowList(SafeBrowsingUrlAllowList&& other);
  SafeBrowsingUrlAllowList& operator=(SafeBrowsingUrlAllowList&& other);
  ~SafeBrowsingUrlAllowList() override;

  // Returns whether unsafe navigations to |url| are allowed.  If |threat_types|
  // is non-null, it is populated with the allowed threat types.
  bool AreUnsafeNavigationsAllowed(
      const GURL& url,
      std::set<safe_browsing::SBThreatType>* threat_types = nullptr);

  // Allows future unsafe navigations to |url| that encounter threats with
  // |threat_type|.
  void AllowUnsafeNavigations(const GURL& url,
                              safe_browsing::SBThreatType threat_type);

  // Prohibits all previously allowed navigations for |url|.
  void DisallowUnsafeNavigations(const GURL& url);

  // Returns whether there are pending unsafe navigation decisions for |url|.
  // If |threat_types| is non-null, it is populated with the pending threat
  // types.
  bool IsUnsafeNavigationDecisionPending(
      const GURL& url,
      std::set<safe_browsing::SBThreatType>* threat_types = nullptr);

  // Records that a navigation to |url| has encountered |threat_type|, but the
  // user has not yet chosen whether to allow the navigation.
  void AddPendingUnsafeNavigationDecision(
      const GURL& url,
      safe_browsing::SBThreatType threat_type);

  // Removes all pending decisions for |url|.
  void RemovePendingUnsafeNavigationDecisions(const GURL& url);

 private:
  explicit SafeBrowsingUrlAllowList(web::WebState* web_state);
  friend class web::WebStateUserData<SafeBrowsingUrlAllowList>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // Struct storing the threat types that have been allowed and those for
  // which the user has not made a decision yet.
  struct UnsafeNavigationDecisions {
    UnsafeNavigationDecisions();
    ~UnsafeNavigationDecisions();
    std::set<safe_browsing::SBThreatType> allowed_threats;
    std::set<safe_browsing::SBThreatType> pending_threats;
  };

  // Returns a reference to the UnsafeNavigationDecisions for |url|.  The path
  // is stripped from the URLs before accessing |decisions_| to allow unafe
  // navigation decisions to be shared across all URLs for a given domain.
  UnsafeNavigationDecisions& GetUnsafeNavigationDecisions(const GURL& url);

  // The WebState whose allowed navigations are recorded by this list.
  web::WebState* web_state_ = nullptr;
  // Map storing the whitelist decisions for each URL.
  std::map<GURL, UnsafeNavigationDecisions> decisions_;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_URL_ALLOW_LIST_H_
