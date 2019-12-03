// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UPGRADER_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UPGRADER_H_

#include "base/callback_forward.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_upgrader_ui_delegate.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace crostini {

class CrostiniUpgrader : public KeyedService,
                         public UpgradeContainerProgressObserver,
                         public CrostiniUpgraderUIDelegate {
 public:
  static CrostiniUpgrader* GetForProfile(Profile* profile);

  explicit CrostiniUpgrader(Profile* profile);
  CrostiniUpgrader(const CrostiniUpgrader&) = delete;
  CrostiniUpgrader& operator=(const CrostiniUpgrader&) = delete;

  ~CrostiniUpgrader() override;

  // KeyedService:
  void Shutdown() override;

  // CrostiniUpgraderUIDelegate:
  void AddObserver(CrostiniUpgraderUIObserver* observer) override;
  void RemoveObserver(CrostiniUpgraderUIObserver* observer) override;
  void Backup() override;
  void Upgrade(const ContainerId& container_id) override;
  void Cancel() override;
  void CancelBeforeStart() override;

  // CrostiniManager::UpgradeContainerProgressObserver:
  void OnUpgradeContainerProgress(
      const ContainerId& container_id,
      UpgradeContainerProgressStatus status,
      const std::vector<std::string>& messages) override;

  // Return true if internal state allows starting upgrade.
  bool CanUpgrade();

 private:
  void OnCancel(CrostiniResult result);
  void OnBackup(CrostiniResult result);
  void OnUpgrade(CrostiniResult result);

  Profile* profile_;
  ContainerId container_id_;
  base::ObserverList<CrostiniUpgraderUIObserver>::Unchecked upgrader_observers_;

  base::WeakPtrFactory<CrostiniUpgrader> weak_ptr_factory_{this};
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UPGRADER_H_
