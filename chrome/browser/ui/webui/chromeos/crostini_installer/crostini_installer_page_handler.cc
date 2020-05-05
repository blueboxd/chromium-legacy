// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_page_handler.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/crostini/crostini_disk.h"
#include "chrome/browser/chromeos/crostini/crostini_installer_ui_delegate.h"
#include "chrome/browser/chromeos/crostini/crostini_types.mojom.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/text/bytes_formatting.h"

namespace chromeos {

CrostiniInstallerPageHandler::CrostiniInstallerPageHandler(
    crostini::CrostiniInstallerUIDelegate* installer_ui_delegate,
    mojo::PendingReceiver<chromeos::crostini_installer::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<chromeos::crostini_installer::mojom::Page> pending_page,
    base::OnceClosure close_dialog_callback)
    : installer_ui_delegate_{installer_ui_delegate},
      receiver_{this, std::move(pending_page_handler)},
      page_{std::move(pending_page)},
      close_dialog_callback_{std::move(close_dialog_callback)} {}

CrostiniInstallerPageHandler::~CrostiniInstallerPageHandler() = default;

void CrostiniInstallerPageHandler::Install(int64_t disk_size_bytes,
                                           const std::string& username) {
  crostini::CrostiniManager::RestartOptions options{};
  if (base::FeatureList::IsEnabled(chromeos::features::kCrostiniDiskResizing)) {
    options.disk_size_bytes = disk_size_bytes;
  }
  if (base::FeatureList::IsEnabled(chromeos::features::kCrostiniUsername)) {
    options.container_username = username;
  }
  installer_ui_delegate_->Install(
      std::move(options),
      base::BindRepeating(&CrostiniInstallerPageHandler::OnProgressUpdate,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&CrostiniInstallerPageHandler::OnInstallFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniInstallerPageHandler::Cancel() {
  installer_ui_delegate_->Cancel(
      base::BindOnce(&CrostiniInstallerPageHandler::OnCanceled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniInstallerPageHandler::CancelBeforeStart() {
  installer_ui_delegate_->CancelBeforeStart();
}

void CrostiniInstallerPageHandler::Close() {
  if (close_dialog_callback_) {
    std::move(close_dialog_callback_).Run();
  }
}

void CrostiniInstallerPageHandler::OnProgressUpdate(
    crostini::mojom::InstallerState installer_state,
    double progress_fraction) {
  page_->OnProgressUpdate(installer_state, progress_fraction);
}

void CrostiniInstallerPageHandler::OnInstallFinished(
    crostini::mojom::InstallerError error) {
  page_->OnInstallFinished(error);
}

void CrostiniInstallerPageHandler::OnCanceled() {
  page_->OnCanceled();
}

void CrostiniInstallerPageHandler::RequestAmountOfFreeDiskSpace() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                     base::FilePath(crostini::kHomeDirectory)),
      base::BindOnce(&CrostiniInstallerPageHandler::OnAmountOfFreeDiskSpace,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniInstallerPageHandler::OnAmountOfFreeDiskSpace(int64_t free_bytes) {
  // User has to leave at least 100MiB for the host system.
  int64_t max_bytes =
      std::max(int64_t{0}, free_bytes - int64_t{100} * 1024 * 1024);
  const int64_t kDefaultDiskSize =
      crostini::CrostiniInstallerUIDelegate::kDefaultDiskSize;
  const int64_t kMinimumFreeDiskSpace =
      crostini::CrostiniInstallerUIDelegate::kMinimumFreeDiskSpace;

  // Default size is max(min_size, min(20GiB, available/2)). If default_size is
  // smaller than minimum size it'll get rounded up by GetTicks.
  int64_t default_size = std::min(kDefaultDiskSize, max_bytes / 2);
  int default_index;
  std::vector<crostini::mojom::DiskSliderTickPtr> ticks =
      crostini::disk::GetTicks(kMinimumFreeDiskSpace, default_size, max_bytes,
                               &default_index);
  page_->OnAmountOfFreeDiskSpace(std::move(ticks), default_index);
}

}  // namespace chromeos
