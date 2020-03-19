// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_UNINSTALL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_UNINSTALL_DIALOG_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/apps/app_service/uninstall_dialog.h"
#include "chrome/browser/ui/views/apps/app_dialog/app_dialog_view.h"
#include "chrome/services/app_service/public/mojom/types.mojom-forward.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/styled_label_listener.h"

class Profile;

namespace views {
class Checkbox;
}  // namespace views

namespace gfx {
class ImageSkia;
}

// This class generates the unified uninstall dialog based on the app type. Once
// the user has confirmed the uninstall, this class calls the parent class
// apps::UninstallDialog::UiBase to notify AppService, which transfers control
// to the publisher to uninstall the app.
class AppUninstallDialogView : public apps::UninstallDialog::UiBase,
                               public AppDialogView,
                               public views::StyledLabelListener {
 public:
  AppUninstallDialogView(Profile* profile,
                         apps::mojom::AppType app_type,
                         const std::string& app_id,
                         const std::string& app_name,
                         gfx::ImageSkia image,
                         apps::UninstallDialog* uninstall_dialog);
  ~AppUninstallDialogView() override;

  static AppUninstallDialogView* GetActiveViewForTesting();

  // views::BubbleDialogDelegateView:
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;

 private:
  void InitializeView(Profile* profile,
                      apps::mojom::AppType app_type,
                      const std::string& app_id);

  void InitializeCheckbox(const GURL& app_launch_url);

  void InitializeViewForExtension(Profile* profile, const std::string& app_id);
  void InitializeViewForWebApp(Profile* profile, const std::string& app_id);
#if defined(OS_CHROMEOS)
  void InitializeViewForArcApp(Profile* profile, const std::string& app_id);
  void InitializeViewForCrostiniApp(Profile* profile,
                                    const std::string& app_id);
#endif

  // views::StyledLabelListener methods.
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  void OnDialogCancelled();
  void OnDialogAccepted();

  Profile* profile_;

  // The type of apps, e.g. Extension-backed app, Android app.
  apps::mojom::AppType app_type_;

  // Whether app represents a shortcut. |shortcut_| is available for the ARC
  // apps only.
#if defined(OS_CHROMEOS)
  bool shortcut_ = false;
#endif

  views::Checkbox* report_abuse_checkbox_ = nullptr;
  views::Checkbox* clear_site_data_checkbox_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppUninstallDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_UNINSTALL_DIALOG_VIEW_H_
