// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_key.h"

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "chrome/browser/profiles/profile_key_android.h"
#endif  // OS_ANDROID

ProfileKey::ProfileKey(const base::FilePath& path, ProfileKey* original_key)
    : SimpleFactoryKey(path, original_key != nullptr /* is_off_the_record */),
      prefs_(nullptr),
      original_key_(original_key) {}

ProfileKey::~ProfileKey() = default;

ProfileKey* ProfileKey::GetOriginalKey() {
  if (original_key_)
    return original_key_;
  return this;
}

PrefService* ProfileKey::GetPrefs() {
  DCHECK(prefs_);
  return prefs_;
}

void ProfileKey::SetPrefs(PrefService* prefs) {
  DCHECK(!prefs_);
  prefs_ = prefs;
}

// static
ProfileKey* ProfileKey::FromSimpleFactoryKey(SimpleFactoryKey* key) {
  return key ? static_cast<ProfileKey*>(key) : nullptr;
}

#if defined(OS_ANDROID)
ProfileKeyAndroid* ProfileKey::GetProfileKeyAndroid() {
  if (!profile_key_android_)
    profile_key_android_ = std::make_unique<ProfileKeyAndroid>(this);
  return profile_key_android_.get();
}
#endif  // OS_ANDROID
