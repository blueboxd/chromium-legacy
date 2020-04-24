// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_INSTALLER_UI_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_INSTALLER_UI_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_types.mojom-forward.h"

namespace crostini {

class CrostiniInstallerUIDelegate {
 public:
  // The size of the download for the VM image.
  // TODO(timloh): This is just a placeholder.
  // As of 2020-01-10 the Termina files.zip is ~90MiB and the squashfs container
  // is ~330MiB.
  static constexpr int64_t kDownloadSizeInBytes =
      450ll * 1024 * 1024;  // 450 MiB

  // As of 2020-01-10 Crostini once installed uses ~1.8GiB, and metrics show
  // that install success rate plummets when users have less than that much free
  // space.
  static constexpr int64_t kMinimumDiskSize =
      int64_t{2} * 1024 * 1024 * 1024;  // 2 GiB
  static constexpr int64_t kMinimumFreeDiskSpace = kMinimumDiskSize;
  static constexpr int64_t kDefaultDiskSize =
      int64_t{20} * 1024 * 1024 * 1024;  // 20 GIB

  // |progress_fraction| ranges from 0.0 to 1.0.
  using ProgressCallback =
      base::RepeatingCallback<void(crostini::mojom::InstallerState state,
                                   double progress_fraction)>;
  using ResultCallback =
      base::OnceCallback<void(crostini::mojom::InstallerError error)>;

  // Start the installation. |progress_callback| will be called multiple times
  // until |result_callback| is called. The crostini terminal will be launched
  // when the installation succeeds.
  virtual void Install(CrostiniManager::RestartOptions options,
                       ProgressCallback progress_callback,
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
