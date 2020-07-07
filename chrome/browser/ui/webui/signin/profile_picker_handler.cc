// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_picker_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/util/values/values_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image.h"

namespace {
const size_t kAvatarIconSize = 74;
}

ProfilePickerHandler::ProfilePickerHandler() = default;

ProfilePickerHandler::~ProfilePickerHandler() = default;

void ProfilePickerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "mainViewInitialize",
      base::BindRepeating(&ProfilePickerHandler::HandleMainViewInitialize,
                          base::Unretained(this)));
}

void ProfilePickerHandler::HandleMainViewInitialize(
    const base::ListValue* args) {
  AllowJavascript();
  PushProfilesList();
}

void ProfilePickerHandler::PushProfilesList() {
  FireWebUIListener("profiles-list-changed", GetProfilesList());
}

base::Value ProfilePickerHandler::GetProfilesList() {
  base::ListValue profiles_list;
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetAllProfilesAttributesSortedByName();
  for (const ProfileAttributesEntry* entry : entries) {
    // Don't show profiles still in the middle of being set up as new legacy
    // supervised users.
    if (entry->IsOmitted())
      continue;

    auto profile_entry = std::make_unique<base::DictionaryValue>();
    profile_entry->SetKey("profilePath",
                          util::FilePathToValue(entry->GetPath()));
    profile_entry->SetString("localProfileName", entry->GetLocalProfileName());
    // GAIA name can be empty, if the profile is not signed in to chrome.
    profile_entry->SetString("gaiaName", entry->GetGAIANameToDisplay());
    gfx::Image icon = profiles::GetSizedAvatarIcon(
        entry->GetAvatarIcon(), true, kAvatarIconSize, kAvatarIconSize);
    std::string icon_url = webui::GetBitmapDataUrl(icon.AsBitmap());
    profile_entry->SetString("avatarIcon", icon_url);
    profiles_list.Append(std::move(profile_entry));
  }
  return std::move(profiles_list);
}
