// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_dialog.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_simple_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_ui.h"
#include "chrome/common/webui_url_constants.h"

namespace {
// The dialog content area size. Note that the height is less than the design
// spec to compensate for title bar height.
constexpr int kDialogWidth = 768;
constexpr int kDialogHeight = 608;

GURL GetUrl() {
  return GURL{chrome::kChromeUICrostiniUpgraderUrl};
}
}  // namespace

namespace chromeos {

void CrostiniUpgraderDialog::Show(base::OnceClosure launch_closure,
                                  bool only_run_launch_closure_on_restart) {
  auto* instance = SystemWebDialogDelegate::FindInstance(GetUrl().spec());
  if (instance) {
    instance->Focus();
    return;
  }

  instance = new CrostiniUpgraderDialog(std::move(launch_closure),
                                        only_run_launch_closure_on_restart);
  instance->ShowSystemDialog();
  base::UmaHistogramEnumeration(crostini::kUpgradeDialogEventHistogram,
                                crostini::UpgradeDialogEvent::kDialogShown);
}

CrostiniUpgraderDialog::CrostiniUpgraderDialog(
    base::OnceClosure launch_closure,
    bool only_run_launch_closure_on_restart)
    : SystemWebDialogDelegate{GetUrl(), /*title=*/{}},
      only_run_launch_closure_on_restart_(only_run_launch_closure_on_restart),
      launch_closure_{std::move(launch_closure)} {}

CrostiniUpgraderDialog::~CrostiniUpgraderDialog() {
  if (deletion_closure_for_testing_) {
    std::move(deletion_closure_for_testing_).Run();
  }
}

void CrostiniUpgraderDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(::kDialogWidth, ::kDialogHeight);
}

bool CrostiniUpgraderDialog::ShouldShowCloseButton() const {
  return false;
}

bool CrostiniUpgraderDialog::ShouldCloseDialogOnEscape() const {
  return false;
}

void CrostiniUpgraderDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->z_order = ui::ZOrderLevel::kNormal;
}

void CrostiniUpgraderDialog::SetDeletionClosureForTesting(
    base::OnceClosure deletion_closure_for_testing) {
  deletion_closure_for_testing_ = std::move(deletion_closure_for_testing);
}

bool CrostiniUpgraderDialog::CanCloseDialog() const {
  // TODO(929571): If other WebUI Dialogs also need to let the WebUI control
  // closing logic, we should find a more general solution.

  if (deletion_closure_for_testing_) {
    // Running in a test.
    return true;
  }
  // Disallow closing without WebUI consent.
  return upgrader_ui_ == nullptr || upgrader_ui_->can_close();
}

namespace {
void RunLaunchClosure(base::WeakPtr<crostini::CrostiniManager> crostini_manager,
                      base::OnceClosure launch_closure,
                      bool only_run_launch_closure_on_restart,
                      bool restart_required) {
  if (!crostini_manager) {
    return;
  }
  if (!restart_required) {
    if (!only_run_launch_closure_on_restart) {
      std::move(launch_closure).Run();
    }
    return;
  }
  crostini_manager->RestartCrostini(
      crostini::kCrostiniDefaultVmName, crostini::kCrostiniDefaultContainerName,
      base::BindOnce(
          [](base::OnceClosure launch_closure,
             crostini::CrostiniResult result) {
            if (result != crostini::CrostiniResult::SUCCESS) {
              LOG(ERROR)
                  << "Failed to restart crostini after upgrade. Error code: "
                  << static_cast<int>(result);
              return;
            }
            std::move(launch_closure).Run();
          },
          std::move(launch_closure)));
}
}  // namespace

void CrostiniUpgraderDialog::OnDialogShown(content::WebUI* webui) {
  auto* crostini_manager =
      crostini::CrostiniManager::GetForProfile(Profile::FromWebUI(webui));
  crostini_manager->SetCrostiniDialogStatus(crostini::DialogType::UPGRADER,
                                            true);
  crostini_manager->UpgradePromptShown(
      crostini::ContainerId(crostini::kCrostiniDefaultVmName,
                            crostini::kCrostiniDefaultContainerName));

  upgrader_ui_ = static_cast<CrostiniUpgraderUI*>(webui->GetController());
  upgrader_ui_->set_launch_callback(base::BindOnce(
      &RunLaunchClosure, crostini_manager->GetWeakPtr(),
      std::move(launch_closure_), only_run_launch_closure_on_restart_));
  return SystemWebDialogDelegate::OnDialogShown(webui);
}

void CrostiniUpgraderDialog::OnCloseContents(content::WebContents* source,
                                             bool* out_close_dialog) {
  upgrader_ui_ = nullptr;
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(
      Profile::FromBrowserContext(source->GetBrowserContext()));
  crostini_manager->SetCrostiniDialogStatus(crostini::DialogType::UPGRADER,
                                            false);
  return SystemWebDialogDelegate::OnCloseContents(source, out_close_dialog);
}

}  // namespace chromeos
