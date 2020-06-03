// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_SETUP_SETUP_H_
#define CHROME_UPDATER_MAC_SETUP_SETUP_H_

namespace updater {

namespace setup_exit_codes {

// Success.
constexpr int kSuccess = 0;

// Failed to copy the updater's bundle.
constexpr int kFailedToCopyBundle = 10;

// Failed to delete the updater's install folder.
constexpr int kFailedToDeleteFolder = 11;

// Failed to remove the active(unversioned) update service job from Launchd.
constexpr int kFailedToRemoveActiveUpdateServiceJobFromLaunchd = 20;

// Failed to remove versioned update service job from Launchd.
constexpr int kFailedToRemoveCandidateUpdateServiceJobFromLaunchd = 21;

// Failed to remove versioned administration job from Launchd.
constexpr int kFailedToRemoveAdministrationJobFromLaunchd = 22;

// Failed to create the active(unversioned) update service Launchd plist.
constexpr int kFailedToCreateUpdateServiceLaunchdJobPlist = 30;

// Failed to create the versioned update service Launchd plist.
constexpr int kFailedToCreateVersionedUpdateServiceLaunchdJobPlist = 31;

// Failed to create the versioned administration Launchd plist.
constexpr int kFailedToCreateAdministrationLaunchdJobPlist = 32;

// Failed to start the active(unversioned) update service job.
constexpr int kFailedToStartLaunchdActiveServiceJob = 40;

// Failed to start the versioned update service job.
constexpr int kFailedToStartLaunchdVersionedServiceJob = 41;

// Failed to start the administration job.
constexpr int kFailedToStartLaunchdAdministrationJob = 42;

}  // namespace setup_exit_codes

// Sets up the candidate updater by copying the bundle, creating launchd plists
// for administration service and XPC service tasks, and start the corresponding
// launchd jobs.
int InstallCandidate();

// Uninstalls this version of the updater.
int UninstallCandidate();

// Sets up this version of the Updater as the active version.
int PromoteCandidate();

// Removes the launchd plists for scheduled tasks and xpc service. Deletes the
// updater bundle from its installed location.
int Uninstall(bool is_machine);

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_SETUP_SETUP_H_
