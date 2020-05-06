// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_UTIL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/observer_list_types.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace aura {
class Window;
}  // namespace aura

namespace dlcservice {
class DlcModuleList;
}  // namespace dlcservice

class Profile;
class GURL;

namespace plugin_vm {

class PluginVmPolicySubscription;

// Generated as crx_file::id_util::GenerateId("org.chromium.plugin_vm");
constexpr char kPluginVmAppId[] = "lgjpclljbbmphhnalkeplcmnjpfmmaek";

// Name of the Plugin VM.
constexpr char kPluginVmName[] = "PvmDefault";

const net::NetworkTrafficAnnotationTag kPluginVmNetworkTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("plugin_vm_image_download", R"(
      semantics {
        sender: "Plugin VM image manager"
        description: "Request to download Plugin VM image is sent in order "
          "to allow user to run Plugin VM."
        trigger: "User clicking on Plugin VM icon when Plugin VM is not yet "
          "installed."
        data: "Request to download Plugin VM image. Sends cookies to "
          "authenticate the user."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        chrome_policy {
          PluginVmImage {
            PluginVmImage: "{'url': 'example.com', 'hash': 'sha256hash'}"
          }
        }
      }
    )");

// Checks if Plugin VM is allowed for the current profile.
bool IsPluginVmAllowedForProfile(const Profile* profile);

// Returns whether Plugin VM has been installed.
// TODO(timloh): We should detect installations via VMC, currently the user
// needs to manually launch the installer once for the pref to get set.
bool IsPluginVmConfigured(const Profile* profile);

// Returns true if Plugin VM is allowed and configured for the current profile.
bool IsPluginVmEnabled(const Profile* profile);

// Determines if the default Plugin VM is running and visible.
bool IsPluginVmRunning(Profile* profile);

void ShowPluginVmInstallerView(Profile* profile);

// Checks if an window is for Plugin VM.
bool IsPluginVmWindow(const aura::Window* window);

// Retrieves the license key to be used for Plugin VM. If
// none is set this will return an empty string.
std::string GetPluginVmLicenseKey();

// Retrieves the User Id to be used for Plugin VM. If none is set this will
// return an empty string.
std::string GetPluginVmUserId();

// Sets fake policy values and enables Plugin VM for testing. These set global
// state so this should be called with empty strings on tear down.
// TODO(crbug.com/1025136): Remove this once Tast supports setting test
// policies.
void SetFakePluginVmPolicy(Profile* profile,
                           const std::string& image_path,
                           const std::string& image_hash,
                           const std::string& license_key);
bool FakeLicenseKeyIsSet();
bool FakeUserIdIsSet();

// Used to clean up the Plugin VM Drive download directory if it did not get
// removed when it should have, perhaps due to a crash.
void RemoveDriveDownloadDirectoryIfExists();
bool IsDriveUrl(const GURL& url);
std::string GetIdFromDriveUrl(const GURL& url);

// Used during communication with |DlcserviceClient|.
dlcservice::DlcModuleList GetPluginVmDlcModuleList();

// A subscription for changes to PluginVm policy that may affect
// IsPluginVmAllowedForProfile.
class PluginVmPolicySubscription {
 public:
  using PluginVmAllowedChanged = base::RepeatingCallback<void(bool is_allowed)>;
  PluginVmPolicySubscription(Profile* profile, PluginVmAllowedChanged callback);
  ~PluginVmPolicySubscription();

  PluginVmPolicySubscription(const PluginVmPolicySubscription&) = delete;
  PluginVmPolicySubscription& operator=(const PluginVmPolicySubscription&) =
      delete;

 private:
  // Internal callback for policy changes.
  void OnPolicyChanged();

  Profile* profile_;

  // Whether Plugin VM was previously allowed for the profile.
  bool is_allowed_;

  // The user-provided callback method.
  PluginVmAllowedChanged callback_;

  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      allowed_subscription_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      license_subscription_;
  std::unique_ptr<base::CallbackList<void(void)>::Subscription>
      fake_license_subscription_;
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_UTIL_H_
