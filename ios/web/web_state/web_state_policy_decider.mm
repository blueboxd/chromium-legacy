// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/navigation/web_state_policy_decider.h"

#import "ios/web/public/web_state.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// static
WebStatePolicyDecider::PolicyDecision
WebStatePolicyDecider::PolicyDecision::Allow() {
  return WebStatePolicyDecider::PolicyDecision(
      WebStatePolicyDecider::PolicyDecision::Decision::kAllow);
}

// static
WebStatePolicyDecider::PolicyDecision
WebStatePolicyDecider::PolicyDecision::Cancel() {
  return WebStatePolicyDecider::PolicyDecision(
      WebStatePolicyDecider::PolicyDecision::Decision::kCancel);
}

bool WebStatePolicyDecider::PolicyDecision::ShouldAllowNavigation() const {
  return decision == WebStatePolicyDecider::PolicyDecision::Decision::kAllow;
}

bool WebStatePolicyDecider::PolicyDecision::ShouldCancelNavigation() const {
  return !ShouldAllowNavigation();
}

WebStatePolicyDecider::WebStatePolicyDecider(WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
  web_state_->AddPolicyDecider(this);
}

WebStatePolicyDecider::~WebStatePolicyDecider() {
  if (web_state_) {
    web_state_->RemovePolicyDecider(this);
  }
}

WebStatePolicyDecider::PolicyDecision WebStatePolicyDecider::ShouldAllowRequest(
    NSURLRequest* request,
    const WebStatePolicyDecider::RequestInfo& request_info) {
  return WebStatePolicyDecider::PolicyDecision::Allow();
}

bool WebStatePolicyDecider::ShouldAllowResponse(NSURLResponse* response,
                                                bool for_main_frame) {
  return true;
}

void WebStatePolicyDecider::ResetWebState() {
  web_state_->RemovePolicyDecider(this);
  web_state_ = nullptr;
}

}  // namespace web
