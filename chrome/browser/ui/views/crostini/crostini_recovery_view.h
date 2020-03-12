// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_RECOVERY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_RECOVERY_VIEW_H_

#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace crostini {
enum class CrostiniResult;
}  // namespace crostini

class Profile;

// Provides a warning to the user that an upgrade is required and and internet
// connection is needed.
class CrostiniRecoveryView : public views::BubbleDialogDelegateView {
 public:
  static bool Show(Profile* profile,
                   const std::string& app_id,
                   int64_t display_id,
                   crostini::LaunchCrostiniAppCallback callback);

  // views::DialogDelegateView:
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  gfx::Size CalculatePreferredSize() const override;
  bool Accept() override;
  bool Cancel() override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;

  static CrostiniRecoveryView* GetActiveViewForTesting();

 private:
  CrostiniRecoveryView(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id,
                       crostini::LaunchCrostiniAppCallback callback);
  ~CrostiniRecoveryView() override;

  void ScheduleAppLaunch();
  void CompleteAppLaunch();

  Profile* profile_;  // Not owned.
  std::string app_id_;
  int64_t display_id_;
  crostini::LaunchCrostiniAppCallback callback_;
  bool can_launch_apps_ = false;
  views::Widget::ClosedReason closed_reason_ =
      views::Widget::ClosedReason::kUnspecified;

  base::WeakPtrFactory<CrostiniRecoveryView> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_RECOVERY_VIEW_H_
