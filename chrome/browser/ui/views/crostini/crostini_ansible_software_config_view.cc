// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_ansible_software_config_view.h"

#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

CrostiniAnsibleSoftwareConfigView*
    g_crostini_ansible_software_configuration_view = nullptr;

}  // namespace

void crostini::ShowCrostiniAnsibleSoftwareConfigView() {
  if (!g_crostini_ansible_software_configuration_view) {
    g_crostini_ansible_software_configuration_view =
        new CrostiniAnsibleSoftwareConfigView();
    views::DialogDelegate::CreateDialogWidget(
        g_crostini_ansible_software_configuration_view, nullptr, nullptr);
  }
  g_crostini_ansible_software_configuration_view->GetWidget()->Show();
}

int CrostiniAnsibleSoftwareConfigView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

base::string16 CrostiniAnsibleSoftwareConfigView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_LABEL);
}

bool CrostiniAnsibleSoftwareConfigView::ShouldShowCloseButton() const {
  return true;
}

gfx::Size CrostiniAnsibleSoftwareConfigView::CalculatePreferredSize() const {
  const int dialog_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH) -
                           margins().width();
  return gfx::Size(dialog_width, GetHeightForWidth(dialog_width));
}

CrostiniAnsibleSoftwareConfigView::CrostiniAnsibleSoftwareConfigView() {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::TEXT, views::DialogContentType::CONTROL));

  const base::string16 message =
      l10n_util::GetStringUTF16(IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_SUBTEXT);
  auto message_label = std::make_unique<views::Label>(message);
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(message_label));

  // Add infinite progress bar.
  // TODO(crbug.com/1000173): add progress reporting and display text above
  // progress bar indicating current process.
  auto progress_bar = std::make_unique<views::ProgressBar>();
  progress_bar->SetVisible(true);
  // Values outside the range [0,1] display an infinite loading animation.
  progress_bar->SetValue(-1);
  AddChildView(std::move(progress_bar));

  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::CROSTINI_ANSIBLE_SOFTWARE_CONFIG);
}

CrostiniAnsibleSoftwareConfigView::~CrostiniAnsibleSoftwareConfigView() {
  g_crostini_ansible_software_configuration_view = nullptr;
}
