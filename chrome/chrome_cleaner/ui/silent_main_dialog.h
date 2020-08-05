// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_UI_SILENT_MAIN_DIALOG_H_
#define CHROME_CHROME_CLEANER_UI_SILENT_MAIN_DIALOG_H_

#include <string>
#include <vector>

#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/ui/main_dialog_api.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

// Silent version of MainDialogAPI, to be used in end to end tests. It silently
// accepts cleanup confirmations, and automatically quits when done messages
// are shown.
class SilentMainDialog : public MainDialogAPI {
 public:
  // The given delegate must outlive the SilentMainDialog.
  explicit SilentMainDialog(MainDialogDelegate* delegate);
  ~SilentMainDialog() override;

  // MainDialogAPI overrides.
  bool Create() override;
  void NoPUPsFound() override;
  void CleanupDone(ResultCode cleanup_result) override;
  void Close() override;
  void DisableExtensions(const std::vector<std::wstring>& extensions,
                         base::OnceCallback<void(bool)> on_disable) override;

 protected:
  void ConfirmCleanup(const std::vector<UwSId>& found_pups,
                      const FilePathSet& files_to_remove,
                      const std::vector<std::wstring>& registry_keys) override;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_UI_SILENT_MAIN_DIALOG_H_
