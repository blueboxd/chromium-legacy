// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_KEY_H_
#define CHROME_BROWSER_PROFILES_PROFILE_KEY_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/keyed_service/core/simple_factory_key.h"

class PrefService;

// An embryonic Profile with only fields accessible in reduced mode.
// Used as a SimpleFactoryKey.
class ProfileKey : public SimpleFactoryKey {
 public:
  ProfileKey(const base::FilePath& path,
             ProfileKey* original_key = nullptr);
  ~ProfileKey() override;

  // Profile-specific APIs needed in reduced mode:
  ProfileKey* GetOriginalKey();
  PrefService* GetPrefs();
  void SetPrefs(PrefService* prefs);

  static ProfileKey* FromSimpleFactoryKey(SimpleFactoryKey* key);

 private:
  PrefService* prefs_;

  // Points to the original (non off-the-record) ProfileKey.
  ProfileKey* original_key_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ProfileKey);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_KEY_H_
