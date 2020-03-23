// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_SHARING_DEVICE_SHARING_MANAGER_H_
#define IOS_CHROME_BROWSER_DEVICE_SHARING_DEVICE_SHARING_MANAGER_H_

#include "components/keyed_service/core/keyed_service.h"

class GURL;

// This manager maintains all state related to sharing the active URL to other
// devices. It has the role of a dispatcher that shares the active URL to
// various internal sharing services (e.g. handoff).
class DeviceSharingManager : public KeyedService {
 public:
  DeviceSharingManager() = default;

  virtual void UpdateActiveUrl(const GURL& active_url) = 0;
  virtual void ClearActiveUrl() = 0;
};

#endif  // IOS_CHROME_BROWSER_DEVICE_SHARING_DEVICE_SHARING_MANAGER_H_
