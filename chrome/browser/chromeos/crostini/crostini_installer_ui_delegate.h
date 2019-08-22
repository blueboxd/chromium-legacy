// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_INSTALLER_UI_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_INSTALLER_UI_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/strings/string16.h"

namespace crostini {

class CrostiniInstallerUIDelegate {
 public:
  // The size of the download for the VM image.
  // TODO(timloh): This is just a placeholder.
  static constexpr int64_t kDownloadSizeInBytes = 300 * 1024 * 1024;
  // The minimum feasible size for a VM disk image.
  static constexpr int64_t kMinimumDiskSize =
      1ll * 1024 * 1024 * 1024;  // 1 GiB
  // Minimum amount of free disk space to install crostini successfully.
  static constexpr int64_t kMinimumFreeDiskSpace =
      crostini::CrostiniInstallerUIDelegate::kDownloadSizeInBytes +
      kMinimumDiskSize;

  enum class InstallationState {
    START,                 // Just started installation
    INSTALL_IMAGE_LOADER,  // Loading the Termina VM component.
    START_CONCIERGE,       // Starting the Concierge D-Bus client.
    CREATE_DISK_IMAGE,     // Creating the image for the Termina VM.
    START_TERMINA_VM,      // Starting the Termina VM.
    CREATE_CONTAINER,      // Creating the container inside the Termina VM.
    SETUP_CONTAINER,       // Setting up the container inside the Termina VM.
    START_CONTAINER,       // Starting the container inside the Termina VM.
    FETCH_SSH_KEYS,        // Fetch ssh keys from concierge.
    MOUNT_CONTAINER,       // Do sshfs mount of container.
  };

  enum class Error {
    NONE,
    ERROR_LOADING_TERMINA,
    ERROR_STARTING_CONCIERGE,
    ERROR_CREATING_DISK_IMAGE,
    ERROR_STARTING_TERMINA,
    ERROR_STARTING_CONTAINER,
    ERROR_OFFLINE,
    ERROR_FETCHING_SSH_KEYS,
    ERROR_MOUNTING_CONTAINER,
    ERROR_SETTING_UP_CONTAINER,
    ERROR_INSUFFICIENT_DISK_SPACE,
  };

  // |progress_fraction| ranges from 0.0 to 1.0.
  using ProgressCallback =
      base::RepeatingCallback<void(InstallationState state,
                                   double progress_fraction)>;
  using ResultCallback = base::OnceCallback<void(Error error)>;

  // Start the installation. |progress_callback| will be called multiple times
  // until |result_callback| is called. The crostini terminal will be launched
  // when the installation succeeds.
  virtual void Install(ProgressCallback progress_callback,
                       ResultCallback result_callback) = 0;

  // Cancel the ongoing installation. |callback| will be called when it
  // finishes. The callbacks passed to |Install()| will not be called anymore.
  // A closing UI should call this if installation has started but hasn't
  // finished.
  virtual void Cancel(base::OnceClosure callback) = 0;
  // UI should call this if the user cancels without starting installation so
  // metrics can be recorded.
  virtual void CancelBeforeStart() = 0;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_INSTALLER_UI_DELEGATE_H_
