// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_DEVICE_REGISTRATION_H_
#define CHROME_BROWSER_SHARING_SHARING_DEVICE_REGISTRATION_H_

#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/gcm_driver/instance_id/instance_id.h"

namespace instance_id {
class InstanceIDDriver;
}

namespace syncer {
class LocalDeviceInfoProvider;
}

class SharingSyncPreference;
class VapidKeyManager;
enum class SharingDeviceRegistrationResult;

// Responsible for registering and unregistering device with
// SharingSyncPreference.
class SharingDeviceRegistration {
 public:
  using RegistrationCallback =
      base::OnceCallback<void(SharingDeviceRegistrationResult)>;

  SharingDeviceRegistration(
      SharingSyncPreference* prefs,
      instance_id::InstanceIDDriver* instance_id_driver,
      VapidKeyManager* vapid_key_manager,
      syncer::LocalDeviceInfoProvider* device_info_tracker);
  virtual ~SharingDeviceRegistration();

  // Registers device with sharing sync preferences. Takes a |callback| function
  // which receives the result of FCM registration for device.
  virtual void RegisterDevice(RegistrationCallback callback);

  // Un-registers device with sharing sync preferences.
  virtual void UnregisterDevice(RegistrationCallback callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(SharingDeviceRegistrationTest,
                           RegisterDeviceTest_Success);

  // Callback function responsible for validating FCM registration token and
  // retrieving public encryption key and authentication secret associated with
  // FCM App ID of Sharing. Also responsible for calling |callback| with
  // |result| of GetToken.
  void OnFCMTokenReceived(RegistrationCallback callback,
                          const std::string& authorized_entity,
                          const std::string& fcm_registration_token,
                          instance_id::InstanceID::Result result);

  // Callback function responsible for deleting FCM registration token
  // associated with FCM App ID of Sharing. Also responsible for calling
  // |callback| with |result| of DeleteToken.
  void OnFCMTokenDeleted(RegistrationCallback callback,
                         instance_id::InstanceID::Result result);

  // Retrieve encryption info from InstanceID.
  void RetrieveEncryptionInfo(RegistrationCallback callback,
                              const std::string& authorized_entity,
                              const std::string& fcm_registration_token);

  // Callback function responsible for saving device registration information in
  // SharingSyncPreference.
  void OnEncryptionInfoReceived(RegistrationCallback callback,
                                const std::string& fcm_registration_token,
                                std::string p256dh,
                                std::string auth_secret);

  // Returns the authorization entity for FCM registration.
  base::Optional<std::string> GetAuthorizationEntity() const;

  // Computes and returns a bitmask of all capabilities supported by the device.
  int GetDeviceCapabilities() const;

  // Returns if device supports telephony capability.
  bool IsTelephonySupported() const;

  SharingSyncPreference* sharing_sync_preference_;
  instance_id::InstanceIDDriver* instance_id_driver_;
  VapidKeyManager* vapid_key_manager_;
  syncer::LocalDeviceInfoProvider* local_device_info_provider_;

  base::WeakPtrFactory<SharingDeviceRegistration> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SharingDeviceRegistration);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_DEVICE_REGISTRATION_H_
