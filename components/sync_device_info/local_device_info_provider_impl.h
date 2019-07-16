// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_IMPL_H_
#define COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_IMPL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "components/version_info/version_info.h"

namespace syncer {

class LocalDeviceInfoProviderImpl : public MutableLocalDeviceInfoProvider {
 public:
  using SigninScopedDeviceIdCallback = base::RepeatingCallback<std::string()>;
  using SendTabToSelfReceivingEnabledCallback = base::RepeatingCallback<bool()>;

  LocalDeviceInfoProviderImpl(
      version_info::Channel channel,
      const std::string& version,
      const SigninScopedDeviceIdCallback& signin_scoped_device_id_callback,
      const SendTabToSelfReceivingEnabledCallback&
          send_tab_to_self_receiving_enabled_callback);
  ~LocalDeviceInfoProviderImpl() override;

  // MutableLocalDeviceInfoProvider implementation.
  void Initialize(const std::string& cache_guid,
                  const std::string& session_name) override;
  void Clear() override;
  version_info::Channel GetChannel() const override;
  const DeviceInfo* GetLocalDeviceInfo() const override;
  std::unique_ptr<Subscription> RegisterOnInitializedCallback(
      const base::RepeatingClosure& callback) override;

 private:
  // The channel (CANARY, DEV, BETA, etc.) of the current client.
  const version_info::Channel channel_;

  // The version string for the current client.
  const std::string version_;

  const SigninScopedDeviceIdCallback signin_scoped_device_id_callback_;
  const SendTabToSelfReceivingEnabledCallback
      send_tab_to_self_receiving_enabled_callback_;

  std::unique_ptr<DeviceInfo> local_device_info_;
  base::CallbackList<void(void)> callback_list_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LocalDeviceInfoProviderImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LocalDeviceInfoProviderImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_IMPL_H_
