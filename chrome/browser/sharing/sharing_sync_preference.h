// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_SYNC_PREFERENCE_H_
#define CHROME_BROWSER_SHARING_SHARING_SYNC_PREFERENCE_H_

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;

// SharingSyncPreference manages all preferences related to Sharing using Sync,
// such as storing list of user devices synced via Chrome and VapidKey used
// for authentication.
class SharingSyncPreference {
 public:
  struct Device {
    Device(std::string fcm_token,
           std::string p256dh,
           std::string auth_secret,
           const int capabilities);
    Device(Device&& other);
    ~Device();

    // FCM registration token of device for sending SharingMessage.
    std::string fcm_token;

    // Subscription public key required for WebPush protocol.
    std::string p256dh;

    // Auth secret key required for WebPush protocol.
    std::string auth_secret;

    // Bitmask of capabilities, defined in SharingDeviceCapability enum, that
    // are supported by the device.
    int capabilities;
  };

  // FCM registration status of current device. Not synced across devices.
  struct FCMRegistration {
    // Authorized entity registered with FCM.
    std::string authorized_entity;

    // FCM registration token of the device.
    std::string fcm_token;

    // Timestamp of latest registration.
    base::Time timestamp;
  };

  explicit SharingSyncPreference(PrefService* prefs);
  ~SharingSyncPreference();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns a copy of |local_value| to sync if its timestamp is older than the
  // one of |server_value|. Otherwise returns nullptr which means that we pick
  // the |server_value| as is.
  static std::unique_ptr<base::Value> MaybeMergeVapidKey(
      const base::Value& local_value,
      const base::Value& server_value);

  // Returns a new dictionary with devices merged from both |local_value| and
  // |server_value| based on their last modified timestamp. May return nullptr
  // if we should just pick |server_value| as is.
  static std::unique_ptr<base::Value> MaybeMergeSyncedDevices(
      const base::Value& local_value,
      const base::Value& server_value);

  // Returns VAPID key from preferences if present, otherwise returns
  // base::nullopt.
  // For more information on vapid keys, please see
  // https://tools.ietf.org/html/draft-thomson-webpush-vapid-02
  base::Optional<std::vector<uint8_t>> GetVapidKey() const;

  // Adds VAPID key to preferences for syncing across devices.
  void SetVapidKey(const std::vector<uint8_t>& vapid_key) const;

  // Observes for VAPID key changes. Replaces previously set observer.
  void SetVapidKeyChangeObserver(const base::RepeatingClosure& obs);

  // Clears previously set observer.
  void ClearVapidKeyChangeObserver();

  // Returns the map of guid to device from sharing preferences. Guid is same
  // as sync device guid.
  std::map<std::string, Device> GetSyncedDevices() const;

  // Stores |device| with key |guid| in sharing preferences.
  // |guid| is same as sync device guid.
  void SetSyncDevice(const std::string& guid, const Device& device);

  // Removes device corresponding to |guid| from sharing preferences.
  // |guid| is same as sync device guid.
  void RemoveDevice(const std::string& guid);

  base::Optional<FCMRegistration> GetFCMRegistration() const;

  void SetFCMRegistration(FCMRegistration registration);

  void ClearFCMRegistration();

 private:
  friend class SharingSyncPreferenceTest;

  static base::Value DeviceToValue(const Device& device, base::Time timestamp);

  static base::Optional<Device> ValueToDevice(const base::Value& value);

  PrefService* prefs_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(SharingSyncPreference);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_SYNC_PREFERENCE_H_
