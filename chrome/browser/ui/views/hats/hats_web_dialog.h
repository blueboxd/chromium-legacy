// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HATS_HATS_WEB_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_HATS_HATS_WEB_DIALOG_H_

#include <string>

#include "base/macros.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class Browser;

// Happiness tracking survey dialog which shows the survey content.
// This class lives on the UI thread and is self deleting.
// TODO(weili): This dialog shares a lot of common code with the one in
// chrome/browser/chromeos/hats/, should be merged into one.
class HatsWebDialog : public ui::WebDialogDelegate {
 public:
  // Create and show an instance of HatsWebDialog.
  static void Show(const Browser* browser);

 private:
  // Use Show() above. |site_id| is used to select the survey.
  explicit HatsWebDialog(const std::string& site_id);
  ~HatsWebDialog() override;

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

  const std::string site_id_;

  DISALLOW_COPY_AND_ASSIGN(HatsWebDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HATS_HATS_WEB_DIALOG_H_
