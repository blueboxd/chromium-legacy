// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/well_known_change_password_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// .well-known/change-password is a defined standard that points to the sites
// change password form. https://wicg.github.io/change-password-url/
bool IsWellKnownChangePasswordUrl(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS() &&
         (url.path() == "/.well-known/change-password" ||
          url.path() == "/.well-known/change-password/");
}

}

namespace password_manager {

WellKnownChangePasswordTabHelper::WellKnownChangePasswordTabHelper(
    web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state), web_state_(web_state) {
  web_state->AddObserver(this);
}

WellKnownChangePasswordTabHelper::~WellKnownChangePasswordTabHelper() = default;

void WellKnownChangePasswordTabHelper::DidRedirectNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // TODO(crbug.com/927473): handle redirects
}

web::WebStatePolicyDecider::PolicyDecision
WellKnownChangePasswordTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    const RequestInfo& request_info) {
  const GURL& request_url = net::GURLWithNSURL(request.URL);
  // Boolean order important. First url then feature flag. Otherwise it messes
  // with usage statistics of control and experimental group (UMA).
  if (request_info.target_frame_is_main &&
      IsWellKnownChangePasswordUrl(request_url) &&
      base::FeatureList::IsEnabled(
          password_manager::features::kWellKnownChangePassword)) {
    // TODO(crbug.com/927473): Make request to non existing resource and check
    // if well-known/change-password is supported.
  }
  return web::WebStatePolicyDecider::PolicyDecision::Allow();
}

void WellKnownChangePasswordTabHelper::ShouldAllowResponse(
    NSURLResponse* response,
    bool for_main_frame,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  const GURL& url = net::GURLWithNSURL(response.URL);
  // Boolean order important, feature flag check last. Otherwise it messes
  // with usage statistics.
  // We only want to handle main_frame requests to keep consistency with the
  // NavigationThrottle implementation.
  if (!for_main_frame || !IsWellKnownChangePasswordUrl(url) ||
      !base::FeatureList::IsEnabled(
          password_manager::features::kWellKnownChangePassword)) {
    // TODO(crbug.com/927473): Handle .well-known/change-passord response.
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }
  // TODO(crbug.com/927473): Handle Response
  std::move(callback).Run(web::WebStatePolicyDecider::PolicyDecision::Allow());
}

void WellKnownChangePasswordTabHelper::WebStateDestroyed() {}

void WellKnownChangePasswordTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  web_state->RemoveObserver(this);
}

WEB_STATE_USER_DATA_KEY_IMPL(WellKnownChangePasswordTabHelper)

}
