// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_UI_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision.mojom.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_handler.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/label.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "url/gurl.h"

namespace chromeos {

// Dialog which displays the add-supervision flow which allows users to
// convert a regular Google account into a Family-Link managed account.
class AddSupervisionDialog : public SystemWebDialogDelegate {
 public:
  // Shows the dialog; if the dialog is already displayed, this function is a
  // no-op.
  static void Show(gfx::NativeView parent);

  // Closes the dialog, if the dialog doesn't exist, this function is a
  // no-op.
  static void Close();

  // ui::WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override;
  void GetDialogSize(gfx::Size* size) const override;
  bool OnDialogCloseRequested() override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;

 protected:
  AddSupervisionDialog();
  ~AddSupervisionDialog() override;

 private:
  static SystemWebDialogDelegate* GetInstance();

  DISALLOW_COPY_AND_ASSIGN(AddSupervisionDialog);
};

// Controller for chrome://add-supervision
class AddSupervisionUI : public ui::MojoWebUIController,
                         public AddSupervisionHandler::Delegate {
 public:
  explicit AddSupervisionUI(content::WebUI* web_ui);
  ~AddSupervisionUI() override;

  // AddSupervisionHandler::Delegate:
  bool CloseDialog() override;

 private:
  void BindAddSupervisionHandler(
      add_supervision::mojom::AddSupervisionHandlerRequest request);
  void SetupResources();

  std::unique_ptr<add_supervision::mojom::AddSupervisionHandler>
      mojo_api_handler_;

  GURL supervision_url_;

  DISALLOW_COPY_AND_ASSIGN(AddSupervisionUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_UI_H_
