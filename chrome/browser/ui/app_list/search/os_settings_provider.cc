// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/os_settings_provider.h"

#include <string>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {
namespace {

constexpr char kOsSettingsResultPrefix[] = "os-settings://";

}  // namespace

OsSettingsResult::OsSettingsResult(float relevance, Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  set_id(kOsSettingsResultPrefix);
}

OsSettingsResult::~OsSettingsResult() = default;

void OsSettingsResult::Open(int event_flags) {}

ash::SearchResultType OsSettingsResult::GetSearchResultType() const {
  return ash::OS_SETTINGS;
}

OsSettingsProvider::OsSettingsProvider(Profile* profile) : profile_(profile) {
  DCHECK(profile_);
}

OsSettingsProvider::~OsSettingsProvider() = default;

void OsSettingsProvider::Start(const base::string16& query) {}

}  // namespace app_list
