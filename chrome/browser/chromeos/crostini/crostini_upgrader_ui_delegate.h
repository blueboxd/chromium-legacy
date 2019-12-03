// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UPGRADER_UI_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UPGRADER_UI_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/strings/string16.h"

namespace crostini {

struct ContainerId;

class CrostiniUpgraderUIObserver {
 public:
  virtual void OnBackupProgress(int percent) = 0;
  virtual void OnBackupSucceeded() = 0;
  virtual void OnBackupFailed() = 0;
  virtual void OnUpgradeProgress(const std::vector<std::string>& messages) = 0;
  virtual void OnUpgradeSucceeded() = 0;
  virtual void OnUpgradeFailed() = 0;
  virtual void OnCanceled() = 0;
};

class CrostiniUpgraderUIDelegate {
 public:
  // |observer| is  used to relay progress messages as they are received from
  // the upgrade flow.
  virtual void AddObserver(CrostiniUpgraderUIObserver* observer) = 0;
  virtual void RemoveObserver(CrostiniUpgraderUIObserver* observer) = 0;

  // Back up the current container before upgrading
  virtual void Backup() = 0;

  // Start the upgrade.
  virtual void Upgrade(const ContainerId& container_id) = 0;

  // Cancel the ongoing upgrade.
  virtual void Cancel() = 0;

  // UI should call this if the user cancels without starting upgrade so
  // metrics can be recorded.
  virtual void CancelBeforeStart() = 0;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UPGRADER_UI_DELEGATE_H_
