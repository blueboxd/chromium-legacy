// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_VAPID_KEY_MANAGER_H_
#define CHROME_BROWSER_SHARING_VAPID_KEY_MANAGER_H_

#include <memory>

#include "base/macros.h"

namespace crypto {
class ECPrivateKey;
}  // namespace crypto

class SharingSyncPreference;

// Responsible for creating, storing and managing VAPID key. VAPID key is
// shared across all devices for a single user and is used for signing VAPID
// headers for Web Push.
// Web Push Protocol :
// https://developers.google.com/web/fundamentals/push-notifications/web-push-protocol
class VapidKeyManager {
 public:
  explicit VapidKeyManager(SharingSyncPreference* sharing_sync_preference);
  virtual ~VapidKeyManager();

  // Returns the shared VAPID key stored in SharingSyncPreference. If no key is
  // found in preferences, it generates a new key and stores in
  // SharingSyncPreference before returning this new key. Conflicts between
  // different devices generating the shared VAPID key is resolved based on
  // creation time.
  virtual crypto::ECPrivateKey* GetOrCreateKey();

 private:
  // Used for storing and fetching VAPID key from preferences.
  SharingSyncPreference* sharing_sync_preference_;

  std::unique_ptr<crypto::ECPrivateKey> vapid_key_;

  DISALLOW_COPY_AND_ASSIGN(VapidKeyManager);
};

#endif  // CHROME_BROWSER_SHARING_VAPID_KEY_MANAGER_H_
