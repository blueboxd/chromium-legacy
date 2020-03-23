// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_sharing/device_sharing_manager_impl.h"

#include "components/handoff/handoff_manager.h"
#import "components/handoff/pref_names_ios.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DeviceSharingManagerImpl::DeviceSharingManagerImpl(
    ChromeBrowserState* browser_state)
    : browser_state_(browser_state),
      prefs_change_observer_(std::make_unique<PrefChangeRegistrar>()) {
  DCHECK(!browser_state || !browser_state->IsOffTheRecord());
  prefs_change_observer_->Init(browser_state_->GetPrefs());
  prefs_change_observer_->Add(
      prefs::kIosHandoffToOtherDevices,
      base::Bind(&DeviceSharingManagerImpl::UpdateHandoffManager,
                 base::Unretained(this)));
  UpdateHandoffManager();
  ClearActiveUrl();
}

DeviceSharingManagerImpl::~DeviceSharingManagerImpl() {}

void DeviceSharingManagerImpl::UpdateActiveUrl(const GURL& active_url) {
  if (active_url.is_empty()) {
    ClearActiveUrl();
    return;
  }

  [handoff_manager_ updateActiveURL:active_url];
}

void DeviceSharingManagerImpl::ClearActiveUrl() {
  [handoff_manager_ updateActiveURL:GURL()];
}

void DeviceSharingManagerImpl::UpdateHandoffManager() {
  if (!browser_state_->GetPrefs()->GetBoolean(
          prefs::kIosHandoffToOtherDevices)) {
    handoff_manager_ = nil;
    return;
  }

  if (!handoff_manager_)
    handoff_manager_ = [[HandoffManager alloc] init];
}
