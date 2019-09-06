// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_MANAGER_OBSERVER_H_

#include "base/observer_list_types.h"

class Profile;

class ProfileManagerObserver : public base::CheckedObserver {
 public:
  // Called when a profile is added to the manager. The profile is fully created
  // and registered with the ProfileManager.
  virtual void OnProfileAdded(Profile* profile) {}

  // Called when the user deletes a profile and all associated data should be
  // erased. Note that the Profile object will not be destroyed until Chrome
  // shuts down. See https://crbug.com/88586
  virtual void OnProfileMarkedForPermanentDeletion(Profile* profile) {}
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_MANAGER_OBSERVER_H_
