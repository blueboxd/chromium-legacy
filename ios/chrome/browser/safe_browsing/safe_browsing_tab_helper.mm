// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/safe_browsing_tab_helper.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/features.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/safe_browsing/safe_browsing_service.h"
#include "ios/web/public/thread/web_task_traits.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - SafeBrowsingTabHelper

SafeBrowsingTabHelper::SafeBrowsingTabHelper(web::WebState* web_state)
    : url_checker_client_(std::make_unique<UrlCheckerClient>()),
      policy_decider_(web_state, url_checker_client_.get()) {
  DCHECK(
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingAvailableOnIOS));
}

SafeBrowsingTabHelper::~SafeBrowsingTabHelper() {
  base::DeleteSoon(FROM_HERE, {web::WebThread::IO},
                   url_checker_client_.release());
}

WEB_STATE_USER_DATA_KEY_IMPL(SafeBrowsingTabHelper)

#pragma mark - SafeBrowsingTabHelper::UrlCheckerClient

SafeBrowsingTabHelper::UrlCheckerClient::UrlCheckerClient() = default;

SafeBrowsingTabHelper::UrlCheckerClient::~UrlCheckerClient() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
}

void SafeBrowsingTabHelper::UrlCheckerClient::CheckUrl(
    std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> url_checker,
    const GURL& url,
    const std::string& method) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  safe_browsing::SafeBrowsingUrlCheckerImpl* url_checker_ptr =
      url_checker.get();
  active_url_checkers_.insert(std::move(url_checker));
  url_checker_ptr->CheckUrl(url, method,
                            base::BindOnce(&UrlCheckerClient::OnCheckUrlResult,
                                           AsWeakPtr(), url_checker_ptr));
}

void SafeBrowsingTabHelper::UrlCheckerClient::OnCheckUrlResult(
    safe_browsing::SafeBrowsingUrlCheckerImpl* url_checker,
    safe_browsing::SafeBrowsingUrlCheckerImpl::NativeUrlCheckNotifier*
        slow_check_notifier,
    bool proceed,
    bool showed_interstitial) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(url_checker);
  if (slow_check_notifier) {
    *slow_check_notifier = base::BindOnce(&UrlCheckerClient::OnCheckComplete,
                                          AsWeakPtr(), url_checker);
    return;
  }

  OnCheckComplete(url_checker, proceed, showed_interstitial);
}

void SafeBrowsingTabHelper::UrlCheckerClient::OnCheckComplete(
    safe_browsing::SafeBrowsingUrlCheckerImpl* url_checker,
    bool proceed,
    bool showed_interstitial) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(url_checker);
  active_url_checkers_.erase(url_checker);
}

#pragma mark - SafeBrowsingTabHelper::PolicyDecider

SafeBrowsingTabHelper::PolicyDecider::PolicyDecider(
    web::WebState* web_state,
    UrlCheckerClient* url_checker_client)
    : web::WebStatePolicyDecider(web_state),
      url_checker_client_(url_checker_client) {}

web::WebStatePolicyDecider::PolicyDecision
SafeBrowsingTabHelper::PolicyDecider::ShouldAllowRequest(
    NSURLRequest* request,
    const web::WebStatePolicyDecider::RequestInfo& request_info) {
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();

  safe_browsing::ResourceType resource_type =
      request_info.target_frame_is_main
          ? safe_browsing::ResourceType::kMainFrame
          : safe_browsing::ResourceType::kSubFrame;
  std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> url_checker =
      safe_browsing_service->CreateUrlChecker(resource_type, web_state());

  GURL request_url = net::GURLWithNSURL(request.URL);
  std::string method = base::SysNSStringToUTF8([request HTTPMethod]);

  base::PostTask(FROM_HERE, {web::WebThread::IO},
                 base::BindOnce(&UrlCheckerClient::CheckUrl,
                                url_checker_client_->AsWeakPtr(),
                                std::move(url_checker), request_url, method));

  return web::WebStatePolicyDecider::PolicyDecision::Allow();
}
