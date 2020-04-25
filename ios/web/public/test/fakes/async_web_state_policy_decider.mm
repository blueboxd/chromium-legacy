// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/async_web_state_policy_decider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

AsyncWebStatePolicyDecider::AsyncWebStatePolicyDecider(WebState* web_state)
    : WebStatePolicyDecider(web_state) {}

AsyncWebStatePolicyDecider::~AsyncWebStatePolicyDecider() = default;

void AsyncWebStatePolicyDecider::ShouldAllowResponse(
    NSURLResponse* response,
    bool for_main_frame,
    base::OnceCallback<void(PolicyDecision)> callback) {
  callback_ = std::move(callback);
}

bool AsyncWebStatePolicyDecider::ReadyToInvokeCallback() const {
  return !callback_.is_null();
}

void AsyncWebStatePolicyDecider::InvokeCallback(
    WebStatePolicyDecider::PolicyDecision decision) {
  std::move(callback_).Run(decision);
}

}  // namespace web
