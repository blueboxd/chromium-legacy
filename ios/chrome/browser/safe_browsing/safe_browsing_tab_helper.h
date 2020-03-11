// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_H_

#include "base/macros.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"

// A tab helper that uses Safe Browsing to check whether URLs that are being
// navigated to are unsafe.
class SafeBrowsingTabHelper
    : public web::WebStateUserData<SafeBrowsingTabHelper> {
 public:
  ~SafeBrowsingTabHelper() override;

  SafeBrowsingTabHelper(const SafeBrowsingTabHelper&) = delete;
  SafeBrowsingTabHelper& operator=(const SafeBrowsingTabHelper&) = delete;

 private:
  friend class web::WebStateUserData<SafeBrowsingTabHelper>;

  // A simple SafeBrowsingDatabaseManager::Client that queries the database but
  // doesn't do anything with the result yet. This class may be constructed on
  // the UI thread but otherwise must only be used and destroyed on the IO
  // thread.
  // TODO(crbug.com/1028755): Use the result of Safe Browsing queries to block
  // navigations that are identified as unsafe.
  class DatabaseClient
      : public safe_browsing::SafeBrowsingDatabaseManager::Client {
   public:
    explicit DatabaseClient(
        safe_browsing::SafeBrowsingDatabaseManager* database_manager);

    ~DatabaseClient() override;

    // Query the database with the given |url|.
    void CheckUrl(const GURL& url);

   private:
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;

    // The set of threat types that URLs are checked against.
    safe_browsing::SBThreatTypeSet threat_types_;
  };

  // A WebStatePolicyDecider that queries the SafeBrowsing database on each
  // request, but always allows the request.
  // TODO(crbug.com/1028755): Use the result of Safe Browsing queries to block
  // navigations that are identified as unsafe.
  class PolicyDecider : public web::WebStatePolicyDecider {
   public:
    PolicyDecider(web::WebState* web_state, DatabaseClient* database_client);

    // web::WebStatePolicyDecider implementation
    bool ShouldAllowRequest(
        NSURLRequest* request,
        const web::WebStatePolicyDecider::RequestInfo& request_info) override;

   private:
    DatabaseClient* database_client_;
  };

  explicit SafeBrowsingTabHelper(web::WebState* web_state);

  std::unique_ptr<DatabaseClient> database_client_;
  std::unique_ptr<PolicyDecider> policy_decider_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_H_
