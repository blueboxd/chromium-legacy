// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_FAKE_LOCAL_DEVICE_INFO_PROVIDER_H_
#define CHROME_BROWSER_SHARING_FAKE_LOCAL_DEVICE_INFO_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/local_device_info_provider.h"

class FakeLocalDeviceInfoProvider : public syncer::LocalDeviceInfoProvider {
 public:
  FakeLocalDeviceInfoProvider();
  ~FakeLocalDeviceInfoProvider() override;

  // Overrides for syncer::LocalDeviceInfoProvider.
  version_info::Channel GetChannel() const override;
  const syncer::DeviceInfo* GetLocalDeviceInfo() const override;
  std::unique_ptr<Subscription> RegisterOnInitializedCallback(
      const base::RepeatingClosure& callback) override;

 private:
  syncer::DeviceInfo device_info_;

  DISALLOW_COPY_AND_ASSIGN(FakeLocalDeviceInfoProvider);
};

#endif  // CHROME_BROWSER_SHARING_FAKE_LOCAL_DEVICE_INFO_PROVIDER_H_
