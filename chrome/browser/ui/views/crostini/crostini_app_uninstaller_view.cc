// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_app_uninstaller_view.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/crostini/crostini_package_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace crostini {

// Declaration in crostini_util.h, definition here. Needed because of include
// restrictions.
void ShowCrostiniAppUninstallerView(Profile* profile,
                                    const std::string& app_id) {
  CrostiniAppUninstallerView::Show(profile, app_id);
}

}  // namespace crostini

// static
void CrostiniAppUninstallerView::Show(Profile* profile,
                                      const std::string& app_id) {
  DCHECK(crostini::IsCrostiniUIAllowedForProfile(profile));
  views::DialogDelegate::CreateDialogWidget(
      new CrostiniAppUninstallerView(profile, app_id), nullptr, nullptr)
      ->Show();
}

int CrostiniAppUninstallerView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

base::string16 CrostiniAppUninstallerView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(
        IDS_CROSTINI_APPLICATION_UNINSTALL_UNINSTALL_BUTTON);
  DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
  return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
}

base::string16 CrostiniAppUninstallerView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_CROSTINI_APPLICATION_UNINSTALL_CONFIRM_TITLE);
}

bool CrostiniAppUninstallerView::ShouldShowCloseButton() const {
  return false;
}

bool CrostiniAppUninstallerView::Accept() {
  // Switch over to the notification service to uninstall the package and
  // display notifications related to the uninstall.
  crostini::CrostiniPackageService::GetForProfile(profile_)
      ->QueueUninstallApplication(app_id_);
  return true;  // Should close the dialog
}

gfx::Size CrostiniAppUninstallerView::CalculatePreferredSize() const {
  const int dialog_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                           margins().width();
  return gfx::Size(dialog_width, GetHeightForWidth(dialog_width));
}

CrostiniAppUninstallerView::CrostiniAppUninstallerView(
    Profile* profile,
    const std::string& app_id)
    : profile_(profile), app_id_(app_id), weak_ptr_factory_(this) {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::TEXT, views::DialogContentType::TEXT));

  crostini::CrostiniRegistryService* registry =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile);
  DCHECK(registry);
  auto app_registration = registry->GetRegistration(app_id);
  DCHECK(app_registration);
  const base::string16 app_name = base::UTF8ToUTF16(app_registration->Name());

  const base::string16 message = l10n_util::GetStringFUTF16(
      IDS_CROSTINI_APPLICATION_UNINSTALL_CONFIRM_BODY, app_name);
  auto* message_label = new views::Label(message);
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label);

  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::CROSTINI_APP_UNINSTALLER);
}

CrostiniAppUninstallerView::~CrostiniAppUninstallerView() = default;
