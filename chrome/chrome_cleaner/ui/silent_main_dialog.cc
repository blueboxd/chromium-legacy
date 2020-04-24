// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ui/silent_main_dialog.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/check.h"

namespace chrome_cleaner {

SilentMainDialog::SilentMainDialog(MainDialogDelegate* delegate)
    : MainDialogAPI(delegate) {
  DCHECK(delegate);
}

SilentMainDialog::~SilentMainDialog() {}

bool SilentMainDialog::Create() {
  return true;
}

void SilentMainDialog::NoPUPsFound() {
  delegate()->OnClose();
}

void SilentMainDialog::ConfirmCleanup(
    const std::vector<UwSId>& found_pups,
    const FilePathSet& files_to_remove,
    const std::vector<base::string16>& registry_keys) {
  delegate()->AcceptedCleanup(true);
}

void SilentMainDialog::CleanupDone(ResultCode cleanup_result) {
  delegate()->OnClose();
}

void SilentMainDialog::Close() {
  delegate()->OnClose();
}

void SilentMainDialog::DisableExtensions(
    const std::vector<base::string16>& extensions,
    base::OnceCallback<void(bool)> on_disable) {
  std::move(on_disable).Run(true);
}

}  // namespace chrome_cleaner
