// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_dialog.h"

#include "chrome/browser/chromeos/crostini/crostini_manager.h"
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

void CrostiniUpgraderDialog::Show(Profile* profile,
                                  base::OnceClosure launch_closure) {
  auto* instance = SystemWebDialogDelegate::FindInstance(GetUrl().spec());
  if (instance) {
    instance->Focus();
    return;
  }

  instance = new CrostiniUpgraderDialog(std::move(launch_closure));
  instance->ShowSystemDialog();
}

CrostiniUpgraderDialog::CrostiniUpgraderDialog(base::OnceClosure launch_closure)
    : SystemWebDialogDelegate{GetUrl(), /*title=*/{}},
      launch_closure_{std::move(launch_closure)} {}

CrostiniUpgraderDialog::~CrostiniUpgraderDialog() = default;

void CrostiniUpgraderDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(::kDialogWidth, ::kDialogHeight);
}

bool CrostiniUpgraderDialog::ShouldShowCloseButton() const {
  return false;
}

void CrostiniUpgraderDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->z_order = ui::ZOrderLevel::kNormal;
}

bool CrostiniUpgraderDialog::CanCloseDialog() const {
  // TODO(929571): If other WebUI Dialogs also need to let the WebUI control
  // closing logic, we should find a more general solution.

  // Disallow closing without WebUI consent.
  return upgrader_ui_ == nullptr || upgrader_ui_->can_close();
}

void CrostiniUpgraderDialog::OnDialogShown(content::WebUI* webui) {
  upgrader_ui_ = static_cast<CrostiniUpgraderUI*>(webui->GetController());
  upgrader_ui_->set_launch_closure(std::move(launch_closure_));
  return SystemWebDialogDelegate::OnDialogShown(webui);
}

void CrostiniUpgraderDialog::OnCloseContents(content::WebContents* source,
                                             bool* out_close_dialog) {
  upgrader_ui_ = nullptr;
  return SystemWebDialogDelegate::OnCloseContents(source, out_close_dialog);
}

}  // namespace chromeos
