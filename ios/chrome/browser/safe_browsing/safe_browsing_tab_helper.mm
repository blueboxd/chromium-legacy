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
#import "ios/chrome/browser/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/thread/web_task_traits.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using safe_browsing::ResourceType;

namespace {
// Creates a PolicyDecision that cancels a navigation to show a safe browsing
// error.
web::WebStatePolicyDecider::PolicyDecision CreateSafeBrowsingErrorDecision() {
  return web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(
      [NSError errorWithDomain:kSafeBrowsingErrorDomain
                          code:kUnsafeResourceErrorCode
                      userInfo:nil]);
}
}  // namespace

#pragma mark - SafeBrowsingTabHelper

SafeBrowsingTabHelper::SafeBrowsingTabHelper(web::WebState* web_state)
    : url_checker_client_(std::make_unique<UrlCheckerClient>()),
      policy_decider_(web_state, url_checker_client_.get()),
      navigation_observer_(web_state, &policy_decider_) {
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
    policy_decision = CreateSafeBrowsingErrorDecision();
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

void SafeBrowsingTabHelper::PolicyDecider::MainFrameWasCommitted() {
  // Since a new main frame was committed, sub frame navigations from the
  // previous page are cancelled and their policy decisions can be erased.
  pending_sub_frame_queries_.clear();
}

#pragma mark web::WebStatePolicyDecider

web::WebStatePolicyDecider::PolicyDecision
SafeBrowsingTabHelper::PolicyDecider::ShouldAllowRequest(
    NSURLRequest* request,
    const web::WebStatePolicyDecider::RequestInfo& request_info) {
  // Allow navigations for URLs that cannot be checked by the service.
  GURL request_url = net::GURLWithNSURL(request.URL);
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  if (!safe_browsing_service->CanCheckUrl(request_url))
    return web::WebStatePolicyDecider::PolicyDecision::Allow();

  // Track all pending URL queries.
  bool is_main_frame = request_info.target_frame_is_main;
  if (is_main_frame) {
    pending_main_frame_query_ = MainFrameUrlQuery(request_url);
  } else if (pending_sub_frame_queries_.find(request_url) ==
             pending_sub_frame_queries_.end()) {
    pending_sub_frame_queries_.insert({request_url, SubFrameUrlQuery()});
  }

  // If there is a pre-existing main frame unsafe resource for |request_url|
  // that haven't yet resulted in an error page, this resource can be used to
  // show the error page for the current load.  This can occur in back/forward
  // navigations to safe browsing error pages, where ShouldAllowRequest() is
  // called multiple times consecutively for the same URL.
  SafeBrowsingUnsafeResourceContainer* unsafe_resource_container =
      SafeBrowsingUnsafeResourceContainer::FromWebState(web_state());
  if (is_main_frame && unsafe_resource_container) {
    const security_interstitials::UnsafeResource* main_frame_resource =
        unsafe_resource_container->GetMainFrameUnsafeResource();
    if (main_frame_resource && main_frame_resource->url == request_url) {
      // TODO(crbug.com/1064803): This should directly return the safe browsing
      // error decision once error pages for cancelled requests are supported.
      // For now, only cancelled response errors are displayed properly.
      pending_main_frame_query_->decision = CreateSafeBrowsingErrorDecision();
      return web::WebStatePolicyDecider::PolicyDecision::Allow();
    }
  }

  // Check to see if an error page should be displayed for a sub frame resource.
  web::NavigationManager* navigation_manager =
      web_state()->GetNavigationManager();
  web::NavigationItem* reloaded_item = navigation_manager->GetPendingItem();
  if (ui::PageTransitionCoreTypeIs(request_info.transition_type,
                                   ui::PAGE_TRANSITION_RELOAD) &&
      reloaded_item == navigation_manager->GetLastCommittedItem() &&
      unsafe_resource_container &&
      unsafe_resource_container->GetSubFrameUnsafeResource(reloaded_item)) {
    // Store the safe browsing error decision without re-checking the URL.
    // TODO(crbug.com/1064803): This should directly return the safe browsing
    // error decision once error pages for cancelled requests are supported.
    // For now, only cancelled response errors are displayed properly.
    pending_main_frame_query_->decision = CreateSafeBrowsingErrorDecision();
    return web::WebStatePolicyDecider::PolicyDecision::Allow();
  }

  // Create the URL checker and check for navigation safety on the IO thread.
  ResourceType resource_type =
      is_main_frame ? ResourceType::kMainFrame : ResourceType::kSubFrame;
  std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> url_checker =
      safe_browsing_service->CreateUrlChecker(resource_type, web_state());
  base::PostTask(
      FROM_HERE, {web::WebThread::IO},
      base::BindOnce(&UrlCheckerClient::CheckUrl,
                     url_checker_client_->AsWeakPtr(), std::move(url_checker),
                     request_url, base::SysNSStringToUTF8([request HTTPMethod]),
                     GetUrlCheckCallback(request_url, request_info)));

  // Allow all requests to continue.  If a safe browsing error is detected, the
  // navigation will be cancelled for using the response policy decision.
  return web::WebStatePolicyDecider::PolicyDecision::Allow();
}

void SafeBrowsingTabHelper::PolicyDecider::ShouldAllowResponse(
    NSURLResponse* response,
    bool for_main_frame,
    base::OnceCallback<void(web::WebStatePolicyDecider::PolicyDecision)>
        callback) {
  // Allow navigations for URLs that cannot be checked by the service.
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  GURL response_url = net::GURLWithNSURL(response.URL);
  if (!safe_browsing_service->CanCheckUrl(response_url)) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  if (for_main_frame) {
    HandleMainFrameResponsePolicy(response_url, std::move(callback));
  } else {
    HandleSubFrameResponsePolicy(response_url, std::move(callback));
  }
}

#pragma mark Response Policy Decision Helpers

void SafeBrowsingTabHelper::PolicyDecider::HandleMainFrameResponsePolicy(
    const GURL& url,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  DCHECK(pending_main_frame_query_);
  DCHECK_EQ(pending_main_frame_query_->url, url);
  auto& decision = pending_main_frame_query_->decision;
  if (decision) {
    std::move(callback).Run(*decision);
  } else {
    pending_main_frame_query_->response_callback = std::move(callback);
  }
}

void SafeBrowsingTabHelper::PolicyDecider::HandleSubFrameResponsePolicy(
    const GURL& url,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  // Sub frame response policy decisions are expected to always be requested
  // after a request policy decision for |url|.
  DCHECK(pending_sub_frame_queries_.find(url) !=
         pending_sub_frame_queries_.end());

  SubFrameUrlQuery& sub_frame_query = pending_sub_frame_queries_[url];
  if (sub_frame_query.decision) {
    std::move(callback).Run(*(sub_frame_query.decision));
  } else {
    sub_frame_query.response_callbacks.push_back(std::move(callback));
  }
}

#pragma mark URL Check Completion Helpers

web::WebStatePolicyDecider::PolicyDecisionCallback
SafeBrowsingTabHelper::PolicyDecider::GetUrlCheckCallback(
    const GURL& url,
    const web::WebStatePolicyDecider::RequestInfo& request_info) {
  if (request_info.target_frame_is_main) {
    return base::BindOnce(&PolicyDecider::OnMainFrameUrlQueryDecided,
                          AsWeakPtr(), url);
  } else {
    web::NavigationItem* navigation_item =
        web_state()->GetNavigationManager()->GetLastCommittedItem();
    return base::BindOnce(&PolicyDecider::OnSubFrameUrlQueryDecided,
                          AsWeakPtr(), url, navigation_item->GetUniqueID());
  }
}

void SafeBrowsingTabHelper::PolicyDecider::OnMainFrameUrlQueryDecided(
    const GURL& url,
    web::WebStatePolicyDecider::PolicyDecision decision) {
  // If the pending main frame URL query has been removed or replaced with one
  // for a new URL, ignore the result.
  if (!pending_main_frame_query_ || pending_main_frame_query_->url != url)
    return;

  pending_main_frame_query_->decision = decision;
  // If ShouldAllowResponse() has already been called for this URL, invoke
  // its callback with the decision.
  auto& response_callback = pending_main_frame_query_->response_callback;
  if (!response_callback.is_null())
    std::move(response_callback).Run(decision);
}

void SafeBrowsingTabHelper::PolicyDecider::OnSubFrameUrlQueryDecided(
    const GURL& url,
    int navigation_item_id,
    web::WebStatePolicyDecider::PolicyDecision decision) {
  web::NavigationManager* navigation_manager =
      web_state()->GetNavigationManager();
  web::NavigationItem* main_frame_item =
      navigation_manager->GetLastCommittedItem();

  // If the URL check for a sub frame completes after a new main frame has been
  // committed, ignore the result.
  if (navigation_item_id != main_frame_item->GetUniqueID())
    return;

  // The URL check is expected to have been registered for the sub frame.
  DCHECK(pending_sub_frame_queries_.find(url) !=
         pending_sub_frame_queries_.end());

  // Store the decision for |url| and run all the response callbacks that have
  // been received before the URL check completion.
  SubFrameUrlQuery& sub_frame_query = pending_sub_frame_queries_[url];
  sub_frame_query.decision = decision;
  for (auto& response_callback : sub_frame_query.response_callbacks) {
    std::move(response_callback).Run(decision);
  }
  sub_frame_query.response_callbacks.clear();

  // Error pages are only shown for cancelled main frame navigations, so
  // executing the sub frame response callbacks with the decision will not
  // actually show the safe browsing blocking page.  To trigger the blocking
  // page, reload the last committed item so that the stored sub frame unsafe
  // resource can be used to populate an error page in the main frame upon
  // reloading.
  if (decision.ShouldCancelNavigation() && decision.ShouldDisplayError()) {
    DCHECK(SafeBrowsingUnsafeResourceContainer::FromWebState(web_state())
               ->GetSubFrameUnsafeResource(main_frame_item));
    navigation_manager->DiscardNonCommittedItems();
    navigation_manager->Reload(web::ReloadType::NORMAL,
                               /*check_for_repost=*/false);
  }
}

#pragma mark SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery

SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery::MainFrameUrlQuery(
    const GURL& url)
    : url(url) {}

SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery::MainFrameUrlQuery(
    MainFrameUrlQuery&& query) = default;

SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery&
SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery::operator=(
    MainFrameUrlQuery&& other) = default;

SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery::~MainFrameUrlQuery() {
  if (!response_callback.is_null()) {
    std::move(response_callback)
        .Run(web::WebStatePolicyDecider::PolicyDecision::Cancel());
  }
}

#pragma mark SafeBrowsingTabHelper::PolicyDecider::SubFrameUrlQuery

SafeBrowsingTabHelper::PolicyDecider::SubFrameUrlQuery::SubFrameUrlQuery() =
    default;

SafeBrowsingTabHelper::PolicyDecider::SubFrameUrlQuery::SubFrameUrlQuery(
    SubFrameUrlQuery&& query) = default;

SafeBrowsingTabHelper::PolicyDecider::SubFrameUrlQuery::~SubFrameUrlQuery() {
  for (auto& response_callback : response_callbacks) {
    std::move(response_callback)
        .Run(web::WebStatePolicyDecider::PolicyDecision::Cancel());
  }
}

#pragma mark - SafeBrowsingTabHelper::NavigationObserver

SafeBrowsingTabHelper::NavigationObserver::NavigationObserver(
    web::WebState* web_state,
    PolicyDecider* policy_decider)
    : policy_decider_(policy_decider) {
  DCHECK(policy_decider_);
  scoped_observer_.Add(web_state);
}

SafeBrowsingTabHelper::NavigationObserver::~NavigationObserver() = default;

void SafeBrowsingTabHelper::NavigationObserver::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->HasCommitted())
    policy_decider_->MainFrameWasCommitted();
}

void SafeBrowsingTabHelper::NavigationObserver::WebStateDestroyed(
    web::WebState* web_state) {
  scoped_observer_.Remove(web_state);
}
