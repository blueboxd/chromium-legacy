// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_helper.h"

#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"
#include "ios/components/security_interstitials/lookalikes/lookalike_url_tab_allow_list.h"
#import "ios/net/protocol_handler_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

LookalikeUrlTabHelper::~LookalikeUrlTabHelper() = default;

LookalikeUrlTabHelper::LookalikeUrlTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {}

web::WebStatePolicyDecider::PolicyDecision
LookalikeUrlTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    const web::WebStatePolicyDecider::RequestInfo& request_info) {
  // Ignore subframe navigations.
  if (!request_info.target_frame_is_main) {
    return web::WebStatePolicyDecider::PolicyDecision::Allow();
  }

  // Get stored interstitial parameters early. Doing so ensures that a
  // navigation to an irrelevant (for this interstitial's purposes) URL such as
  // chrome://settings while the lookalike interstitial is being shown clears
  // the stored state:
  // 1. User navigates to lookalike.tld which redirects to site.tld.
  // 2. Interstitial shown.
  // 3. User navigates to chrome://settings.
  // If, after this, the user somehow ends up on site.tld with a reload (e.g.
  // with ReloadType::ORIGINAL_REQUEST_URL), this will correctly not show an
  // interstitial.
  LookalikeUrlContainer* lookalike_container =
      LookalikeUrlContainer::FromWebState(web_state());
  std::unique_ptr<LookalikeUrlContainer::InterstitialParams>
      interstitial_params = lookalike_container->ReleaseInterstitialParams();

  GURL request_url = net::GURLWithNSURL(request.URL);

  // If the URL is not an HTTP or HTTPS page, don't show any warning.
  if (!request_url.SchemeIsHTTPOrHTTPS()) {
    return web::WebStatePolicyDecider::PolicyDecision::Allow();
  }

  // If the URL is in the allowlist, don't show any warning.
  LookalikeUrlTabAllowList* allow_list =
      LookalikeUrlTabAllowList::FromWebState(web_state());
  if (allow_list->IsDomainAllowed(request_url.host())) {
    return web::WebStatePolicyDecider::PolicyDecision::Allow();
  }

  const DomainInfo navigated_domain = GetDomainInfo(request_url);
  // Empty domain_and_registry happens on private domains.
  if (navigated_domain.domain_and_registry.empty() ||
      IsTopDomain(navigated_domain)) {
    return web::WebStatePolicyDecider::PolicyDecision::Allow();
  }

  // TODO(crbug.com/1058898): After site engagement has been componentized,
  // fetch and set |engaged_sites| here so that an interstitial won't be
  // shown on engaged sites, and so that the interstitial will be shown on
  // lookalikes of engaged sites.
  std::vector<DomainInfo> engaged_sites;
  std::string matched_domain;
  LookalikeUrlMatchType match_type;
  // Target allowlist is not currently used in ios.
  const LookalikeTargetAllowlistChecker in_target_allowlist =
      base::BindRepeating(^(const GURL& url) {
        return false;
      });
  if (!GetMatchingDomain(navigated_domain, engaged_sites, in_target_allowlist,
                         &matched_domain, &match_type)) {
    return web::WebStatePolicyDecider::PolicyDecision::Allow();
  }
  DCHECK(!matched_domain.empty());

  if (ShouldBlockLookalikeUrlNavigation(match_type, navigated_domain)) {
    // TODO(crbug.com/1058898): Use the below information to generate the
    // blocking page UI.
    const std::string suggested_domain = GetETLDPlusOne(matched_domain);
    DCHECK(!suggested_domain.empty());
    GURL::Replacements replace_host;
    replace_host.SetHostStr(suggested_domain);
    const GURL suggested_url =
        request_url.ReplaceComponents(replace_host).GetWithEmptyPath();

    // TODO(crbug.com/1058898): Instead of a net error, pass a custom NSError
    // for lookalikes. GetErrorPage will first need to be updated to accept
    // non-net errors.
    return web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(
        [NSError errorWithDomain:net::kNSErrorDomain
                            code:net::ERR_BLOCKED_BY_CLIENT
                        userInfo:nil]);
  }

  return web::WebStatePolicyDecider::PolicyDecision::Allow();
}

WEB_STATE_USER_DATA_KEY_IMPL(LookalikeUrlTabHelper)
