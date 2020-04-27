// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/unique_id_tab_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UniqueIDTabHelper::~UniqueIDTabHelper() = default;

uint32_t UniqueIDTabHelper::GetNextAvailableRendererID() {
  return next_available_renderer_id_;
}

void UniqueIDTabHelper::SetNextAvailableRendererID(uint32_t new_id) {
  next_available_renderer_id_ = new_id;
}

UniqueIDTabHelper::UniqueIDTabHelper(web::WebState* web_state) {
  web_state->AddObserver(this);
}

void UniqueIDTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}

WEB_STATE_USER_DATA_KEY_IMPL(UniqueIDTabHelper)
