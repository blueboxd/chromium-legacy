// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"

#include "ios/chrome/browser/infobars/infobar_badge_model.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper_delegate.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/infobar_ui_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - Public

// static
void InfobarBadgeTabHelper::CreateForWebState(web::WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(), base::WrapUnique(new InfobarBadgeTabHelper(web_state)));
  }
}

void InfobarBadgeTabHelper::SetDelegate(
    id<InfobarBadgeTabHelperDelegate> delegate) {
  delegate_ = delegate;
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarAccepted(
    InfobarType infobar_type) {
  NSNumber* infobar_type_key = GetNumberKeyForInfobarType(infobar_type);
  infobar_badge_models_[infobar_type_key].badgeState |=
      BadgeStateAccepted | BadgeStateRead;
  [delegate_ updateInfobarBadge:infobar_badge_models_[infobar_type_key]];
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarReverted(
    InfobarType infobar_type) {
  NSNumber* infobar_type_key = GetNumberKeyForInfobarType(infobar_type);
  infobar_badge_models_[infobar_type_key].badgeState &= ~BadgeStateAccepted;
  [delegate_ updateInfobarBadge:infobar_badge_models_[infobar_type_key]];
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarBannerPresented(
    InfobarType infobar_type) {
  NSNumber* infobar_type_key = GetNumberKeyForInfobarType(infobar_type);
  infobar_badge_models_[infobar_type_key].badgeState |= BadgeStatePresented;
  [delegate_ updateInfobarBadge:infobar_badge_models_[infobar_type_key]];
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarBannerDismissed(
    InfobarType infobar_type) {
  NSNumber* infobar_type_key = GetNumberKeyForInfobarType(infobar_type);
  infobar_badge_models_[infobar_type_key].badgeState &= ~BadgeStatePresented;
  [delegate_ updateInfobarBadge:infobar_badge_models_[infobar_type_key]];
}

NSArray<id<BadgeItem>>* InfobarBadgeTabHelper::GetInfobarBadgeItems() {
  return [infobar_badge_models_ allValues];
}

InfobarBadgeTabHelper::~InfobarBadgeTabHelper() = default;

#pragma mark - Private

InfobarBadgeTabHelper::InfobarBadgeTabHelper(web::WebState* web_state)
    : infobar_observer_(this) {
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(web_state);
  if (infoBarManager) {
    infobar_observer_.Add(infoBarManager);
  }
  // Since the TabHelper is being created, |infobar_badge_models_| needs to be
  // properly initialized.
  infobar_badge_models_ = [NSMutableDictionary dictionary];
}

NSNumber* InfobarBadgeTabHelper::GetNumberKeyForInfobarType(
    InfobarType infobar_type) {
  NSNumber* infobar_type_key = [NSNumber numberWithInt:(int)infobar_type];
  DCHECK(infobar_type_key);
  return infobar_type_key;
}

#pragma mark InfobarObserver

void InfobarBadgeTabHelper::OnInfoBarAdded(infobars::InfoBar* infobar) {
  this->UpdateBadgeForInfobar(infobar, true);
}

void InfobarBadgeTabHelper::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                             bool animate) {
  this->UpdateBadgeForInfobar(infobar, false);
}

void InfobarBadgeTabHelper::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  infobar_observer_.Remove(manager);
}

#pragma mark Helpers

void InfobarBadgeTabHelper::UpdateBadgeForInfobar(infobars::InfoBar* infobar,
                                                  bool display) {
  InfoBarIOS* infobar_ios = static_cast<InfoBarIOS*>(infobar);
  id<InfobarUIDelegate> controller_ = infobar_ios->InfobarUIDelegate();
  if (IsInfobarUIRebootEnabled() && controller_.hasBadge) {
    InfobarType infobar_type = controller_.infobarType;
    if (display) {
      InfobarBadgeModel* new_badge =
          [[InfobarBadgeModel alloc] initWithInfobarType:infobar_type];
      NSNumber* infobar_type_key = [NSNumber numberWithInt:(int)infobar_type];
      infobar_badge_models_[infobar_type_key] = new_badge;
      [delegate_ addInfobarBadge:new_badge];
    } else {
      NSNumber* infobar_type_key = [NSNumber numberWithInt:(int)infobar_type];
      InfobarBadgeModel* removed_badge =
          infobar_badge_models_[infobar_type_key];
      [infobar_badge_models_ removeObjectForKey:infobar_type_key];
      [delegate_ removeInfobarBadge:removed_badge];
    }
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(InfobarBadgeTabHelper)
