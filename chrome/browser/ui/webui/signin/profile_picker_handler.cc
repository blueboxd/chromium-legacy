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
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/profile_picker.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image.h"

namespace {
const size_t kAvatarIconSize = 74;
}

ProfilePickerHandler::ProfilePickerHandler() = default;

ProfilePickerHandler::~ProfilePickerHandler() {
  OnJavascriptDisallowed();
}

void ProfilePickerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "mainViewInitialize",
      base::BindRepeating(&ProfilePickerHandler::HandleMainViewInitialize,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "launchSelectedProfile",
      base::BindRepeating(&ProfilePickerHandler::HandleLaunchSelectedProfile,
                          base::Unretained(this)));
}

void ProfilePickerHandler::OnJavascriptAllowed() {
  g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .AddObserver(this);
}
void ProfilePickerHandler::OnJavascriptDisallowed() {
  g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .RemoveObserver(this);
}

void ProfilePickerHandler::HandleMainViewInitialize(
    const base::ListValue* args) {
  AllowJavascript();
  PushProfilesList();
}

void ProfilePickerHandler::HandleLaunchSelectedProfile(
    const base::ListValue* args) {
  const base::Value* profile_path_value = nullptr;
  if (!args->Get(0, &profile_path_value))
    return;

  base::Optional<base::FilePath> profile_path =
      util::ValueToFilePath(*profile_path_value);
  if (!profile_path)
    return;

  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(*profile_path, &entry)) {
    NOTREACHED();
    return;
  }

  if (entry->IsSigninRequired()) {
    // The new profile picker does not yet support force signin policy and
    // should not be accessible for devices with this policy.
    NOTREACHED();
    return;
  }

  profiles::SwitchToProfile(
      *profile_path, /*always_create=*/false,
      base::Bind(&ProfilePickerHandler::OnSwitchToProfileComplete,
                 weak_factory_.GetWeakPtr()));
}

void ProfilePickerHandler::OnSwitchToProfileComplete(
    Profile* profile,
    Profile::CreateStatus profile_create_status) {
  Browser* browser = chrome::FindAnyBrowser(profile, false);
  DCHECK(browser);
  DCHECK(browser->window());
  ProfilePicker::Hide();
}

void ProfilePickerHandler::PushProfilesList() {
  DCHECK(IsJavascriptAllowed());
  FireWebUIListener("profiles-list-changed", GetProfilesList());
}

base::Value ProfilePickerHandler::GetProfilesList() {
  base::ListValue profiles_list;
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetAllProfilesAttributesSortedByName();
  for (const ProfileAttributesEntry* entry : entries) {
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

void ProfilePickerHandler::OnProfileAdded(const base::FilePath& profile_path) {
  PushProfilesList();
}

void ProfilePickerHandler::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  PushProfilesList();
}

void ProfilePickerHandler::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  PushProfilesList();
}

void ProfilePickerHandler::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  PushProfilesList();
}

void ProfilePickerHandler::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  PushProfilesList();
}
