// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_recovery_view.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {

CrostiniRecoveryView* g_crostini_recovery_view = nullptr;

constexpr char kCrostiniRecoverySourceHistogram[] = "Crostini.RecoverySource";

}  // namespace

bool crostini::ShowCrostiniRecoveryView(
    Profile* profile,
    crostini::CrostiniUISurface ui_surface,
    const std::string& app_id,
    int64_t display_id,
    crostini::LaunchCrostiniAppCallback callback) {
  bool allow_app_launch = CrostiniRecoveryView::Show(
      profile, app_id, display_id, std::move(callback));
  if (!allow_app_launch) {
    // App launches are prevented by the view's can_launch_apps_. In this case,
    // we want to sample the Show call.
    base::UmaHistogramEnumeration(kCrostiniRecoverySourceHistogram, ui_surface,
                                  crostini::CrostiniUISurface::kCount);
  }
  return allow_app_launch;
}

bool CrostiniRecoveryView::Show(Profile* profile,
                                const std::string& app_id,
                                int64_t display_id,
                                crostini::LaunchCrostiniAppCallback callback) {
  DCHECK(crostini::CrostiniFeatures::Get()->IsUIAllowed(profile));
  if (!g_crostini_recovery_view) {
    g_crostini_recovery_view = new CrostiniRecoveryView(
        profile, app_id, display_id, std::move(callback));
    CreateDialogWidget(g_crostini_recovery_view, nullptr, nullptr);
  }
  g_crostini_recovery_view->GetWidget()->Show();
  return g_crostini_recovery_view->can_launch_apps_;
}

base::string16 CrostiniRecoveryView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CROSTINI_RECOVERY_TITLE);
}

bool CrostiniRecoveryView::ShouldShowCloseButton() const {
  return false;
}

bool CrostiniRecoveryView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  // Buttons are disabled after Accept or Cancel have been clicked.
  return closed_reason_ == views::Widget::ClosedReason::kUnspecified;
}

gfx::Size CrostiniRecoveryView::CalculatePreferredSize() const {
  const int dialog_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH) -
                           margins().width();
  return gfx::Size(dialog_width, GetHeightForWidth(dialog_width));
}

bool CrostiniRecoveryView::Accept() {
  closed_reason_ = views::Widget::ClosedReason::kAcceptButtonClicked;
  if (can_launch_apps_) {
    return true;
  }
  crostini::CrostiniManager::GetForProfile(profile_)->StopVm(
      crostini::kCrostiniDefaultVmName,
      base::BindOnce(
          [](base::WeakPtr<CrostiniRecoveryView> weak_this,
             crostini::CrostiniResult result) {
            if (weak_this) {
              weak_this->ScheduleAppLaunch();
            }
          },
          weak_ptr_factory_.GetWeakPtr()));
  DialogModelChanged();
  return false;
}

void CrostiniRecoveryView::ScheduleAppLaunch() {
  VLOG(1) << "Scheduling app launch " << app_id_;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&CrostiniRecoveryView::CompleteAppLaunch,
                                weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniRecoveryView::CompleteAppLaunch() {
  can_launch_apps_ = true;
  if (!app_id_.empty()) {
    crostini::LaunchCrostiniApp(profile_, app_id_, display_id_, {},
                                std::move(callback_));
  }
  GetWidget()->CloseWithReason(closed_reason_);
}

bool CrostiniRecoveryView::Cancel() {
  closed_reason_ = views::Widget::ClosedReason::kCancelButtonClicked;
  if (can_launch_apps_) {
    return true;
  }
  app_id_ = crostini::GetTerminalId();
  ScheduleAppLaunch();
  DialogModelChanged();
  return false;
}

// static
CrostiniRecoveryView* CrostiniRecoveryView::GetActiveViewForTesting() {
  return g_crostini_recovery_view;
}

CrostiniRecoveryView::CrostiniRecoveryView(
    Profile* profile,
    const std::string& app_id,
    int64_t display_id,
    crostini::LaunchCrostiniAppCallback callback)
    : profile_(profile),
      app_id_(app_id),
      display_id_(display_id),
      callback_(std::move(callback)),
      weak_ptr_factory_(this) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  DialogDelegate::SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_CROSTINI_RECOVERY_RESTART_BUTTON));
  DialogDelegate::SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_CROSTINI_RECOVERY_TERMINAL_BUTTON));

  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  const base::string16 message =
      l10n_util::GetStringUTF16(IDS_CROSTINI_RECOVERY_MESSAGE);
  views::Label* message_label = new views::Label(message);
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label);

  chrome::RecordDialogCreation(chrome::DialogIdentifier::CROSTINI_RECOVERY);
}

CrostiniRecoveryView::~CrostiniRecoveryView() {
  g_crostini_recovery_view = nullptr;
}
