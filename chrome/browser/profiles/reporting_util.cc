// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/reporting_util.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace reporting {

base::Value GetContext(Profile* profile) {
  base::Value context(base::Value::Type::DICTIONARY);
  context.SetStringPath("browser.userAgent", GetUserAgent());

  if (!profile)
    return context;

  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry = nullptr;
  if (storage.GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
    context.SetStringPath("profile.profileName", entry->GetName());
    context.SetStringPath("profile.gaiaEmail", entry->GetUserName());
  }

  context.SetStringPath("profile.profilePath", profile->GetPath().value());

  return context;
}

}  // namespace reporting
