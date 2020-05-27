// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_process_manager.h"

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sharing/webrtc/sharing_mojo_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/service_process_host.h"

namespace {

ProfileAttributesEntry* GetStoredNearbyProfile() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return nullptr;

  base::FilePath advertising_profile_path =
      local_state->GetFilePath(kNearbySharingActiveProfilePrefName);
  if (advertising_profile_path.empty())
    return nullptr;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;

  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  ProfileAttributesEntry* entry;
  if (!storage.GetProfileAttributesWithPath(advertising_profile_path, &entry)) {
    // Stored profile path is invalid so remove it.
    local_state->ClearPref(kNearbySharingActiveProfilePrefName);
    return nullptr;
  }
  return entry;
}

void SetStoredNearbyProfile(Profile* profile) {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return;

  if (profile) {
    local_state->SetFilePath(kNearbySharingActiveProfilePrefName,
                             profile->GetPath());
  } else {
    local_state->ClearPref(kNearbySharingActiveProfilePrefName);
  }
}

bool IsStoredNearbyProfile(Profile* profile) {
  ProfileAttributesEntry* entry = GetStoredNearbyProfile();
  if (!entry)
    return profile == nullptr;
  return profile && entry->GetPath() == profile->GetPath();
}

}  // namespace

// static
NearbyProcessManager& NearbyProcessManager::GetInstance() {
  static base::NoDestructor<NearbyProcessManager> instance;
  return *instance;
}

void NearbyProcessManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NearbyProcessManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

ProfileAttributesEntry* NearbyProcessManager::GetActiveProfile() const {
  return GetStoredNearbyProfile();
}

bool NearbyProcessManager::IsActiveProfile(Profile* profile) const {
  // If the active profile is not loaded yet, try looking in prefs.
  // TODO(knollr): Add a test for this.
  if (!active_profile_)
    return IsStoredNearbyProfile(profile);

  return active_profile_ == profile;
}

bool NearbyProcessManager::IsAnyProfileActive() const {
  return !IsActiveProfile(/*profile=*/nullptr);
}

void NearbyProcessManager::SetActiveProfile(Profile* profile) {
  if (IsActiveProfile(profile))
    return;

  active_profile_ = profile;
  SetStoredNearbyProfile(active_profile_);
  StopProcess(active_profile_);

  for (auto& observer : observers_)
    observer.OnNearbyProfileChanged(profile);
}

void NearbyProcessManager::ClearActiveProfile() {
  SetActiveProfile(/*profile=*/nullptr);
}

location::nearby::connections::mojom::NearbyConnections*
NearbyProcessManager::GetOrStartNearbyConnections(Profile* profile) {
  if (!IsActiveProfile(profile))
    return nullptr;

  // Launch a new Nearby Connections interface if required.
  if (!connections_.is_bound())
    BindNearbyConnections();

  return connections_.get();
}

void NearbyProcessManager::StopProcess(Profile* profile) {
  if (!IsActiveProfile(profile))
    return;

  bool was_running = sharing_process_.is_bound();

  connections_host_.reset();
  connections_.reset();
  sharing_process_.reset();

  if (was_running) {
    for (auto& observer : observers_)
      observer.OnNearbyProcessStopped();
  }
}

void NearbyProcessManager::OnProfileAdded(Profile* profile) {
  // Cache active |profile| once it loads so we don't have to check prefs.
  if (IsActiveProfile(profile))
    active_profile_ = profile;
}

void NearbyProcessManager::OnProfileMarkedForPermanentDeletion(
    Profile* profile) {
  if (IsActiveProfile(profile))
    SetActiveProfile(nullptr);
}

void NearbyProcessManager::BindSharingProcess(
    mojo::PendingRemote<sharing::mojom::Sharing> sharing) {
  sharing_process_.Bind(std::move(sharing));
  // base::Unretained() is safe as |this| is a singleton.
  sharing_process_.set_disconnect_handler(base::BindOnce(
      &NearbyProcessManager::OnNearbyProcessStopped, base::Unretained(this)));
}

NearbyProcessManager::NearbyProcessManager() {
  // profile_manager() might be null in tests or during shutdown.
  if (auto* manager = g_browser_process->profile_manager())
    manager->AddObserver(this);
}

NearbyProcessManager::~NearbyProcessManager() {
  if (auto* manager = g_browser_process->profile_manager())
    manager->RemoveObserver(this);
}

void NearbyProcessManager::LaunchNewProcess() {
  // Stop any running process and mojo pipes.
  StopProcess(active_profile_);

  // Launch a new sandboxed process.
  // TODO(knollr): Set process name to "Nearby Sharing".
  BindSharingProcess(sharing::LaunchSharing());
}

void NearbyProcessManager::BindNearbyConnections() {
  // Start a new process if there is none running yet.
  if (!sharing_process_.is_bound())
    LaunchNewProcess();

  // Create the Nearby Connections stack in the sandboxed process.
  // base::Unretained() calls below are safe as |this| is a singleton.
  sharing_process_->CreateNearbyConnections(
      connections_host_.BindNewPipeAndPassRemote(),
      base::BindOnce(&NearbyProcessManager::OnNearbyConnections,
                     base::Unretained(this),
                     connections_.BindNewPipeAndPassReceiver()));

  // Terminate the process if the Nearby Connections interface disconnects as
  // that indicated an incorrect state and we have to restart the process.
  connections_host_.set_disconnect_handler(base::BindOnce(
      &NearbyProcessManager::OnNearbyProcessStopped, base::Unretained(this)));
  connections_.set_disconnect_handler(base::BindOnce(
      &NearbyProcessManager::OnNearbyProcessStopped, base::Unretained(this)));
}

void NearbyProcessManager::OnNearbyConnections(
    mojo::PendingReceiver<NearbyConnectionsMojom> receiver,
    mojo::PendingRemote<NearbyConnectionsMojom> remote) {
  if (!mojo::FusePipes(std::move(receiver), std::move(remote))) {
    LOG(WARNING) << "Failed to initialize Nearby Connectins process";
    StopProcess(active_profile_);
    return;
  }

  for (auto& observer : observers_)
    observer.OnNearbyProcessStarted();
}

void NearbyProcessManager::OnNearbyProcessStopped() {
  StopProcess(active_profile_);
}
