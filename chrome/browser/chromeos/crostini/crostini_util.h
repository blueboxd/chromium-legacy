// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UTIL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "ui/base/resource/scale_factor.h"

namespace base {
class FilePath;
class TimeDelta;
}  // namespace base

namespace gfx {
class ImageSkia;
}  // namespace gfx

class Profile;

namespace crostini {

struct LinuxPackageInfo;

// A unique identifier for our containers. This is <vm_name, container_name>.
using ContainerId = std::pair<std::string, std::string>;

// Return" (<vm_name>, <container_name>)".
std::string ContainerIdToString(const ContainerId& container_id);

// Returns true if crostini is allowed to run for |profile|.
// Otherwise, returns false, e.g. if crostini is not available on the device,
// or it is in the flow to set up managed account creation.
bool IsCrostiniAllowedForProfile(Profile* profile);

// When |check_policy| is true, returns true if fully interactive crostini UI
// may be shown. Implies crostini is allowed to run.
// When check_policy is false, returns true if crostini UI is not forbidden by
// hardware, flags, etc, even if it is forbidden by the enterprise policy. The
// UI uses this to indicate that crostini is available but disabled by policy.
bool IsCrostiniUIAllowedForProfile(Profile* profile, bool check_policy = true);

// Returns true if policy allows export import UI.
bool IsCrostiniExportImportUIAllowedForProfile(Profile* profile);

// Returns whether if Crostini has been enabled, i.e. the user has launched it
// at least once and not deleted it.
bool IsCrostiniEnabled(Profile* profile);

// Returns whether the default Crostini VM is running for the user.
bool IsCrostiniRunning(Profile* profile);

// Returns whether infrastructure for applying Ansible playbook to default
// Crostini container is enabled.
bool IsCrostiniAnsibleInfrastructureEnabled();

// Launches the Crostini app with ID of |app_id| on the display with ID of
// |display_id|. |app_id| should be a valid Crostini app list id.
void LaunchCrostiniApp(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id);

// Launch a Crostini App with a given set of files, given as absolute paths in
// the container. For apps which can only be launched with a single file,
// launch multiple instances.
void LaunchCrostiniApp(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id,
                       const std::vector<std::string>& files);

// Convenience wrapper around CrostiniAppIconLoader. As requesting icons from
// the container can be slow, we just use the default (penguin) icons after the
// timeout elapses. Subsequent calls would get the correct icons once loaded.
void LoadIcons(Profile* profile,
               const std::vector<std::string>& app_ids,
               int resource_size_in_dip,
               ui::ScaleFactor scale_factor,
               base::TimeDelta timeout,
               base::OnceCallback<void(const std::vector<gfx::ImageSkia>&)>
                   icons_loaded_callback);

// Retrieves cryptohome_id from profile.
std::string CryptohomeIdForProfile(Profile* profile);

// Retrieves username from profile.  This is the text until '@' in
// profile->GetProfileUserName() email address.
std::string DefaultContainerUserNameForProfile(Profile* profile);

// Returns the mount directory within the container where paths from the Chrome
// OS host such as within Downloads are shared with the container.
base::FilePath ContainerChromeOSBaseDirectory();

// The Terminal opens Crosh but overrides the Browser's app_name so that we can
// identify it as the Crostini Terminal. In the future, we will also use these
// for Crostini apps marked Terminal=true in their .desktop file.
std::string AppNameFromCrostiniAppId(const std::string& id);

// Returns nullopt for a non-Crostini app name.
base::Optional<std::string> CrostiniAppIdFromAppName(
    const std::string& app_name);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CrostiniUISurface { kSettings = 0, kAppList = 1, kCount };

// See chrome/browser/ui/views/crostini for implementation of the ShowXXX
// functions below.

// Shows the Crostini Installer dialog.
void ShowCrostiniInstallerView(Profile* profile, CrostiniUISurface ui_surface);
// Shows the Crostini Uninstaller dialog.
void ShowCrostiniUninstallerView(Profile* profile,
                                 CrostiniUISurface ui_surface);
// Shows the Crostini App installer dialog.
void ShowCrostiniAppInstallerView(Profile* profile,
                                  const LinuxPackageInfo& package_info);
// Shows the Crostini App Uninstaller dialog.
void ShowCrostiniAppUninstallerView(Profile* profile,
                                    const std::string& app_id);
// Shows the Crostini Termina Upgrade dialog (for blocking crostini start until
// Termina version matches).
void ShowCrostiniUpgradeView(Profile* profile, CrostiniUISurface ui_surface);

// Shows the Crostini Container Upgrade dialog (for running upgrades in the
// container).
void ShowCrostiniUpgradeContainerView(Profile* profile,
                                      CrostiniUISurface ui_surface);
// Show the Crostini Container Upgrade dialog after a delay
// (CloseCrostiniUpgradeContainerView will cancel the next dialog show).
void PrepareShowCrostiniUpgradeContainerView(Profile* profile,
                                             CrostiniUISurface ui_surface);
// Closes the current CrostiniUpgradeContainerView or ensures that the view will
// not open until PrepareShowCrostiniUpgradeContainerView is called again.
void CloseCrostiniUpgradeContainerView();

// We use an arbitrary well-formed extension id for the Terminal app, this
// is equal to GenerateId("Terminal").
constexpr char kCrostiniTerminalId[] = "oajcgpnkmhaalajejhlfpacbiokdnnfe";

constexpr char kCrostiniDefaultVmName[] = "termina";
constexpr char kCrostiniDefaultContainerName[] = "penguin";
constexpr char kCrostiniCroshBuiltinAppId[] =
    "nkoccljplnhpfnfiajclkommnmllphnl";
// In order to be compatible with sync folder id must match standard.
// Generated using crx_file::id_util::GenerateId("LinuxAppsFolder")
constexpr char kCrostiniFolderId[] = "ddolnhmblagmcagkedkbfejapapdimlk";
constexpr char kCrostiniDefaultImageServerUrl[] =
    "https://storage.googleapis.com/cros-containers/%d";
constexpr char kCrostiniDefaultImageAlias[] = "debian/stretch";
constexpr base::FilePath::CharType kHomeDirectory[] =
    FILE_PATH_LITERAL("/home");

// Whether running Crostini is allowed for unaffiliated users per enterprise
// policy.
bool IsUnaffiliatedCrostiniAllowedByPolicy();

// Add a newly created LXD container to the kCrostiniContainers pref
void AddNewLxdContainerToPrefs(Profile* profile,
                               std::string vm_name,
                               std::string container_name);

// Remove a newly deleted LXD container from the kCrostiniContainers pref, and
// deregister its apps and mime types.
void RemoveLxdContainerFromPrefs(Profile* profile,
                                 std::string vm_name,
                                 std::string container_name);

// Returns a string to be displayed in a notification with the estimated time
// left for an operation to run which started and time |start| and is current
// at |percent| way through.
base::string16 GetTimeRemainingMessage(base::TimeTicks start, int percent);

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UTIL_H_
