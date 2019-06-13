// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_app_installer_view.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/crostini/crostini_package_service.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace crostini {

constexpr unsigned int kMinScrollHeight = 0;
constexpr unsigned int kMaxScrollHeight = 250;

// Declaration in crostini_util.h, definition here. Needed because of include
// restrictions.
void ShowCrostiniAppInstallerView(Profile* profile,
                                  const LinuxPackageInfo& package_info) {
  views::DialogDelegate::CreateDialogWidget(
      new CrostiniAppInstallerView(profile, package_info), nullptr, nullptr)
      ->Show();
}

}  // namespace crostini

CrostiniAppInstallerView::CrostiniAppInstallerView(
    Profile* profile,
    const crostini::LinuxPackageInfo& package_info)
    : profile_(profile), package_info_(package_info) {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::TEXT, views::DialogContentType::TEXT));

  views::Label* message_label_ = new views::Label(
      l10n_util::GetStringFUTF16(IDS_CROSTINI_APP_INSTALL_DIALOG_BODY_TEXT,
                                 base::UTF8ToUTF16(package_info_.name)));
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label_);
  base::string16 description;

  if (package_info_.description.empty()) {
    description = l10n_util::GetStringUTF16(
        IDS_CROSTINI_APP_INSTALL_DIALOG_NO_DESCRIPTION_TEXT);
  } else {
    description = base::UTF8ToUTF16(package_info_.description);
  }

  base::string16 message = l10n_util::GetStringFUTF16(
      IDS_CROSTINI_APP_INSTALL_DIALOG_PACKAGE_TEXT,
      base::UTF8ToUTF16(package_info_.version), description);

  auto message_label = std::make_unique<views::Label>(message);
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  views::ScrollView* scroll_view = new views::ScrollView;
  scroll_view->SetDrawOverflowIndicator(true);
  scroll_view->ClipHeightTo(crostini::kMinScrollHeight,
                            crostini::kMaxScrollHeight);
  message_label_ = scroll_view->SetContents(std::move(message_label));

  AddChildView(scroll_view);
}

int CrostiniAppInstallerView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

base::string16 CrostiniAppInstallerView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(
        IDS_CROSTINI_APP_INSTALL_DIALOG_INSTALL_BUTTON);
  DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
  return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
}

base::string16 CrostiniAppInstallerView::GetWindowTitle() const {
  return l10n_util::GetStringFUTF16(IDS_CROSTINI_APP_INSTALL_DIALOG_TITLE_TEXT,
                                    base::UTF8ToUTF16(package_info_.name));
}

bool CrostiniAppInstallerView::ShouldShowCloseButton() const {
  return false;
}

bool CrostiniAppInstallerView::Accept() {
  // Start the installation process for the chosen Crosini app.

  // TODO(https://crbug.com/921429): We should handle installation errors.
  crostini::CrostiniPackageService::GetForProfile(profile_)
      ->InstallLinuxPackageFromApt(crostini::kCrostiniDefaultVmName,
                                   crostini::kCrostiniDefaultContainerName,
                                   package_info_.package_id, base::DoNothing());
  return true;
}

gfx::Size CrostiniAppInstallerView::CalculatePreferredSize() const {
  const int dialog_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                           margins().width();
  return gfx::Size(dialog_width, GetHeightForWidth(dialog_width));
}

CrostiniAppInstallerView::~CrostiniAppInstallerView() = default;
