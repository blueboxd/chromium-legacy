// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_SHARING_DEVICE_SHARING_MANAGER_IMPL_H_
#define IOS_CHROME_BROWSER_DEVICE_SHARING_DEVICE_SHARING_MANAGER_IMPL_H_

#import <memory>

#include "base/gtest_prod_util.h"
#import "ios/chrome/browser/device_sharing/device_sharing_manager.h"

class ChromeBrowserState;
class PrefChangeRegistrar;
@class HandoffManager;

class DeviceSharingManagerImpl : public DeviceSharingManager {
 public:
  explicit DeviceSharingManagerImpl(ChromeBrowserState* browser_state);

  // Not copyable or moveable.
  DeviceSharingManagerImpl(const DeviceSharingManagerImpl&) = delete;
  DeviceSharingManagerImpl& operator=(const DeviceSharingManagerImpl&) = delete;
  ~DeviceSharingManagerImpl() override;

  void UpdateActiveUrl(const GURL& active_url) override;
  void ClearActiveUrl() override;

 private:
  // Allow tests to inspect the handoff manager.
  friend class DeviceSharingManagerImplTest;
  friend class DeviceSharingAppInterfaceWrapper;

  void UpdateHandoffManager();

  ChromeBrowserState* browser_state_;

  // Registrar for pref change notifications to the active browser state.
  std::unique_ptr<PrefChangeRegistrar> prefs_change_observer_;

  // Responsible for maintaining all state related to the Handoff feature.
  __strong HandoffManager* handoff_manager_;
};

#endif  // IOS_CHROME_BROWSER_DEVICE_SHARING_DEVICE_SHARING_MANAGER_IMPL_H_
