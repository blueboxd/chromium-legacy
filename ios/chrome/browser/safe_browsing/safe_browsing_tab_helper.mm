// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/safe_browsing_tab_helper.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/core/features.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/safe_browsing/safe_browsing_service.h"
#include "ios/web/public/thread/web_task_traits.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - SafeBrowsingTabHelper

SafeBrowsingTabHelper::SafeBrowsingTabHelper(web::WebState* web_state) {
  DCHECK(
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingAvailableOnIOS));

  // Unit tests that use a TestingApplicationContext don't have a
  // SafeBrowsingService.
  // TODO(crbug.com/1060300): Create a FakeSafeBrowsingService and use it in
  // TestingApplicationContext, so that a special case for tests isn't needed
  // here.
  if (!GetApplicationContext()->GetSafeBrowsingService())
    return;

  database_client_ = std::make_unique<SafeBrowsingTabHelper::DatabaseClient>(
      GetApplicationContext()->GetSafeBrowsingService()->GetDatabaseManager());
  policy_decider_ = std::make_unique<SafeBrowsingTabHelper::PolicyDecider>(
      web_state, database_client_.get());
}

SafeBrowsingTabHelper::~SafeBrowsingTabHelper() {
  if (database_client_) {
    base::DeleteSoon(FROM_HERE, {web::WebThread::IO},
                     database_client_.release());
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(SafeBrowsingTabHelper)

#pragma mark - SafeBrowsingTabHelper::DatabaseClient

SafeBrowsingTabHelper::DatabaseClient::DatabaseClient(
    safe_browsing::SafeBrowsingDatabaseManager* database_manager)
    : database_manager_(database_manager),
      threat_types_(safe_browsing::CreateSBThreatTypeSet(
          {safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
           safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
           safe_browsing::SB_THREAT_TYPE_URL_UNWANTED,
           safe_browsing::SB_THREAT_TYPE_BILLING})) {}

SafeBrowsingTabHelper::DatabaseClient::~DatabaseClient() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  database_manager_->CancelCheck(this);
}

void SafeBrowsingTabHelper::DatabaseClient::CheckUrl(const GURL& url) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  database_manager_->CheckBrowseUrl(url, threat_types_, this);
}

#pragma mark - SafeBrowsingTabHelper::PolicyDecider

SafeBrowsingTabHelper::PolicyDecider::PolicyDecider(
    web::WebState* web_state,
    SafeBrowsingTabHelper::DatabaseClient* database_client)
    : web::WebStatePolicyDecider(web_state),
      database_client_(database_client) {}

bool SafeBrowsingTabHelper::PolicyDecider::ShouldAllowRequest(
    NSURLRequest* request,
    const web::WebStatePolicyDecider::RequestInfo& request_info) {
  if (!database_client_)
    return true;

  GURL request_url = net::GURLWithNSURL(request.URL);
  base::PostTask(
      FROM_HERE, {web::WebThread::IO},
      base::BindOnce(&SafeBrowsingTabHelper::DatabaseClient::CheckUrl,
                     base::Unretained(database_client_), request_url));

  return true;
}
