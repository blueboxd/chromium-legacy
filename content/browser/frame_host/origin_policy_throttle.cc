// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/origin_policy_throttle.h"
#include "content/public/browser/origin_policy_commands.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/mojom/origin_policy_manager.mojom.h"
#include "url/origin.h"

namespace content {

// Implement the public "API" from
// content/public/browser/origin_policy_commands.h:
void OriginPolicyAddExceptionFor(BrowserContext* browser_context,
                                 const GURL& url) {
  OriginPolicyThrottle::AddExceptionFor(browser_context, url);
}

// static
bool OriginPolicyThrottle::ShouldRequestOriginPolicy(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool origin_policy_enabled =
      base::FeatureList::IsEnabled(features::kOriginPolicy) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures);
  if (!origin_policy_enabled)
    return false;

  if (!url.SchemeIs(url::kHttpsScheme))
    return false;

  return true;
}

// static
std::unique_ptr<NavigationThrottle>
OriginPolicyThrottle::MaybeCreateThrottleFor(NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle);

  // We use presence of the origin policy request header to determine
  // whether we should create the throttle.
  if (!handle->GetRequestHeaders().HasHeader(
          net::HttpRequestHeaders::kSecOriginPolicy))
    return nullptr;

  // TODO(vogelheim): Rewrite & hoist up this DCHECK to ensure that ..HasHeader
  //     and ShouldRequestOriginPolicy are always equal on entry to the method.
  //     This depends on https://crbug.com/881234 being fixed.
  DCHECK(OriginPolicyThrottle::ShouldRequestOriginPolicy(handle->GetURL()));
  return base::WrapUnique(new OriginPolicyThrottle(handle));
}

// static
void OriginPolicyThrottle::AddExceptionFor(BrowserContext* browser_context,
                                           const GURL& url) {
  DCHECK(browser_context);
  StoragePartitionImpl* storage_partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetStoragePartitionForSite(browser_context, url));
  network::mojom::OriginPolicyManager* origin_policy_manager =
      storage_partition->GetOriginPolicyManagerForBrowserProcess();

  origin_policy_manager->AddExceptionFor(url::Origin::Create(url));
}

OriginPolicyThrottle::~OriginPolicyThrottle() {}

NavigationThrottle::ThrottleCheckResult
OriginPolicyThrottle::WillStartRequest() {
  // TODO(vogelheim): It might be faster in the common case to optimistically
  //     fetch the policy indicated in the request already here. This would
  //     be faster if the last known version is the current version, but
  //     slower (and wasteful of bandwidth) if the server sends us a new
  //     version. It's unclear how much the upside is, though.
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
OriginPolicyThrottle::WillProcessResponse() {
  DCHECK(navigation_handle());

  // Per spec, Origin Policies are only fetched for https:-requests. So we
  // should always have HTTP headers at this point.
  // However, some unit tests generate responses without headers, so we still
  // need to check.
  if (!navigation_handle()->GetResponseHeaders())
    return NavigationThrottle::PROCEED;

  std::string header;
  navigation_handle()->GetResponseHeaders()->GetNormalizedHeader(
      net::HttpRequestHeaders::kSecOriginPolicy, &header);

  network::mojom::OriginPolicyManager::RetrieveOriginPolicyCallback
      origin_policy_manager_done = base::BindOnce(
          &OriginPolicyThrottle::OnOriginPolicyManagerRetrieveDone,
          weak_factory_.GetWeakPtr());

  SiteInstance* site_instance = navigation_handle()->GetStartingSiteInstance();
  StoragePartitionImpl* storage_partition =
      static_cast<StoragePartitionImpl*>(BrowserContext::GetStoragePartition(
          site_instance->GetBrowserContext(), site_instance));
  network::mojom::OriginPolicyManager* origin_policy_manager =
      storage_partition->GetOriginPolicyManagerForBrowserProcess();

  origin_policy_manager->RetrieveOriginPolicy(
      GetRequestOrigin(), header, std::move(origin_policy_manager_done));

  return NavigationThrottle::DEFER;
}

const char* OriginPolicyThrottle::GetNameForLogging() {
  return "OriginPolicyThrottle";
}

OriginPolicyThrottle::OriginPolicyThrottle(NavigationHandle* handle)
    : NavigationThrottle(handle) {}

const url::Origin OriginPolicyThrottle::GetRequestOrigin() const {
  return url::Origin::Create(navigation_handle()->GetURL());
}

void OriginPolicyThrottle::CancelNavigation(network::OriginPolicyState state,
                                            const GURL& policy_url) {
  base::Optional<std::string> error_page =
      GetContentClient()->browser()->GetOriginPolicyErrorPage(
          state, navigation_handle());
  CancelDeferredNavigation(NavigationThrottle::ThrottleCheckResult(
      NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page));
}

void OriginPolicyThrottle::OnOriginPolicyManagerRetrieveDone(
    const network::OriginPolicy& origin_policy) {
  switch (origin_policy.state) {
    case network::OriginPolicyState::kCannotLoadPolicy:
    case network::OriginPolicyState::kInvalidRedirect:
      CancelNavigation(origin_policy.state, origin_policy.policy_url);
      return;

    case network::OriginPolicyState::kNoPolicyApplies:
      Resume();
      return;

    case network::OriginPolicyState::kLoaded:
      DCHECK(origin_policy.contents);
      static_cast<NavigationHandleImpl*>(navigation_handle())
          ->navigation_request()
          ->SetOriginPolicy(origin_policy);
      Resume();
      return;

    default:
      NOTREACHED();
  }
}

}  // namespace content
