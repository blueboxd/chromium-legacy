// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_page_handler.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_dialog.h"
#include "content/public/browser/web_contents.h"

namespace chromeos {

CrostiniUpgraderPageHandler::CrostiniUpgraderPageHandler(
    content::WebContents* web_contents,
    crostini::CrostiniUpgraderUIDelegate* upgrader_ui_delegate,
    mojo::PendingReceiver<chromeos::crostini_upgrader::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<chromeos::crostini_upgrader::mojom::Page> pending_page,
    base::OnceClosure close_dialog_callback,
    base::OnceClosure launch_closure)
    : web_contents_{web_contents},
      upgrader_ui_delegate_{upgrader_ui_delegate},
      receiver_{this, std::move(pending_page_handler)},
      page_{std::move(pending_page)},
      close_dialog_callback_{std::move(close_dialog_callback)},
      launch_closure_{std::move(launch_closure)} {
  upgrader_ui_delegate_->AddObserver(this);
}

CrostiniUpgraderPageHandler::~CrostiniUpgraderPageHandler() {
  upgrader_ui_delegate_->RemoveObserver(this);
}

namespace {

void Redisplay() {
  CrostiniUpgraderDialog::Show(base::DoNothing());
}

}  // namespace

void CrostiniUpgraderPageHandler::Backup() {
  Redisplay();
  upgrader_ui_delegate_->Backup(
      crostini::ContainerId(crostini::kCrostiniDefaultVmName,
                            crostini::kCrostiniDefaultContainerName),
      web_contents_);
}

void CrostiniUpgraderPageHandler::StartPrechecks() {
  upgrader_ui_delegate_->StartPrechecks();
}

void CrostiniUpgraderPageHandler::Upgrade() {
  Redisplay();
  upgrader_ui_delegate_->Upgrade(
      crostini::ContainerId(crostini::kCrostiniDefaultVmName,
                            crostini::kCrostiniDefaultContainerName));
}

void CrostiniUpgraderPageHandler::Restore() {
  Redisplay();
  upgrader_ui_delegate_->Restore(
      crostini::ContainerId(crostini::kCrostiniDefaultVmName,
                            crostini::kCrostiniDefaultContainerName),
      web_contents_);
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
  if (launch_closure_) {
    std::move(launch_closure_).Run();
  }
  std::move(close_dialog_callback_).Run();
}

void CrostiniUpgraderPageHandler::OnUpgradeProgress(
    const std::vector<std::string>& messages) {
  page_->OnUpgradeProgress(messages);
}

void CrostiniUpgraderPageHandler::OnUpgradeSucceeded() {
  Redisplay();
  page_->OnUpgradeSucceeded();
}

void CrostiniUpgraderPageHandler::OnUpgradeFailed() {
  Redisplay();
  page_->OnUpgradeFailed();
}

void CrostiniUpgraderPageHandler::OnBackupProgress(int percent) {
  page_->OnBackupProgress(percent);
}

void CrostiniUpgraderPageHandler::OnBackupSucceeded() {
  Redisplay();
  page_->OnBackupSucceeded();
}

void CrostiniUpgraderPageHandler::OnBackupFailed() {
  Redisplay();
  page_->OnBackupFailed();
}

void CrostiniUpgraderPageHandler::PrecheckStatus(
    chromeos::crostini_upgrader::mojom::UpgradePrecheckStatus status) {
  page_->PrecheckStatus(status);
}

void CrostiniUpgraderPageHandler::OnRestoreProgress(int percent) {
  page_->OnRestoreProgress(percent);
}

void CrostiniUpgraderPageHandler::OnRestoreSucceeded() {
  Redisplay();
  page_->OnRestoreSucceeded();
}

void CrostiniUpgraderPageHandler::OnRestoreFailed() {
  Redisplay();
  page_->OnRestoreFailed();
}

void CrostiniUpgraderPageHandler::OnCanceled() {
  page_->OnCanceled();
}

}  // namespace chromeos
