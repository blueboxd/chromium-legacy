// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_HATS_HATS_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_HATS_HATS_DIALOG_H_

#include <string>

#include "base/macros.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class Profile;

namespace chromeos {

// Happiness tracking survey dialog. Sometimes appears after login to ask the
// user how satisfied they are with their Chromebook.
// This class lives on the UI thread and is self deleting.
class HatsDialog : public ui::WebDialogDelegate {
 public:
  // Creates an instance of HatsDialog and posts a task to load all the relevant
  // device info before displaying the dialog.
  static void CreateAndShow(bool is_google_account);

 private:
  static void Show(bool is_google_account, const std::string& site_context);

  // Use CreateAndShow() above.
  explicit HatsDialog(const std::string& html_data, Profile* otr_profile);
  ~HatsDialog() override;

  // ui::WebDialogDelegate implementation.
  ui::ModalType GetDialogModalType() const override;
  base::string16 GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  bool CanResizeDialog() const override;
  std::string GetDialogArgs() const override;
  // NOTE: This function deletes this object at the end.
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;

  const std::string html_data_;
  Profile* otr_profile_;

  DISALLOW_COPY_AND_ASSIGN(HatsDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_HATS_HATS_DIALOG_H_
