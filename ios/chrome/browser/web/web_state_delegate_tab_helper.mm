// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/web_state_delegate_tab_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WEB_STATE_USER_DATA_KEY_IMPL(WebStateDelegateTabHelper)

WebStateDelegateTabHelper::WebStateDelegateTabHelper(web::WebState* web_state) {
}

WebStateDelegateTabHelper::~WebStateDelegateTabHelper() = default;

void WebStateDelegateTabHelper::OnAuthRequired(
    web::WebState* source,
    NSURLProtectionSpace* protection_space,
    NSURLCredential* proposed_credential,
    const web::WebStateDelegate::AuthCallback& callback) {
  AuthCallback local_callback(callback);
  local_callback.Run(nil, nil);
  // TODO(crbug.com/980101): Show HTTP authentication dialog using
  // OverlayPresenter.
}
