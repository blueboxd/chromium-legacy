// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_page_handler.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"

namespace chromeos {

CrostiniUpgraderPageHandler::CrostiniUpgraderPageHandler(
    crostini::CrostiniUpgraderUIDelegate* upgrader_ui_delegate,
    mojo::PendingReceiver<chromeos::crostini_upgrader::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<chromeos::crostini_upgrader::mojom::Page> pending_page,
    base::OnceClosure close_dialog_callback,
    base::OnceClosure launch_closure)
    : upgrader_ui_delegate_{upgrader_ui_delegate},
      receiver_{this, std::move(pending_page_handler)},
      page_{std::move(pending_page)},
      close_dialog_callback_{std::move(close_dialog_callback)},
      launch_closure_{std::move(launch_closure)} {
  upgrader_ui_delegate_->AddObserver(this);
}

CrostiniUpgraderPageHandler::~CrostiniUpgraderPageHandler() {
  upgrader_ui_delegate_->RemoveObserver(this);
}

void CrostiniUpgraderPageHandler::Backup() {
  upgrader_ui_delegate_->Backup();
}

void CrostiniUpgraderPageHandler::Upgrade() {
  upgrader_ui_delegate_->Upgrade(
      crostini::ContainerId(crostini::kCrostiniDefaultVmName,
                            crostini::kCrostiniDefaultContainerName));
}

void CrostiniUpgraderPageHandler::Cancel() {
  upgrader_ui_delegate_->Cancel();
}

void CrostiniUpgraderPageHandler::Launch() {
  std::move(launch_closure_).Run();
}

void CrostiniUpgraderPageHandler::CancelBeforeStart() {
  upgrader_ui_delegate_->CancelBeforeStart();
}

void CrostiniUpgraderPageHandler::Close() {
  std::move(close_dialog_callback_).Run();
}

void CrostiniUpgraderPageHandler::OnUpgradeProgress(
    const std::vector<std::string>& messages) {
  page_->OnUpgradeProgress(messages);
}

void CrostiniUpgraderPageHandler::OnUpgradeSucceeded() {
  page_->OnUpgradeSucceeded();
}

void CrostiniUpgraderPageHandler::OnUpgradeFailed() {
  page_->OnUpgradeFailed();
}

void CrostiniUpgraderPageHandler::OnBackupProgress(int percent) {
  page_->OnBackupProgress(percent);
}

void CrostiniUpgraderPageHandler::OnBackupSucceeded() {
  page_->OnBackupSucceeded();
}

void CrostiniUpgraderPageHandler::OnBackupFailed() {
  page_->OnBackupFailed();
}

void CrostiniUpgraderPageHandler::OnCanceled() {
  page_->OnCanceled();
}

}  // namespace chromeos
