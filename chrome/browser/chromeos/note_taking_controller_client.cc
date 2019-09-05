// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/note_taking_controller_client.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace chromeos {

NoteTakingControllerClient::NoteTakingControllerClient(NoteTakingHelper* helper)
    : helper_(helper) {
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllSources());
  user_manager::UserManager::Get()->AddSessionStateObserver(this);
}

NoteTakingControllerClient::~NoteTakingControllerClient() {
  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
}

bool NoteTakingControllerClient::CanCreateNote() {
  return profile_ && helper_->IsAppAvailable(profile_);
}

void NoteTakingControllerClient::CreateNote() {
  helper_->LaunchAppForNewNote(profile_, base::FilePath());
}

void NoteTakingControllerClient::ActiveUserChanged(
    user_manager::User* active_user) {
  if (!active_user)
    return;

  active_user->AddProfileCreatedObserver(
      base::BindOnce(&NoteTakingControllerClient::SetProfileByUser,
                     weak_ptr_factory_.GetWeakPtr(), active_user));
}

void NoteTakingControllerClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_PROFILE_DESTROYED);
  // Update |profile_| when exiting a session or shutting down.
  Profile* profile = content::Source<Profile>(source).ptr();
  if (profile_ == profile)
    profile_ = nullptr;
}

void NoteTakingControllerClient::SetProfileByUser(
    const user_manager::User* user) {
  profile_ = ProfileHelper::Get()->GetProfileByUser(user);
}

}  // namespace chromeos
