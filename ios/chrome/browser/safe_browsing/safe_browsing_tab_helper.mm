// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/safe_browsing_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/features.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_error.h"
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
    const std::string& method,
    base::OnceCallback<void(web::WebStatePolicyDecider::PolicyDecision)>
        callback) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  safe_browsing::SafeBrowsingUrlCheckerImpl* url_checker_ptr =
      url_checker.get();
  active_url_checkers_[std::move(url_checker)] = std::move(callback);
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
  web::WebStatePolicyDecider::PolicyDecision policy_decision =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  if (showed_interstitial) {
    policy_decision =
        web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(
            [NSError errorWithDomain:kSafeBrowsingErrorDomain
                                code:kUnsafeResourceErrorCode
                            userInfo:nil]);
  } else if (!proceed) {
    policy_decision = web::WebStatePolicyDecider::PolicyDecision::Cancel();
  }

  auto it = active_url_checkers_.find(url_checker);
  base::PostTask(FROM_HERE, {web::WebThread::UI},
                 base::BindOnce(std::move(it->second), policy_decision));

  active_url_checkers_.erase(it);
}

#pragma mark - SafeBrowsingTabHelper::PolicyDecider

SafeBrowsingTabHelper::PolicyDecider::PolicyDecider(
    web::WebState* web_state,
    UrlCheckerClient* url_checker_client)
    : web::WebStatePolicyDecider(web_state),
      url_checker_client_(url_checker_client) {}

SafeBrowsingTabHelper::PolicyDecider::~PolicyDecider() = default;

web::WebStatePolicyDecider::PolicyDecision
SafeBrowsingTabHelper::PolicyDecider::ShouldAllowRequest(
    NSURLRequest* request,
    const web::WebStatePolicyDecider::RequestInfo& request_info) {
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  GURL request_url = net::GURLWithNSURL(request.URL);
  if (!safe_browsing_service->CanCheckUrl(request_url))
    return web::WebStatePolicyDecider::PolicyDecision::Allow();

  safe_browsing::ResourceType resource_type =
      request_info.target_frame_is_main
          ? safe_browsing::ResourceType::kMainFrame
          : safe_browsing::ResourceType::kSubFrame;
  std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> url_checker =
      safe_browsing_service->CreateUrlChecker(resource_type, web_state());

  std::string method = base::SysNSStringToUTF8([request HTTPMethod]);

  base::OnceCallback<void(web::WebStatePolicyDecider::PolicyDecision)>
      callback = base::Bind(&PolicyDecider::OnUrlQueryDecided, AsWeakPtr(),
                            request_url, request_info.target_frame_is_main);
  base::PostTask(
      FROM_HERE, {web::WebThread::IO},
      base::BindOnce(&UrlCheckerClient::CheckUrl,
                     url_checker_client_->AsWeakPtr(), std::move(url_checker),
                     request_url, method, std::move(callback)));

  // TODO(crbug.com/1049340): Also keep track of queries for iframes, and
  // use the results from those queries to decide whether to show an error
  // page.
  if (request_info.target_frame_is_main) {
    // Clear stale queries for |url|, so that their decisions are not re-used.
    while (!pending_main_frame_queries_.empty() &&
           pending_main_frame_queries_.front().url == request_url) {
      DCHECK(pending_main_frame_queries_.front().response_callback.is_null());
      pending_main_frame_queries_.pop_front();
    }
    PendingUrlQuery pending_query(request_url);
    pending_main_frame_queries_.push_back(std::move(pending_query));
  }

  return web::WebStatePolicyDecider::PolicyDecision::Allow();
}

void SafeBrowsingTabHelper::PolicyDecider::ShouldAllowResponse(
    NSURLResponse* response,
    bool for_main_frame,
    base::OnceCallback<void(web::WebStatePolicyDecider::PolicyDecision)>
        callback) {
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  GURL response_url = net::GURLWithNSURL(response.URL);
  if (!for_main_frame || !safe_browsing_service->CanCheckUrl(response_url)) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  while (pending_main_frame_queries_.front().url != response_url) {
    // Since |pending_main_frame_queries_| maintains the same order as calls
    // to ShouldAllowRequest(), all URLs that appear in this list before
    // |response_url| are for requests that happened before the request with
    // |response_url|, and were therefore either:
    // (1) cancelled by other policy deciders
    // (2) cancelled by the start of the navigation to |response_url|
    // (3) redirected to |response_url| by a server redirect
    // This means there will not be any future ShouldAllowResponse() call for
    // these URLs, so it is safe to delete these queries. We still have to
    // run any stored response_callbacks for these queries, since WebKit fires
    // an exception when a navigation response callback is destroyed without
    // being called, even if this callback no longer has any effect.
    // TODO(crbug.com/1049340): For case (3) above, keep track of the redirect
    // chain so that an unsafe result for any URL in the chain causes the
    // entire chain to be treated as unsafe.
    if (!pending_main_frame_queries_.front().response_callback.is_null()) {
      std::move(pending_main_frame_queries_.front().response_callback)
          .Run(web::WebStatePolicyDecider::PolicyDecision::Cancel());
    }
    pending_main_frame_queries_.pop_front();
  }

  DCHECK(!pending_main_frame_queries_.empty());

  if (pending_main_frame_queries_.front().decision) {
    // This query is left in the list rather than being popped, because it's
    // possible to get another call to ShouldAllowResponse for the same
    // URL without another call to ShouldAllowRequest. This is because WebKit
    // sometimes skips asking whether to allow a request that has the same
    // URL as the immediately preceding request, but will still ask whether to
    // allow the response.
    std::move(callback).Run(*(pending_main_frame_queries_.front().decision));
  } else {
    pending_main_frame_queries_.front().response_callback = std::move(callback);
  }
}

void SafeBrowsingTabHelper::PolicyDecider::OnUrlQueryDecided(
    const GURL& url,
    bool for_main_frame,
    web::WebStatePolicyDecider::PolicyDecision decision) {
  if (!for_main_frame)
    return;
  for (auto it = pending_main_frame_queries_.begin();
       it != pending_main_frame_queries_.end(); ++it) {
    if (it->url == url) {
      it->decision = decision;

      // If ShouldAllowResponse() has already been called for this URL, invoke
      // its callback with the decision.
      if (!it->response_callback.is_null())
        std::move(it->response_callback).Run(decision);
    }
  }
}

#pragma mark - SafeBrowsingTabHelper::PolicyDecider::PendingUrlQuery

SafeBrowsingTabHelper::PolicyDecider::PendingUrlQuery::PendingUrlQuery(
    const GURL& url)
    : url(url) {}

SafeBrowsingTabHelper::PolicyDecider::PendingUrlQuery::PendingUrlQuery(
    PendingUrlQuery&& query) = default;

SafeBrowsingTabHelper::PolicyDecider::PendingUrlQuery::~PendingUrlQuery() =
    default;
