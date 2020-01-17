// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/font_size_tab_helper.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/ui/util/dynamic_type_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FontSizeTabHelper::~FontSizeTabHelper() {
  // Remove observer in destructor because |this| is captured by the usingBlock
  // in calling [NSNotificationCenter.defaultCenter
  // addObserverForName:object:queue:usingBlock] in constructor.
  [NSNotificationCenter.defaultCenter
      removeObserver:content_size_did_change_observer_];
}

FontSizeTabHelper::FontSizeTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state->AddObserver(this);
  content_size_did_change_observer_ = [NSNotificationCenter.defaultCenter
      addObserverForName:UIContentSizeCategoryDidChangeNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* _Nonnull note) {
                SetPageFontSize(GetFontSize());
              }];
}

void FontSizeTabHelper::SetPageFontSize(int size) {
  if (web_state_->ContentIsHTML()) {
    NSString* js = [NSString
        stringWithFormat:@"__gCrWeb.accessibility.adjustFontSize(%d)", size];
    web_state_->ExecuteJavaScript(base::SysNSStringToUTF16(js));
  }
}

void FontSizeTabHelper::UserZoom(Zoom zoom) {
  // TODO(crbug.com/1028938): Replace with actual logic.
  return;
}

bool FontSizeTabHelper::CanUserZoomIn() const {
  // TODO(crbug.com/1028938): Replace with actual logic.
  return true;
}

bool FontSizeTabHelper::CanUserZoomOut() const {
  // TODO(crbug.com/1028938): Replace with actual logic.
  return true;
}

int FontSizeTabHelper::GetFontSize() const {
  // Multiply by 100 as the web property needs a percentage.
  return SystemSuggestedFontSizeMultiplier() * GetCurrentUserZoomMultiplier() *
         100;
}

double FontSizeTabHelper::GetCurrentUserZoomMultiplier() const {
  return 1;
}

void FontSizeTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}

void FontSizeTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK_EQ(web_state, web_state_);
  int size = GetFontSize();
  if (size != 100)
    SetPageFontSize(size);
}

WEB_STATE_USER_DATA_KEY_IMPL(FontSizeTabHelper)
